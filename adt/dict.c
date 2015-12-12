#include "dict.h"
#include "dict-bucket.h"
#include "dict-map.h"
#include <string.h>
#include <stdlib.h>

// memory-efficient, cache-conscious dictionary implemented as immutable tree
// a mixture of HAT-trie, HAMT

// todo support mmap for disk storage in the future
// todo currently it is utf8 binary collation, find a way to move to utf8 code point collation?

// todo when inserting large k, put it in a separate chunk
typedef struct {
  ValHeader h;
  int64_t size;
  Val root;
} Dict;

typedef struct {
  int parent_bytes;
  int child_bytes;
} PartionData;

#pragma mark ## operation to the value attached to node

static bool IS_NODE(Val m) {
  if (m == VAL_UNDEF) {
    return false;
  }
  return VAL_KLASS(m) == KLASS_DICT_MAP || VAL_KLASS(m) == KLASS_DICT_BUCKET;
}

static bool IS_BUCKET(Val m) {
  if (m == VAL_UNDEF) {
    return false;
  }
  return VAL_KLASS(m) == KLASS_DICT_BUCKET;
}

static bool IS_MAP(Val m) {
  if (m == VAL_UNDEF) {
    return false;
  }
  return VAL_KLASS(m) == KLASS_DICT_MAP;
}

static bool IS_DICT(Val m) {
  if (m == VAL_UNDEF) {
    return false;
  }
  return VAL_KLASS(m) == KLASS_DICT;
}

#define GET_V(m) (m)->v
#define HAS_V(m) (GET_V(m) != VAL_UNDEF)

// NOTE mut operation
// return true if entries added, false if entry number unchanged
static bool SET_V(Val m, Val v) {
  assert(IS_NODE(m));
  Val old_v = GET_V((Map*)m);
  ((Map*)m)->v = v;
  RETAIN(v);
  RELEASE(old_v);
  return old_v == VAL_UNDEF;
}

inline static Dict* DICT_NEW() {
  Dict* d = val_alloc(sizeof(Dict));
  d->h.klass = KLASS_DICT;
  return d;
}

inline static void DICT_DESTROY(void* p) {
  Dict* d = p;
  RELEASE(d->root);
}

#pragma mark ### helper decl

static BucketIter _prefix_of_k(Bucket* b, const char* k, size_t ksize);
static PartionData _prefix_partition(Bucket* b, const char* k, size_t ksize);

static bool _generic_find(Val m, const char* k, size_t ksize, Val* v);
static bool _bucket_find(Bucket* b, const char* k, size_t ksize, Val* v);
static bool _map_find(Map* m, const char* k, size_t ksize, Val* v);

static bool _generic_insert(Val* v_addr, const char* k, size_t ksize, Val v);
static bool _bucket_insert(Val* v_addr, const char* k, size_t ksize, Val v);
static bool _map_insert(Val* v_addr, const char* k, size_t ksize, Val v);

static Map* _burst(Bucket* b, uint8_t extra_c);
static void _bucket_debug(Bucket* b, bool bucket_as_binary);
static void _map_debug(Map* map, bool bucket_as_binary);

#pragma mark ### interface

static Val empty_dict;

void nb_dict_init_module() {
  Dict* d = DICT_NEW();
  val_perm(d);
  d->size = 0;
  d->root = VAL_UNDEF;
  empty_dict = (Val)d;

  klass_def_internal(KLASS_DICT_MAP, val_strlit_new_c("DictMap"));
  klass_set_destruct_func(KLASS_DICT_MAP, MAP_DESTROY);
  klass_def_internal(KLASS_DICT_BUCKET, val_strlit_new_c("DictBucket"));
  klass_set_destruct_func(KLASS_DICT_BUCKET, BUCKET_DESTROY);
  klass_def_internal(KLASS_DICT, val_strlit_new_c("Dict"));
  klass_set_destruct_func(KLASS_DICT, DICT_DESTROY);
}

