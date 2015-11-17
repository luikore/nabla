#include <stdlib.h>
#include <memory.h>
#include "utils/backtrace.h"
#include "map.h"

// immutable implementation of Bagwell's HAMT
// http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf

// Differences:
// - nodes are allocated by reference counting GC, when ref_count == 1, node can be updated in-place
// - node can be used as hash-array-mapped node or collision resolution array, depends on the depth

// don't need CTrie, because there are no mutable methods

#define W 6
#define W_MASK ((1ULL << W) - 1)
#define MAX_NODE_LEVEL ((1ULL << W) / W * W - W)
#define IS_COLLISION_LEVEL(_level_) ((_level_) == MAX_NODE_LEVEL)

inline static int GET_POS(uint64_t hash, int level) {
  return ((hash >> level) & W_MASK);
}

inline static uint64_t GET_FLAG(int pos) {
  return (1ULL << (pos));
}

inline static bool COLLIDE(uint64_t h1, uint64_t h2, int level) {
  return GET_POS(h1, level) == GET_POS(h2, level);
}

#define BM_TEST_POS(_bm_, _pos_) ((_bm_ >> _pos_) & 1)
inline static void BM_SET_FLAG(uint64_t* bm, uint64_t flag, bool set) {
  if (set) {
    *bm |= flag;
  } else {
    *bm &= ~flag;
  }
}
#define BM_INDEX(_bm_, _pos_) PDLEX_POPCNT(_bm_ & ((1ULL << (_pos_)) - 1))

static uint32_t KLASS_MAP_SLAB;
static Val empty_map;
static Val empty_map_i;

// 0 or positive: downward with index
enum { NOT_FOUND=-1, FOUND_KV=-2 };

// struct size of Kv and Node are the same, so don't inline the contained array
//
// allocations only happen on slab-like nodes

union SlotUnion;
typedef union SlotUnion Slot;

struct MapStruct;
typedef struct MapStruct Map;

struct SlabStruct;
typedef struct SlabStruct Slab;

typedef struct {
  Val k;
  Val v;
} Kv;

typedef struct {
  Slab* slab;  // same position as Kv.k, so we can use VAL_KLASS(((Kv*)slot)->k) to determine if the slot is node
               // this field is non-NULL, so we can have less code for edge-cases
  uint64_t bitmap;
} Node;

union SlotUnion {
  Kv kv;
  Node node;
};

struct SlabStruct {
  ValHeader header; // klass = KLASS_MAP_SLAB, flags = slots size
  Slot slots[];     // for intermediate nodes: 0...2**W (can be 0 or 1 elem due to remove actions)
                    // for collision nodes: 0...inf
};

// like a slab, mostly
struct MapStruct {
  ValHeader header; // klass = KLASS_MAP, flags = slots size
  uint64_t is_int_valued;
  size_t size;
  uint64_t bitmap;
  Slot slots[];
};

struct IterNodeStruct;
typedef struct IterNodeStruct IterNode;
struct IterNodeStruct {
  Node* node;
  uint64_t i;
};

#define SLAB_SIZE(_slab_) (_slab_)->header.flags

inline static Slab* SLAB_ALLOC(size_t size) {
  Slab* slab = val_alloc(sizeof(Slab) + sizeof(Kv) * size);
  slab->header.klass = KLASS_MAP_SLAB;
  SLAB_SIZE(slab) = size;
  return slab;
}

#define SLOT_IS_NODE(_slot_) (VAL_KLASS(((Kv*)(_slot_))->k) == KLASS_MAP_SLAB)

inline static void SLOT_RETAIN(Slot* slot, bool is_int_valued) {
  if (SLOT_IS_NODE(slot)) {
    RETAIN(slot->node.slab);
  } else if (is_int_valued) {
    RETAIN(slot->kv.k);
  } else {
    RETAIN(slot->kv.k);
    RETAIN(slot->kv.v);
  }
}

