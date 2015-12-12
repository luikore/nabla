#include <stdlib.h>
#include <memory.h>
#include "utils/backtrace.h"
#include "map.h"
#include "map-node.h"
#include "map-cola.h"

// immutable implementation of Bagwell's HAMT
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf

// Differences:
// - nodes are allocated by reference counting GC, when ref_count == 1, node can be updated in-place
// - node can be used as hash-array-mapped node or collision resolution array, depends on the depth

// don't need CTrie, because there are no mutable methods

// like a node, but inlines root slots
typedef struct {
  ValHeader header; // klass = KLASS_MAP, flags=root_size, user1=is_int_valued
  size_t size;
  uint64_t bitmap;
  Slot slots[];
} Map;

static Val empty_map;
static Val empty_map_i; // int valued

#pragma mark ### map helpers

#define MAP_ROOT_SIZE(_m_) (_m_)->header.flags

#define MAP_IS_INT_VALUED(_m_) (_m_)->header.user1

#define MAP_BM_TEST_POS(_map_, _pos_) (((_map_)->bitmap >> _pos_) & 1)

static Map* MAP_ALLOC(size_t root_size) {
  Map* m = val_alloc(KLASS_MAP, sizeof(Map) + sizeof(Slot) * root_size);
  MAP_ROOT_SIZE(m) = root_size;
  return m;
}

static void MAP_DESTROY(void* p) {
  Map* map = p;
  int size = MAP_ROOT_SIZE(map);
  for (int i = 0; i < size; i++) {
    Slot* slot = map->slots + i;
    SLOT_RELEASE(slot, MAP_IS_INT_VALUED(map));
  }
}

// similar to NODE_FIND_SLOT
// return NULL if not found
static Slot* MAP_FIND_SLOT(Map* h, uint64_t hash) {
  // try to find in root
  int pos = GET_POS(hash, 0);
  uint64_t flag = GET_FLAG(pos);
  size_t index = BM_INDEX(h->bitmap, pos);
  if (!MAP_BM_TEST_POS(h, pos)) {
    return NULL;
  }
  return h->slots + index;
}

static Map* MAP_DUP(Map* h) {
  size_t bytesize = sizeof(Map) + sizeof(Slot) * MAP_ROOT_SIZE(h);
  Map* m = val_dup(h, bytesize, bytesize);
  for (int i = 0; i < MAP_ROOT_SIZE(m); i++) {
    SLOT_RETAIN(m->slots + i, MAP_IS_INT_VALUED(m));
  }
  return m;
}

// prereq: k not in map
// the newly inserted slot is set to VAL_UNDEF
static Map* MAP_INSERT(Map* h, uint64_t hash, Val k, Val v) {
  int pos = GET_POS(hash, 0);
  assert(!MAP_BM_TEST_POS(h, pos));

  Map* new_map = MAP_ALLOC(MAP_ROOT_SIZE(h) + 1);
  MAP_IS_INT_VALUED(new_map) = MAP_IS_INT_VALUED(h);
  new_map->size = h->size + 1; // NOTE: total size
  new_map->bitmap = h->bitmap;

  size_t index = BM_INDEX(new_map->bitmap, pos);
  uint64_t flag = GET_FLAG(pos);
  BM_SET_FLAG(&new_map->bitmap, flag, true);

  for (int i = 0, j = 0; i < MAP_ROOT_SIZE(h); i++) {
    if (i == index) {
      new_map->slots[j].kv.k = VAL_UNDEF;
      new_map->slots[j].kv.v = VAL_UNDEF;
      j++;
    }
    SLOT_RETAIN(h->slots + i, MAP_IS_INT_VALUED(h));
    new_map->slots[j++] = h->slots[i];
  }

  return new_map;
}

// prereq: k in map
static Map* MAP_REMOVE(Map* h, uint64_t hash) {
  int pos = GET_POS(hash, 0);
  assert(MAP_BM_TEST_POS(h, pos));

  Map* new_map = MAP_ALLOC(MAP_ROOT_SIZE(h) - 1);
  MAP_IS_INT_VALUED(new_map) = MAP_IS_INT_VALUED(h);
  new_map->size = h->size - 1; // NOTE: total size
  new_map->bitmap = h->bitmap;

  size_t index = BM_INDEX(new_map->bitmap, pos);
  uint64_t flag = GET_FLAG(pos);
  BM_SET_FLAG(&h->bitmap, flag, false);

  for (int i = 0, j = 0; i < MAP_ROOT_SIZE(h); i++) {
    if (i != index) {
      SLOT_RETAIN(h->slots + i, MAP_IS_INT_VALUED(h));
      new_map->slots[j++] = h->slots[i];
    }
  }

  return new_map;
}