Val nb_dict_new() {
  return empty_dict;
}

Val nb_dict_new_with_root(Val root, int64_t size) {
  Dict* d = DICT_NEW();
  d->size = size;
  d->root = root;
  return (Val)d;
}

size_t nb_dict_size(Val dict) {
  Dict* d = (Dict*)dict;
  return d->size;
}

bool nb_dict_find(Val dict, const char* k, size_t ksize, Val* v) {
  Dict* obj = (Dict*)dict;
  if (obj->size == 0) {
    return false;
  }

  Val root = obj->root;
  bool res;
  if (IS_BUCKET(root)) {
    res = _bucket_find((Bucket*)root, k, ksize, v);
  } else {
    res = _map_find((Map*)root, k, ksize, v);
  }
  // if (res) {
  //   RETAIN(*v);
  // }
  return res;
}

Val nb_dict_insert(Val dict, const char* k, size_t ksize, Val v) {
  Dict* r = (Dict*)dict;
  r = val_dup(r, sizeof(Dict), sizeof(Dict));

  bool added = _generic_insert(&r->root, k, ksize, v);

  if (added) {
    r->size++;
  }
  return (Val)r;
}

bool nb_dict_remove(Val dict, const char* k, size_t ksize, Val* v) {
  // todo
  return false;
}

void nb_dict_debug(Val v, bool bucket_as_binary) {
  if (IS_BUCKET(v)) {
    Bucket* b = (Bucket*)v;
    _bucket_debug(b, bucket_as_binary);
  } else if (IS_MAP(v)) {
    Map* m = (Map*)v;
    _map_debug(m, bucket_as_binary);
  } else if (IS_DICT(v)) {
    Dict* d = (Dict*)v;
    printf("\n=== <Dict#%p size=%llu> ===\n", d, d->size);
    nb_dict_debug(d->root, bucket_as_binary);
  }
}

#pragma mark ### helper impl

static bool _generic_find(Val m, const char* k, size_t ksize, Val* v) {
  if (ksize == 0) {
    if (IS_NODE(m)) {
      Val got = GET_V((Map*)m);
      if (got == VAL_UNDEF) {
        return false;
      } else {
        *v = got;
        return true;
      }
    } else {
      *v = m;
      return true;
    }
  } else {
    if (IS_BUCKET(m)) {
      return _bucket_find((Bucket*)m, k, ksize, v);
    } else if (IS_MAP(m)) {
      return _map_find((Map*)m, k, ksize, v);
    } else {
      return false;
    }
  }
}

static bool _bucket_find(Bucket* b, const char* k, size_t ksize, Val* v) {
  assert(ksize);

  BucketIter it = _prefix_of_k(b, k, ksize);
  if (BUCKET_ITER_IS_END(&it)) {
    return false;
  } else {
    return _generic_find(*it.v, k + it.ksize, ksize - it.ksize, v);
  }
}

static bool _map_find(Map* m, const char* k, size_t ksize, Val* v) {
  int index = BIT_MAP_INDEX(m->bit_map, k[0]);
  if (index < 0) {
    return false;
  }
  return _generic_find(m->slots[index], k + 1, ksize - 1, v);
}
// #include "utils/backtrace.h"

static BucketIter _prefix_of_k(Bucket* b, const char* k, size_t ksize) {
  BucketIter it = BUCKET_ITER_NEW(b);
  for (; !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    if (str_is_prefix(it.ksize, it.k, ksize, k)) {
      return it;
    }
  }
  return it;
}

static PartionData _prefix_partition(Bucket* b, const char* k, size_t ksize) {
  PartionData r = {0, 0};
  bool k_inserted_in_parent = false;
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    if (str_is_prefix(ksize, k, it.ksize, it.k)) {
      assert(it.ksize > ksize);
      r.child_bytes += BUCKET_ENTRY_BYTES(it.ksize - ksize);
      if (!k_inserted_in_parent) {
        r.parent_bytes += BUCKET_ENTRY_BYTES(ksize);
      }
    } else {
      r.parent_bytes += BUCKET_ENTRY_BYTES(it.ksize);
    }
  }
  return r;
}