inline static void SLOT_RELEASE(Slot* slot, bool is_int_valued) {
  if (SLOT_IS_NODE(slot)) {
    Node* node = &slot->node;
    Slab* slab = node->slab;
    if (VAL_IS_PERM(slab)) {
      return;
    }
    if (slab->header.ref_count > 1) {
      slab->header.ref_count--;
      return;
    }
    assert(slab->header.ref_count == 1);
    for (int i = 0; i < SLAB_SIZE(slab); i++) {
      SLOT_RELEASE(slab->slots + i, is_int_valued);
    }
    val_free(slab);
  } else if (is_int_valued) {
    RELEASE(slot->kv.k);
  } else {
    RELEASE(slot->kv.k);
    RELEASE(slot->kv.v);
  }
}

// from kv to node
// NOTE level for the upgraded child node
inline static void SLOT_UPGRADE(Slot* slot, int level) {
  assert(!SLOT_IS_NODE(slot));

  Val k = slot->kv.k;
  Slab* slab = SLAB_ALLOC(1);
  slab->slots[0].kv = slot->kv;
  slot->node.slab = slab;

  if (IS_COLLISION_LEVEL(level)) {
    slot->node.bitmap = 0;
  } else {
    slot->node.bitmap = GET_FLAG(GET_POS(val_hash(k), level));
  }
}

inline static void SLOTS_CPY(Slot* dst, int src_size, Slot* src, bool is_int_valued) {
  for (int i = 0; i < src_size; i++) {
    dst[i] = src[i];
    SLOT_RETAIN(dst + i, is_int_valued);
  }
}

inline static void SLOTS_CPY_INSERT(Slot* dst, int src_size, Slot* src, int index, Kv kv, bool is_int_valued) {
  int i = 0;
  for (int j = 0; j <= src_size; j++) {
    if (j == index) {
      dst[j].kv = kv;
    } else {
      Slot* slot = src + (i++);
      dst[j] = *slot;
    }
    SLOT_RETAIN(dst + j, is_int_valued);
  }
}

inline static Kv SLOTS_CPY_REMOVE(Slot* dst, int src_size, Slot* src, int index, bool is_int_valued) {
  int j = 0;
  Kv kv;
  for (int i = 0; i < src_size; i++) {
    Slot* slot = src + i;
    if (i == index) {
      kv = slot->kv;
      continue;
    }
    dst[j++] = *slot;
    SLOT_RETAIN(slot, is_int_valued);
  }
  return kv;
}

// return FOUND_KV/NOT_FOUND
// NOTE level is for node-child
static int SLOT_FIND(Slot* slot, uint64_t hash, Val k, Val* v, int level) {
  if (SLOT_IS_NODE(slot)) {
    Node* node = &slot->node;
    if (IS_COLLISION_LEVEL(level)) {
      Slab* slab = node->slab;
      for (int i = 0; i < SLAB_SIZE(slab); i++) {
        Slot* slot = slab->slots + i;
        if (VAL_EQ(slot->kv.k, k)) {
          return i;
          // NOTE: why not return FOUND_KV? because:
          //       - unified node traversing for the use in insert/remove
          //       - just 1 more VAL_EQ check when found, but since collision is very rare
        }
      }
      return NOT_FOUND;
    } else {
      int pos = GET_POS(hash, level);
      if (BM_TEST_POS(node->bitmap, pos)) {
        return BM_INDEX(node->bitmap, pos);
      } else {
        return NOT_FOUND;
      }
    }
  } else {
    if (VAL_EQ(slot->kv.k, k)) {
      *v = slot->kv.v;
      return FOUND_KV;
    } else {
      return NOT_FOUND;
    }
  }
}

static void SLOT_REPLACE_V(Slot* slot, Val v, bool is_int_valued) {
  Kv* kv = &slot->kv;
  if (!is_int_valued) {
    RELEASE(kv->v);
  }
  kv->v = v;
  if (!is_int_valued) {
    RETAIN(v);
  }
}