#pragma mark ### helpers decl

static bool _deep_find(Slot* slot, uint64_t hash, Val k, Val* v, bool is_int_valued);
static bool _deep_insert(Slot* slot, uint64_t hash, Val k, Val v, int level, bool is_int_valued);
static bool _deep_remove(Slot* slot, uint64_t hash, Val k, Val* v, bool is_int_valued);
static NbMapEachRet _slot_each(Slot* slot, Val udata, NbMapEachCb cb);
static void _debug(Slot* slot, int level);

#pragma mark ### interface impl

void nb_map_init_module() {
  // perm empty map
  Map* map = MAP_ALLOC(0);
  val_perm(map);
  map->size = 0;
  map->bitmap = 0;
  empty_map = (Val)map;

  // perm empty map of int value
  map = MAP_ALLOC(0);
  val_perm(map);
  map->size = 0;
  map->bitmap = 0;
  MAP_IS_INT_VALUED(map) = 1;
  empty_map_i = (Val)map;

  // destructor func
  klass_def_internal(KLASS_MAP, val_strlit_new_c("Map"));
  klass_set_destruct_func(KLASS_MAP, MAP_DESTROY);
  klass_def_internal(KLASS_MAP_NODE, val_strlit_new_c("MapNode"));
  klass_set_destruct_func(KLASS_MAP_NODE, NODE_DESTROY);
  klass_def_internal(KLASS_MAP_COLA, val_strlit_new_c("MapCola"));
  klass_set_destruct_func(KLASS_MAP_COLA, COLA_DESTROY);
}

Val nb_map_new() {
  return empty_map;
}

Val nb_map_new_i() {
  return empty_map_i;
}

size_t nb_map_size(Val vm) {
  Map* m = (Map*)vm;
  return m->size;
}

Val nb_map_find(Val vh, Val k) {
  Map* h = (Map*)vh;
  if (h->size == 0) {
    return VAL_UNDEF;
  }
  uint64_t hash = val_hash(k);

  Slot* slot = MAP_FIND_SLOT(h, hash);
  if (!slot) {
    return VAL_UNDEF;
  }

  Val v;
  if (_deep_find(slot, hash, k, &v, MAP_IS_INT_VALUED(h))) {
    return v;
  } else {
    return VAL_UNDEF;
  }
}

Val nb_map_insert(Val vh, Val k, Val v) {
  Map* h = (Map*)vh;
  uint64_t hash = val_hash(k);
  Map* new_map;

  // there is a minor chance that v is the same as a member in map,
  // in that case we can save log(n) allocs.
  // but the impl is complex and requires redundant loop first,
  // we can leave the optimization to the transient api.

  Slot* slot = MAP_FIND_SLOT(h, hash);
  if (slot) {
    new_map = MAP_DUP(h);
    slot = MAP_FIND_SLOT(new_map, hash);
    bool added = _deep_insert(slot, hash, k, v, W, MAP_IS_INT_VALUED(h));
    if (added) {
      new_map->size++;
    }
  } else {
    new_map = MAP_INSERT(h, hash, k, v);
    slot = MAP_FIND_SLOT(new_map, hash);
    slot->kv.k = k;
    slot->kv.v = v;
    KV_RETAIN(slot->kv, MAP_IS_INT_VALUED(new_map));
  }

  return (Val)new_map;
}

Val nb_map_remove(Val vh, Val k, Val* v) {
  Map* h = (Map*)vh;
  uint64_t hash = val_hash(k);

  Slot* found = MAP_FIND_SLOT(h, hash);
  if (found) {
    Slot slot = *found;
    if (_deep_remove(&slot, hash, k, v, MAP_IS_INT_VALUED(h))) {
      Map* new_map;
      if (slot.h == VAL_UNDEF) {
        new_map = MAP_REMOVE(h, hash);
      } else {
        new_map = MAP_DUP(h);
        new_map->size--;
      }
      return (Val)new_map;
    }
  }

  RETAIN(vh);
  return VAL_UNDEF;
}