static bool _generic_insert(Val* v_addr, const char* k, size_t ksize, Val v) {
  if (IS_MAP(*v_addr)) {
    return _map_insert(v_addr, k, ksize, v);
  } else if (IS_BUCKET(*v_addr)) {
    return _bucket_insert(v_addr, k, ksize, v);
  } else if (ksize) {
    if (*v_addr == VAL_UNDEF) {
      *v_addr = (Val)BUCKET_NEW_KV(k, ksize, v);
    } else {
      REPLACE(*v_addr, (Val)BUCKET_NEW_2(*v_addr, k, ksize, v));
    }
    return true;
  } else {
    bool added = (*v_addr == VAL_UNDEF);
    if (!val_eq(v, *v_addr)) {
      RETAIN(v);
      REPLACE(*v_addr, v);
    }
    return added;
  }
}

static bool _bucket_insert(Val* vptr, const char* k, size_t ksize, Val v) {
  assert(ksize);
  Bucket* b = (Bucket*)(*vptr);

  // - prefix of k is in bucket
  //   insert (k - prefix), v
  BucketIter it = _prefix_of_k(b, k, ksize);
  if (!BUCKET_ITER_IS_END(&it)) {
    *vptr = (Val)BUCKET_DUP(b);
    Val* v_addr = (Val*)(((Bucket*)(*vptr))->data + ((char*)it.v - b->data));
    return _generic_insert(v_addr, k + it.ksize, ksize - it.ksize, v);
  }

  // - k is prefix of at least one key in bucket
  //   remove keys that start with k, and place them into a new bucket
  //   (since prev bucket can hold them all, the new bucket can hold them all too)
  PartionData partition = _prefix_partition(b, k, ksize);
  if (partition.child_bytes) {
    Bucket* parent = BUCKET_NEW(partition.parent_bytes);
    int parent_pos = 0;
    Bucket* child = BUCKET_NEW(partition.child_bytes);
    int child_pos = 0;
    bool parent_inserted = false;
    SET_V((Val)child, v);
    for (it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
      if (str_is_prefix(ksize, k, it.ksize, it.k)) {
        assert(ksize < it.ksize); // not gonna be eq, since it falls in prev case
        child_pos = DATA_SET(child->data, child_pos, it.ksize - ksize, it.k + ksize, *it.v);
        if (!parent_inserted) {
          parent_pos = DATA_SET(parent->data, parent_pos, ksize, k, (Val)child);
          parent_inserted = true;
        }
      } else {
        parent_pos = DATA_SET(parent->data, parent_pos, it.ksize, it.k, *it.v);
      }
    }
    assert(parent_pos == partition.parent_bytes);
    assert(child_pos == partition.child_bytes);
    *vptr = (Val)parent;
    return true;
  }

  // - k not in bucket and can fit inside
  //   insert k and v
  if (BUCKET_ENTRIES(b) < BUCKET_MAX_ENTRIES && BUCKET_ENTRY_BYTES(ksize) + BUCKET_BYTES(b) < BUCKET_MAX_BYTES) {
    *vptr = (Val)BUCKET_NEW_INSERT(b, k, ksize, v);
    return true;
  }

  // - k not in bucket and can not fit inside
  //   burst into map
  //   TODO (complex but saves space?) or burst into bucket based on count of first char
  //        the threshold is chosen that:
  //          8 * 4 = map_bitmask_space ~= bucket_key_string_space = threshold * 3
  //        then we have threshold ~ 9
  //        since searching map is a bit faster, so we take a balanced threshold of 6
  Map* m = _burst(b, (uint8_t)k[0]);
  *vptr = (Val)m;
  Val* slot = MAP_SLOT(m, (uint8_t)k[0]);
  if (*slot == VAL_UNDEF) {
    if (ksize == 1) {
      RETAIN(v);
      *slot = v;
    } else {
      REPLACE(*slot, (Val)BUCKET_NEW_KV(k + 1, ksize - 1, v));
    }
    return true;
  } else {
    // insert bucket
    assert(IS_BUCKET(*slot));
    if (ksize == 1) {
      return SET_V(*slot, v);
    } else {
      return _bucket_insert(slot, k + 1, ksize - 1, v);
    }
  }
}