static void SLOT_EXTEND(Slot* slot, uint64_t hash, Val k, Val v, int level, bool is_int_valued) {
  // not found - kv: upgrade to node first
  // todo optimize upgrade-to-2-slots when memory check tests passed
  if (!SLOT_IS_NODE(slot)) {
    SLOT_UPGRADE(slot, level);
  }

  Node* node = &slot->node;

  int pos = -1;
  int index;
  if (IS_COLLISION_LEVEL(level)) {
    index = SLAB_SIZE(node->slab);
  } else {
    pos = GET_POS(hash, level);
    index = BM_INDEX(node->bitmap, pos);
  }
  assert(index <= SLAB_SIZE(node->slab));

  Slab* oslab = node->slab;
  Slab* nslab = SLAB_ALLOC(SLAB_SIZE(oslab) + 1);
  Kv kv = {.k = k, .v = v};
  SLOTS_CPY_INSERT(nslab->slots, SLAB_SIZE(oslab), oslab->slots, index, kv, is_int_valued);
  node->slab = nslab;

  if (pos >= 0) { // bm
    assert(!BM_TEST_POS(node->bitmap, pos));
    BM_SET_FLAG(&node->bitmap, GET_FLAG(pos), true);
  }
  RELEASE(oslab);
}

#pragma mark ### node helpers

#define NODE_SIZE(_node_) SLAB_SIZE((_node_)->slab)

#define NODE_GET_SLOT(_node_, _i_) ({assert(_i_ < NODE_SIZE(_node_)); (_node_)->slab->slots + _i_;})

static void NODE_DUP_SLAB(Node* node, bool is_int_valued) {
  Slab* oslab = node->slab;
  node->slab = SLAB_ALLOC(SLAB_SIZE(oslab));
  SLOTS_CPY(node->slab->slots, SLAB_SIZE(oslab), oslab->slots, is_int_valued);
  RELEASE(oslab);
}

// dup and remove child for intermediate node
// degrade into Kv if only 1 element left
static void NODE_DUP_REMOVE(Node* node, int pos, int level, bool is_int_valued) {
  assert(BM_TEST_POS(node->bitmap, pos));

  int index = BM_INDEX(node->bitmap, pos);
  Slab* oslab = node->slab;
  if (SLAB_SIZE(oslab) == 2) {
    Slot* left_slot = NODE_GET_SLOT(node, (index == 0 ? 1 : 0));
    *((Kv*)node) = left_slot->kv;
    RELEASE(oslab);
    return;
  }

  assert(SLAB_SIZE(oslab) > 2);
  node->slab = SLAB_ALLOC(SLAB_SIZE(oslab) - 1);
  BM_SET_FLAG(&node->bitmap, pos, false);
  SLOTS_CPY_REMOVE(node->slab->slots, SLAB_SIZE(oslab), oslab->slots, index, is_int_valued);
  RELEASE(oslab);
}

// dup and remove child for collision node
// degrade into Kv if only 1 element left
static void NODE_COLLISION_DUP_REMOVE(Node* node, Val k, bool is_int_valued) {
  Slab* oslab = node->slab;

  int index = -1;
  for (int i = 0; i < SLAB_SIZE(oslab); i++) {
    if (VAL_EQ(k, oslab->slots[i].kv.k)) {
      index = i;
      break;
    }
  }
  assert(index != -1);

  if (SLAB_SIZE(oslab) == 2) {
    Slot* left_slot = NODE_GET_SLOT(node, (index == 0 ? 1 : 0));
    *((Kv*)node) = left_slot->kv;
    RELEASE(oslab);
    return;
  }

  assert(SLAB_SIZE(oslab) > 2);
  node->slab = SLAB_ALLOC(SLAB_SIZE(oslab) - 1);
  SLOTS_CPY_REMOVE(node->slab->slots, SLAB_SIZE(oslab), oslab->slots, index, is_int_valued);
  RELEASE(oslab);
}

// for building node chain
// intermediate nodes have preset ref_count = 1, but leaf kv should be retained outside
inline static Node NODE_OF_SINGLE_SLOT(Val k, Val v, int level) {
  Node node = {
    .bitmap = (IS_COLLISION_LEVEL(level) ? 0 : GET_FLAG(GET_POS(val_hash(k), level))),
    .slab = SLAB_ALLOC(1)
  };
  node.slab->slots[0].kv.k = k;
  node.slab->slots[0].kv.v = v;

  return node;
}