void nb_map_debug(Val vh) {
  Map* h = (Map*)vh;
  printf("<map size=%lu root_size=%u is_int_valued=%d bitmap=0x%llx>\n",
  h->size, MAP_ROOT_SIZE(h), (int)MAP_IS_INT_VALUED(h), h->bitmap);
  for (int i = 0; i < MAP_ROOT_SIZE(h); i++) {
    Slot* slot = h->slots + i;
    if (SLOT_IS_NODE(slot)) {
      _debug(slot, W);
    } else {
      Kv kv = slot->kv;
      printf("  <kv k=%lu v=%lu>\n", kv.k, kv.v);
    }
  }
}

NbMapEachRet nb_map_each(Val m, Val udata, NbMapEachCb cb) {
  assert(cb);
  Map* map = (Map*)m;

  for (int i = 0; i < MAP_ROOT_SIZE(map); i++) {
    NbMapEachRet ret = _slot_each(map->slots + i, udata, cb);
    if (ret != NB_MAP_NEXT) {
      return ret;
    }
  }
  return NB_MAP_FIN;
}

#pragma mark ### helpers impl

// return true if found
static bool _deep_find(Slot* slot, uint64_t hash, Val k, Val* v, bool is_int_valued) {
  if (SLOT_IS_NODE(slot)) {
    Slot* child = NODE_FIND_SLOT((Node*)slot->h, slot->nb.b, hash);
    if (child) {
      return _deep_find(child, hash, k, v, is_int_valued);
    }
  } else if (SLOT_IS_COLA(slot)) {
    return COLA_FIND((Cola*)slot->h, k, v);
  } else {
    if (val_eq(slot->kv.k, k)) {
      *v = slot->kv.v;
      if (!is_int_valued) {
        RETAIN(*v);
      }
      return true;
    }
  }
  return false;
}

// return true if inserted
static bool _deep_insert(Slot* slot, uint64_t hash, Val k, Val v, int level, bool is_int_valued) {
  if (slot->h == VAL_UNDEF) { // newly added slot
    // NOTE we need to check it first because SLOT_IS_NODE requires VAL_KLASS but VAL_UNDEF has no class
    RETAIN(k);
    if (!is_int_valued) {
      RETAIN(v);
    }
    slot->kv.k = k;
    slot->kv.v = v;
    return true;

  } else if (SLOT_IS_NODE(slot)) { // insert node
    Slot* found = NODE_FIND_SLOT(slot->nb.n, slot->nb.b, hash);
    Node* new_node;
    Val old_node = slot->h;
    if (found) {
      slot->nb.n = NODE_DUP(slot->nb.n);
    } else {
      slot->nb = NODE_BM_INSERT(slot->nb, hash);
    }
    RELEASE(old_node);
    Slot* new_slot = NODE_FIND_SLOT(slot->nb.n, slot->nb.b, hash);
    return _deep_insert(new_slot, hash, k, v, level + W, is_int_valued);

  } else if (SLOT_IS_COLA(slot)) { // insert cola
    bool size_increased;
    Cola* cola = COLA_INSERT((Cola*)slot->h, k, v, &size_increased);
    RELEASE(slot->h);
    Slot res = {.h = (Val)cola};
    *slot = res;
    return size_increased;

  } else { // replace kv
    if (val_eq(slot->kv.k, k)) { // in-place update
      if (!is_int_valued) {
        RETAIN(v);
        RELEASE(slot->kv.v); // NOTE different than _deep_remove, the slot here was retained before
      }
      slot->kv.v = v;
      return false;

    } if (level == MAX_NODE_LEVEL) { // slot->kv.k != k and last level, upgrade to cola
      Val new_cola = (Val)COLA_NEW2(slot->kv, k, v, is_int_valued);
      KV_RELEASE(slot->kv, is_int_valued);
      slot->kv.v = VAL_UNDEF;
      slot->h = new_cola;
      return true;

    } else { // slot->kv.k != k and not last level, upgrade to node
      if (!COLLIDE(val_hash(slot->kv.k), hash, level)) {
        NodeBm nb = NODE_NEW2(slot->kv, hash, k, v, level, is_int_valued);
        KV_RELEASE(slot->kv, is_int_valued);
        slot->nb = nb;
        return true;

      } else { // replace kv in next level
        Slot child_slot = *slot;
        bool res = _deep_insert(&child_slot, hash, k, v, level + W, is_int_valued);
        KV_RELEASE(slot->kv, is_int_valued);
        slot->nb = NODE_WRAP_SLOT(child_slot, hash, level, is_int_valued);
        return res;
      }
    }
  }
}