static bool _map_insert(Val* vptr, const char* k, size_t ksize, Val v) {
  Map* m = (Map*)(*vptr);
  assert(MAP_SIZE(m) == BIT_MAP_COUNT(m->bit_map));
  assert(ksize);

  uint8_t c = (uint8_t)k[0];
  int index;
  bool hit = BIT_MAP_HIT(m->bit_map, c, &index);
  Map* r;

  if (!hit) {
    r = MAP_NEW(MAP_SIZE(m) + 1);
    memcpy(r->bit_map, m->bit_map, sizeof(uint64_t) * 4);
    BIT_MAP_SET(r->bit_map, c, true);

    int j = 0;
    for (int i = 0; i < MAP_SIZE(r); i++) {
      if (i == index) {
        if (ksize == 1) {
          RETAIN(v);
          r->slots[i] = v;
        } else {
          r->slots[i] = (Val)BUCKET_NEW_KV(k + 1, ksize - 1, v);
        }
      } else {
        r->slots[i] = m->slots[j++];
        RETAIN(r->slots[i]);
      }
    }

    assert(MAP_SIZE(r) == BIT_MAP_COUNT(r->bit_map));
    *vptr = (Val)r;
    return true;
  } else {
    r = MAP_NEW(MAP_SIZE(m));
    memcpy(r->bit_map, m->bit_map, sizeof(uint64_t) * 4);

    bool added = false;
    for (int i = 0; i < MAP_SIZE(r); i++) {
      if (i == index) {
        added = _generic_insert(r->slots + index, k + 1, ksize - 1, v);
      } else {
        r->slots[i] = m->slots[i];
        RETAIN(r->slots[i]);
      }
    }

    assert(MAP_SIZE(r) == BIT_MAP_COUNT(r->bit_map));
    *vptr = (Val)r;
    return added;
  }
}