#pragma mark ### map helpers

#define MAP_SIZE(_m_) (_m_)->size

#define MAP_ROOT_SIZE(_m_) (_m_)->header.flags

#define MAP_IS_INT_VALUED(_m_) (_m_)->is_int_valued

#define MAP_BM_TEST_POS(_map_, _pos_) (((_map_)->bitmap >> _pos_) & 1)

inline static Map* MAP_ALLOC(size_t root_size) {
  Map* m = val_alloc(sizeof(Map) + sizeof(Slot) * root_size);
  m->header.klass = KLASS_MAP;
  MAP_ROOT_SIZE(m) = root_size;
  return m;
}

inline static Map* MAP_DUP(Map* h) {
  size_t bytesize = sizeof(Map) + sizeof(Slot) * MAP_ROOT_SIZE(h);
  Map* m = val_dup(h, sizeof(Map), bytesize);
  SLOTS_CPY(m->slots, MAP_ROOT_SIZE(m), h->slots, MAP_IS_INT_VALUED(m));
  return m;
}

inline static Map* MAP_DUP_EXTEND(Map* h, int pos, Val k, Val v) {
  assert(!BM_TEST_POS(h->bitmap, pos));
  uint64_t flag = GET_FLAG(pos);
  size_t index = BM_INDEX(h->bitmap, pos);
  assert(MAP_ROOT_SIZE(h) >= index);

  size_t bytesize = sizeof(Map) + sizeof(Slot) * MAP_ROOT_SIZE(h);
  Map* m = val_alloc(bytesize + sizeof(Slot));
  m->header.klass = KLASS_MAP;
  MAP_SIZE(m) = MAP_SIZE(h) + 1;
  MAP_ROOT_SIZE(m) = MAP_ROOT_SIZE(h) + 1;
  MAP_IS_INT_VALUED(m) = MAP_IS_INT_VALUED(h);

  m->bitmap = h->bitmap;
  BM_SET_FLAG(&m->bitmap, flag, true);

  Kv kv = {.k = k, .v = v};
  SLOTS_CPY_INSERT(m->slots, MAP_ROOT_SIZE(h), h->slots, index, kv, MAP_IS_INT_VALUED(h));
  return m;
}

// return NULL if the slot is not kv
// return h if not found
inline static Map* MAP_TRY_DUP_REMOVE_KV(Map* h, uint64_t hash, Val k, Val* v) {
  int pos = GET_POS(hash, 0);
  if (!BM_TEST_POS(h->bitmap, pos)) {
    return NULL;
  }

  size_t index = BM_INDEX(h->bitmap, pos);
  Slot* slot = h->slots + index;
  if (SLOT_IS_NODE(slot) || !VAL_EQ(k, slot->kv.k)) {
    return NULL;
  }
  *v = slot->kv.v;

  assert(MAP_SIZE(h));
  uint64_t flag = GET_FLAG(pos);
  assert(MAP_ROOT_SIZE(h) > index);

  size_t bytesize = sizeof(Map) + sizeof(Slot) * MAP_ROOT_SIZE(h);
  Map* m = val_alloc(bytesize - sizeof(Slot));
  m->header.klass = KLASS_MAP;
  MAP_SIZE(m) = MAP_SIZE(h) - 1;
  MAP_ROOT_SIZE(m) = MAP_ROOT_SIZE(h) - 1;
  MAP_IS_INT_VALUED(m) = MAP_IS_INT_VALUED(h);

  m->bitmap = h->bitmap;
  BM_SET_FLAG(&m->bitmap, flag, false);

  SLOTS_CPY_REMOVE(m->slots, MAP_ROOT_SIZE(h), h->slots, index, MAP_IS_INT_VALUED(h));
  return m;
}

#pragma mark ### helpers decl

static void _init() __attribute__((constructor(100)));
static void _debug(Slot* slot, int level);
static void _map_destructor(void* map);
static void _check_find();
static void _check_root_insert();

#pragma mark ### interface impl

Val nb_map_new() {
  return empty_map;
}