// return true if element is less
// NOTE do not release slot if replaced
// NOTE we don't need to retain node when not found, in this case only top level map requir retain
static bool _deep_remove(Slot* slot, uint64_t hash, Val k, Val* v, bool is_int_valued) {
  if (SLOT_IS_NODE(slot)) {
    Slot* found = NODE_FIND_SLOT(slot->nb.n, slot->nb.b, hash);
    if (found) {
      Slot remove_res = *found;
      if (_deep_remove(&remove_res, hash, k, v, is_int_valued)) { // removed in child
        if (remove_res.h == VAL_UNDEF) {
          slot->nb = NODE_BM_REMOVE(slot->nb, hash);
        } else {
          Node* new_node = NODE_DUP_REPLACE(slot->nb.n, slot->nb.b, hash, remove_res);
          slot->nb.n = new_node;
          // same bitmap
        }
        return true;
      }
    }
    return false;

  } else if (SLOT_IS_COLA(slot)) {
    bool size_decreased;
    Slot res = COLA_REMOVE((Cola*)slot->h, k, v, &size_decreased);
    RELEASE(slot->h);
    *slot = res;
    return size_decreased;

  } else { // kv
    if (val_eq(slot->kv.k, k)) {
      *v = slot->kv.v;
      if (!is_int_valued) {
        RETAIN(*v);
      }
      slot->kv.k = VAL_UNDEF;
      slot->kv.v = VAL_UNDEF;
      return true;
    }
  }

  return false;
}

static NbMapEachRet _slot_each(Slot* slot, Val udata, NbMapEachCb cb) {
  if (SLOT_IS_NODE(slot)) {
    Node* n = (Node*)slot->h;
    for (int i = 0; i < SIZE(n); i++) {
      NbMapEachRet ret = _slot_each(n->slots + i, udata, cb);
      if (ret != NB_MAP_NEXT) {
        return ret;
      }
    }

  } else if (SLOT_IS_COLA(slot)) {
    Cola* c = (Cola*)slot->h;
    for (int i = 0; i < SIZE(c); i++) {
      NbMapEachRet ret = cb(c->kvs[i].k, c->kvs[i].v, udata);
      if (ret != NB_MAP_NEXT) {
        return ret;
      }
    }

  } else {
    NbMapEachRet ret = cb(slot->kv.k, slot->kv.v, udata);
    if (ret != NB_MAP_NEXT) {
      return ret;
    }
  }

  return NB_MAP_NEXT;
}

static void _debug(Slot* slot, int level) {
  int indent = level/W*2;
  
  if (SLOT_IS_COLA(slot)) {
    Cola* c = (Cola*)slot->h;
    printf("%*s<cola#%p rc=%d level=%d size=%d>\n",
    indent, "", c, (int)VAL_REF_COUNT(slot->h), (int)LEVEL(c), (int)SIZE(c));
    for (int i = 0; i < SIZE(c); i++) {
      _debug((Slot*)(c->kvs + i), level + W);
    }

  } else if (SLOT_IS_NODE(slot)) {
    Node* n = slot->nb.n;
    printf("%*s<node#%p rc=%d level=%d size=%u bitmap=0x%llx>\n",
    indent, "", n, (int)VAL_REF_COUNT(slot->h), (int)LEVEL(n), (int)SIZE(n), slot->nb.b);
    for (int i = 0; i < SIZE(n); i++) {
      _debug((Slot*)(n->slots + i), level + W);
    }

  } else {
    Kv kv = slot->kv;
    printf("%*s<kv k=%lu v=%lu>\n",
    indent, "", kv.k, kv.v);
  }
}