// burst the bucket into map, the resulting key set of the map also contains bucket for k
// the slot for extra_c is left 0 when no other keys start with extra_c
// worst case: 2 keys with very long common prefix
//             TODO we can eliminate worst cases by adding long key map
static Map* _burst(Bucket* b, uint8_t extra_c) {
  uint64_t bit_map[] = {0, 0, 0, 0};

  // to store sizes of each slot, so we can avoid much allocation
  // NOTE not storing for extra_c
  // TODO do not alloc bucket when entries == 1
  typedef struct {
    int8_t has_v;
    int8_t entries;
    int16_t bytes;
  } Sizes;
  Sizes sizes[256];
  memset(sizes, 0, sizeof(Sizes) * 256);

  // set up bit map for the new map
  BIT_MAP_SET(bit_map, extra_c, true);
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    uint8_t c = (uint8_t)it.k[0];
    BIT_MAP_SET(bit_map, c, true);
    if (it.ksize == 1) {
      sizes[c].has_v = 1;
    } else {
      sizes[c].entries++;
      assert(it.ksize);
      sizes[c].bytes += BUCKET_ENTRY_BYTES(it.ksize); // TODO what if bytes exceed 32768?
    }
  }

  // alloc memory for buckets
  // each child node is <= b in bytes, so they are all buckets
  Map* m = MAP_NEW(BIT_MAP_COUNT(bit_map));
  SET_V((Val)m, GET_V(b));
  memcpy(m->bit_map, bit_map, sizeof(uint64_t) * 4);
  for (int i = 0, j = 0; i < 256; i++) {
    if (sizes[i].has_v && !sizes[i].entries) {
      m->slots[j] = VAL_UNDEF; // do not create new bucket
      j++;
    } if (sizes[i].entries) {
      m->slots[j] = (Val)BUCKET_NEW(sizes[i].bytes);
      BUCKET_BYTES(m->slots[j]) = 0; // in next section, BUCKET_BYTES will be used as pos first
      j++;
    } else if (i == extra_c) {
      m->slots[j] = VAL_UNDEF; // to be filled by code after _burst()
      j++;
    }
  }

  // copy keys and values
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    int index = BIT_MAP_INDEX(m->bit_map, (uint8_t)it.k[0]);
    Val slot = m->slots[index];
    if (slot == VAL_UNDEF) {
      assert(it.ksize == 1);
      RETAIN(*it.v);
      m->slots[index] = *it.v;
    } else {
      Bucket* child = (Bucket*)m->slots[index];
      if (it.ksize > 1) {
        RETAIN(*it.v);
        BUCKET_ENTRIES(child)++;
        BUCKET_BYTES(child) = DATA_SET(child->data, BUCKET_BYTES(child), it.ksize - 1, it.k + 1, *it.v);
      } else {
        SET_V((Val)child, *it.v);
      }
    }
  }

  return m;
}

static void _bucket_debug(Bucket* b, bool bucket_as_binary) {
  printf("<Bucket#%p rc=%llu v=%lu bytes=%d data=",
  b, VAL_REF_COUNT((Val)b), GET_V(b), (int)BUCKET_BYTES(b));
  if (bucket_as_binary) {
    printf("[");
    for (int i = 0; i < BUCKET_BYTES(b); i++) {
      printf("%x ", (uint8_t)b->data[i]);
    }
    printf("]");
  } else {
    printf("{");
    for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it) ; BUCKET_ITER_NEXT(b, &it)) {
      int64_t ref_count = (VAL_IS_IMM(*it.v) ? -1 : VAL_REF_COUNT(*it.v));
      printf("\"%.*s\": %lx(klass=%u,rc=%lld), ",
      it.ksize, it.k, *it.v, (uint32_t)VAL_KLASS(*it.v), ref_count);
    }
    printf("}");
  }
  printf(">\n");
}

static void _map_debug(Map* m, bool bucket_as_binary) {
  // assert(m->h.size == BIT_MAP_COUNT(m->bit_map));
  if (MAP_SIZE(m) != BIT_MAP_COUNT(m->bit_map)) {
    printf("Bad map: size=%d bit_map_count=%d\n", (int)MAP_SIZE(m), BIT_MAP_COUNT(m->bit_map));
    exit(-1);
  }
  printf("<Map#%p rc=%llu v=%lu size=%d bitmap=[",
  m, VAL_REF_COUNT((Val)m), GET_V(m), (int)MAP_SIZE(m));
  for (int i = 0; i < 4; i++) {
    printf("%llx ", m->bit_map[i]);
  }
  printf("] slots={");
  for (int c = 0; c < 256; c++) {
    int i = BIT_MAP_INDEX(m->bit_map, (char)((unsigned char)c));
    if (i >= 0) {
      Val obj = m->slots[i];
      int64_t ref_count = (VAL_IS_IMM(obj) ? -1 : VAL_REF_COUNT(obj));
      printf("'%c': %lx(klass=%u,rc=%lld), ", c, obj, (uint32_t)VAL_KLASS(obj), ref_count);
    }
  }
  printf("}>\n");
  for (int i = 0; i < MAP_SIZE(m); i++) {
    if (IS_NODE(m->slots[i])) {
      nb_dict_debug(m->slots[i], bucket_as_binary);
    }
  }
}