Val nb_map_new_i() {
  return empty_map_i;
}

size_t nb_map_size(Val vm) {
  Map* m = (Map*)vm;
  return MAP_SIZE(m);
}

Val nb_map_find(Val vh, Val k) {
  Map* h = (Map*)vh;
  if (MAP_SIZE(h) == 0) {
    return VAL_UNDEF;
  }
  uint64_t hash = val_hash(k);

  // try to find in root
  int pos = GET_POS(hash, 0);
  uint64_t flag = GET_FLAG(pos);
  size_t index = BM_INDEX(h->bitmap, pos);
  if (!BM_TEST_POS(h->bitmap, pos)) {
    return VAL_UNDEF;
  }

  Slot* slot = h->slots + index;
  Val v;
  for (int level = W; level <= MAX_NODE_LEVEL; level += W) {
    int i = SLOT_FIND(slot, hash, k, &v, level);
    if (i >= 0) {
      slot = NODE_GET_SLOT(&slot->node, i);
    } else {
      if (!MAP_IS_INT_VALUED(h)) {
        RETAIN(v);
      }
      return v;
    }
  }

  // should not come to here
  assert(false);
  return VAL_UNDEF;
}

Val nb_map_insert(Val vh, Val k, Val v) {
  Map* h = (Map*)vh;
  uint64_t hash = val_hash(k);
  int pos = GET_POS(hash, 0);
  int index = BM_INDEX(h->bitmap, pos);

  // try to insert in root
  if (!BM_TEST_POS(h->bitmap, pos)) {
    return (Val)MAP_DUP_EXTEND(h, pos, k, v);
  }

  Map* map = MAP_DUP(h);
  Slot* slot = map->slots + index;
  if (!SLOT_IS_NODE(slot)) {
    if (VAL_EQ(slot->kv.k, k)) {
      RETAIN(v);
      RELEASE(slot->kv.v);
      slot->kv.v = v;
      return (Val)map;
    } else {
      SLOT_UPGRADE(slot, W);
      // now slot is surely an intermediate node
    }
  }

  for (int level = W; level <= MAX_NODE_LEVEL; level += W) {
    Val vv;
    int index = SLOT_FIND(slot, hash, k, &vv, level);

    if (index >= 0) {
      NODE_DUP_SLAB(&slot->node, MAP_IS_INT_VALUED(map));
      slot = slot->node.slab->slots + index;
    } else if (index == FOUND_KV) {
      SLOT_REPLACE_V(slot, v, MAP_IS_INT_VALUED(map));
      break;
    } else {
      assert(index == NOT_FOUND);
      SLOT_EXTEND(slot, hash, k, v, level, MAP_IS_INT_VALUED(map));
      MAP_SIZE(map)++;
      break;
    }
  }
  return (Val)map;
}

Val nb_map_remove(Val vh, Val k, Val* v) {
  Map* h = (Map*)vh;
  uint64_t hash = val_hash(k);

  // try remove from root
  Map* map = MAP_TRY_DUP_REMOVE_KV(h, hash, k, v);
  if (map == h) {
    *v = VAL_UNDEF;
    RETAIN(h);
    return vh;
  } else if (map) {
    return (Val)map;
  }

  // now there is a found slot in root, and it is a node
  map = MAP_DUP(h);
  int pos = GET_POS(hash, 0);
  int index = BM_INDEX(map->bitmap, pos);

  Slot* parent = map->slots + index;
  NODE_DUP_SLAB(&parent->node, MAP_IS_INT_VALUED(map));

  for (int level = W; level <= MAX_NODE_LEVEL; level += W) {
    int index;

    // hit a node, just dup forward
    index = SLOT_FIND(parent, hash, k, v, level);
    if (index >= 0) {
      Slot* parent = NODE_GET_SLOT(&parent->node, index);
      NODE_DUP_SLAB(&parent->node, MAP_IS_INT_VALUED(map));
      continue;
    }

    // hit a kv, remove it
    if (index == FOUND_KV) {
      RETAIN(*v);
      if (IS_COLLISION_LEVEL(level)) {
        NODE_COLLISION_DUP_REMOVE(&parent->node, k, MAP_IS_INT_VALUED(map));
      } else {
        NODE_DUP_REMOVE(&parent->node, pos, level, MAP_IS_INT_VALUED(map));
      }
      return (Val)map;
    }

    // finally, no match, we release the new map and just return old one
    assert(index == NOT_FOUND);
    *v = VAL_UNDEF;
    RELEASE(map);
    RETAIN(h);
    return vh;
  }

  assert(false);
  RETAIN(h);
  return vh;
}

void nb_map_debug(Val vh) {
  Map* h = (Map*)vh;
  printf("<map size=%lu root_size=%u is_int_valued=%llu bitmap=0x%llx>\n", MAP_SIZE(h), MAP_ROOT_SIZE(h), MAP_IS_INT_VALUED(h), h->bitmap);
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

void nb_map_check_internal_structs() {
  _check_find();
  _check_root_insert();

  // todo check behavior on collision level
}

NbMapIterRet nb_map_iter(Val m, Val udata, NbMapIterCb cb) {
  assert(cb);
  Map* map = (Map*)m;
  if (MAP_SIZE(map) == 0) {
    return PDLEX_MAP_IFIN;
  }

  IterNode iter_nodes[64/W + 1];
  iter_nodes[0].node = NULL;
  iter_nodes[0].i = 0;
  int i = 0;
# define ITER_NODE_SIZE (i ? NODE_SIZE(iter_nodes[i].node) : MAP_ROOT_SIZE(map))
# define ITER_NODE_SLOTS (i ? iter_nodes[i].node->slab->slots : map->slots)

  while (true) {
    // down to leaf
    while (true) {
      Slot* next_slot = ITER_NODE_SLOTS + iter_nodes[i].i;
      if (SLOT_IS_NODE(next_slot)) {
        iter_nodes[i + 1].node = &next_slot->node;
        iter_nodes[i + 1].i = 0;
        i++;
      } else {
        break;
      }
    }

    Kv kv = ITER_NODE_SLOTS[iter_nodes[i].i].kv;
    NbMapIterRet ret = cb(kv.k, kv.v, udata);
    if (ret == PDLEX_MAP_IBREAK) {
      return PDLEX_MAP_IBREAK;
    }

    // carry
    while (iter_nodes[i].i + 1 == ITER_NODE_SIZE) {
      if (i <= 0) {
        return PDLEX_MAP_IFIN;
      }
      i--;
    }
    iter_nodes[i].i++;
  }
  return PDLEX_MAP_IFIN;

# undef ITER_NODE_SIZE
# undef ITER_NODE_SLOTS
}

#pragma mark ### helpers impl

static void _init() {
  // register a klass for slab
  KLASS_MAP_SLAB = val_new_class_id();

  // perm empty map
  Map* map = MAP_ALLOC(0);
  val_perm(map);
  MAP_SIZE(map) = 0;
  map->bitmap = 0;
  empty_map = (Val)map;

  // perm empty map of int value
  map = MAP_ALLOC(0);
  val_perm(map);
  MAP_SIZE(map) = 0;
  map->bitmap = 0;
  MAP_IS_INT_VALUED(map) = 1;
  empty_map_i = (Val)map;

  // destructor func
  val_register_destructor_func(KLASS_MAP, _map_destructor);
}

static void _debug(Slot* slot, int level) {
  Node node = slot->node;
  int sz = NODE_SIZE(&node);
  int indent = level/W*2;
  if (level == MAX_NODE_LEVEL) {
    printf("%*s<node-collision ref_count=%llu size=%u slab=%p slots=[",
      indent, "", node.slab->header.ref_count, sz, node.slab);
    for (int i = 0; i < sz; i++) {
      Kv kv = NODE_GET_SLOT(&node, i)->kv;
      printf("<kv k=%lu v=%lu>,", kv.k, kv.v);
    }
  } else {
    printf("%*s<node ref_count=%llu level=%d size=%u bitmap=0x%llx slab=%p slots=[\n",
      indent, "", node.slab->header.ref_count, level, sz, node.bitmap, node.slab);
    for (int i = 0; i < sz; i++) {
      Slot* slot = NODE_GET_SLOT(&node, i);
      if (SLOT_IS_NODE(slot)) {
        _debug(slot, level + W);
      } else {
        Kv kv = slot->kv;
        printf("%*s<kv k=%lu v=%lu>\n", indent + 2, "", kv.k, kv.v);
      }
    }
  }
  printf("%*s]>\n", indent, "");
}

static void _map_destructor(void* p) {
  Map* map = p;
  int size = MAP_ROOT_SIZE(map);
  for (int i = 0; i < size; i++) {
    SLOT_RELEASE(map->slots + i, MAP_IS_INT_VALUED(map));
  }
}

static void _check_find() {
  // build a node and try search it

  // assume level is W
  Slab* slab = SLAB_ALLOC(3);

  uint64_t hash0 = 1;
  slab->slots[0].kv.k = VAL_TRUE;
  slab->slots[0].kv.v = VAL_FROM_INT(0);

  uint64_t hash1 = 1 << W;
  slab->slots[1].kv.k = VAL_FALSE;
  slab->slots[1].kv.v = VAL_FROM_INT(1);

  Slab* child_slab = SLAB_ALLOC(1);
  child_slab->slots[0].kv.k = VAL_NIL;
  child_slab->slots[0].kv.v = VAL_FROM_INT(2);
  uint64_t hash2 = ((7 << W) | 3);

# if 0
  printf("\nhash0: %llx hash1: %llx hash2: %llx\n", hash0, hash1, hash2);
  printf("pos0: %x pos1: %x pos2: %x\n", GET_POS(hash0, W), GET_POS(hash1, W), GET_POS(hash2, W));
  printf("flag0: %llx flag1: %llx flag2: %llx\n", GET_FLAG(GET_POS(hash0, W)), GET_FLAG(GET_POS(hash1, W)), GET_FLAG(GET_POS(hash2, W)));
# endif

  Node child_node = {.bitmap = 0, .slab = child_slab};
  BM_SET_FLAG(&child_node.bitmap, GET_FLAG(GET_POS(hash2, W + W)), true);
  slab->slots[2].node = child_node;

  Slot slot = {.node = {.bitmap = 0, .slab = slab}};
  int flag = (GET_FLAG(GET_POS(hash0, W)) | GET_FLAG(GET_POS(hash1, W)) | GET_FLAG(GET_POS(hash2, W)));
  BM_SET_FLAG(&slot.node.bitmap, flag, true);

  Val v;
  int index;

  index = SLOT_FIND(&slot, hash0, VAL_TRUE, &v, W);
  assert(index == 0);

  index = SLOT_FIND(&slot, hash1, VAL_FALSE, &v, W);
  assert(index == 1);

  index = SLOT_FIND(&slot, hash2, VAL_NIL, &v, W);
  assert(index == 2);

  index = SLOT_FIND(NODE_GET_SLOT(&slot.node, 0), hash0, VAL_TRUE, &v, W + W);
  assert(index == FOUND_KV);
  assert(v == VAL_FROM_INT(0));

  index = SLOT_FIND(NODE_GET_SLOT(&slot.node, 1), hash1, VAL_FALSE, &v, W + W);
  assert(index == FOUND_KV);
  assert(v == VAL_FROM_INT(1));

  index = SLOT_FIND(NODE_GET_SLOT(&slot.node, 2), hash2, VAL_NIL, &v, W + W);
  assert(index == 0);

  val_free(child_slab);
  val_free(slab);
}

static void _check_root_insert() {
  // insert into root
  Val map = nb_map_new_i();
  map = nb_map_insert(map, VAL_TRUE, 3);

  Map* m = (Map*)map;
  assert(MAP_IS_INT_VALUED(m));
  assert(MAP_SIZE(m) == 1);
  assert(MAP_ROOT_SIZE(m) == 1);

  Slot* slots = m->slots;
  assert(m->slots[0].kv.k == VAL_TRUE);
  assert(m->slots[0].kv.v == 3);
  assert(m->header.ref_count == 1);

  val_free(m);
}
