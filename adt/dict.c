#include "dict.h"
#include <string.h>
#include <stdlib.h>

// memory-efficient, cache-conscious dictionary implemented as immutable tree
// a mixture of HAT-trie, HAMT

// todo support mmap for disk storage in the future
// todo currently it is utf8 binary collation, find a way to move to utf8 code point collation?

// 16384 is better for reading, but insertion will be too slow if bucket too small
#define BUCKET_MAX 4096

static uint32_t KLASS_DICT_BUCKET;
static uint32_t KLASS_DICT_MAP;
// todo static uint32_t KLASS_DICT_FATMAP;
// todo static uint32_t KLASS_DICT_EDGE;

// retain/release of internal nodes are managed by dict
// dynamic allocated nodes are: bucket, map, fat_map and edge

// common header shared by dict/nodes
typedef struct {
  ValHeader vh; // flags = has_v
  int64_t size;
  Val v;
} Header;

// sequence encoded kvs chunk
typedef struct {
  Header h;     // size = of data
  char data[];  // 2 bytes of key len, key, value, terminates with 0
} Bucket;

#define BUCKET_ITER_END_POS -1
typedef struct {
  int pos; // pos after current v, =BUCKET_ITER_END_POS for end
  int ksize;
  const char* k;
  Val* v;
} BucketIter;

// mini popcnt based map
typedef struct {
  Header h;            // size = of slots
  uint64_t bit_map[4]; // the HAMT way of encoding, slots size = popcnt(bit_map), and they are sorted by the byte
  Header* slots[];
} Map;

// (todo) for optimizing the speed of rich map node
typedef struct {
  Header h;
  Header* arr[256]; // empty slots are filled with VAL_UNDEF
} FatMap;

// (todo) for singular long keys
typedef struct {
  Header h;
  char data[];
} Edge;

typedef struct {
  Header h;
  Header* root;
} Dict;

#define HAS_V(_d_) ((ValHeader*)(_d_))->flags

// set v on bucket/map/dict
// return true if entries added, false if entry number unchanged
inline static bool SET_V(Header* h, Val v) {
  bool added = !HAS_V(h);
  HAS_V(h) = 1;
  Val old_v = h->v;
  h->v = v;
  RETAIN(v);
  if (!added) {
    RELEASE(old_v);
  }
  return added;
}

// remove v on bucket/map/dict
// return true if removed, false if nothing removed
inline static bool REMOVE_V(Header* h, Val* v) {
  if (HAS_V(v)) {
    HAS_V(h) = 0;
    Val ret_v = h->v;
    h->v = *v;
    *v = ret_v;
    return true;
  }
  return false;
}

inline static bool IS_BUCKET(Header* h) {
  return h->vh.klass == KLASS_DICT_BUCKET;
}

inline static bool IS_MAP(Header* h) {
  return h->vh.klass == KLASS_DICT_MAP;
}

inline static void DATA_SET_SZ(char* data, uint16_t sz) {
  *((uint16_t*)data) = sz;
}

inline static void DATA_SET_STR(char* data, int sz, const char* str) {
  assert((const char*)data != str);
  memcpy(data, str, sz);
}

inline static void DATA_SET_VAL(char* data, Val v) {
  *((Val*)data) = v;
}

inline static BucketIter BUCKET_ITER_NEW(Bucket* b) {
  int ksize = *((uint16_t*)b->data);
  if (ksize) {
    BucketIter it = {
      .pos = ksize + (sizeof(uint16_t) + sizeof(Val)),
      .ksize = ksize,
      .k = b->data + 2,
      .v = (Val*)(b->data + 2 + ksize)
    };
    return it;
  } else {
    BucketIter it = {.pos = BUCKET_ITER_END_POS};
    return it;
  }
}

inline static bool BUCKET_ITER_IS_END(BucketIter* it) {
  return it->pos == BUCKET_ITER_END_POS;
}

inline static void BUCKET_ITER_NEXT(Bucket* b, BucketIter* it) {
  assert(!BUCKET_ITER_IS_END(it));

  it->ksize = *((uint16_t*)(b->data + it->pos));
  it->pos += 2;
  if (it->ksize == 0) {
    it->pos = BUCKET_ITER_END_POS;
    return;
  }

  it->k = b->data + it->pos;
  it->pos += it->ksize;
  it->v = (Val*)(b->data + it->pos);
  it->pos += sizeof(Val);
}

// set bucket data according to content of iter
// NOTE in this function, it->pos initially works as starting pos,
//      then after the call, offset is added and it->pos becomes the same finishing pos as used in other iter functions
inline static void BUCKET_ITER_SET_BACK(Bucket* b, BucketIter* it) {
  if (it->ksize == 0) {
    DATA_SET_SZ(b->data + it->pos, 0);
    it->pos += 2;
    return;
  }
  DATA_SET_SZ(b->data + it->pos, it->ksize);
  it->pos += sizeof(uint16_t);
  DATA_SET_STR(b->data + it->pos, it->ksize, it->k);
  it->pos += it->ksize;
  DATA_SET_VAL(b->data + it->pos, *it->v);
  it->pos += sizeof(Val);
}

inline static Dict* DICT_NEW() {
  Dict* d = val_alloc(sizeof(Dict));
  d->h.vh.klass = KLASS_DICT;
  return d;
}

// NOTE sz includes the terminating \x00\x00
inline static Bucket* BUCKET_NEW(size_t sz) {
  assert(sz >= 2);
  Bucket* b = val_alloc(sizeof(Bucket) + sizeof(char) * sz);
  b->h.size = sz;
  b->h.vh.klass = KLASS_DICT_BUCKET;
  return b;
}

inline static Bucket* BUCKET_NEW_KV(const char* k, size_t ksize, Val v) {
  if (ksize == 0) {
    Bucket* b = BUCKET_NEW(2);
    HAS_V(b) = 1;
    b->h.v = v;
    return b;
  }

  // key sz + leading sz + val sz + terminate sz
  size_t size = ksize + (sizeof(uint16_t) + sizeof(Val) + sizeof(uint16_t));
  Bucket* b = BUCKET_NEW(size);
  BucketIter iter = {
    .pos = 0,
    .ksize = ksize,
    .k = k,
    .v = &v
  };
  BUCKET_ITER_SET_BACK(b, &iter);
  iter.ksize = 0;
  BUCKET_ITER_SET_BACK(b, &iter);
  return b;
}

inline static void BUCKET_RETAIN_ALL(Bucket* b) {
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    RETAIN(*it.v);
  }
}

inline static Bucket* BUCKET_DUP(Bucket* b) {
  BUCKET_RETAIN_ALL(b);
  size_t size = sizeof(Bucket) + sizeof(char) * b->h.size;
  Bucket* r = val_dup(b, size, size);
  return r;
}

inline static Map* MAP_NEW(size_t sz) {
  Map* m = val_alloc(sizeof(Map) + sizeof(Header*) * sz);
  m->h.size = sz;
  m->h.vh.klass = KLASS_DICT_MAP;
  return m;
}

inline static Map* MAP_DUP(Map* m) {
  size_t size = sizeof(Map) + sizeof(Header*) * m->h.size;
  Map* r = val_dup(m, size, size);
  for (int i = 0; i < m->h.size; i++) {
    RETAIN(r->slots[i]);
  }
  return r;
}

inline static void BIT_MAP_SET(uint64_t* bit_map, char c, bool set) {
  uint8_t d = (uint8_t)c;
  for (int i = 0; i < 4; i++) {
    if (d < 64) {
      if (set) {
        bit_map[i] |= (1ULL << d);
      } else {
        bit_map[i] &= ~(1ULL << d);
      }
      break;
    } else {
      d -= 64;
    }
  }
}

inline static int BIT_MAP_COUNT(uint64_t* bit_map) {
  int r = 0;
  for (int i = 0; i < 4; i++) {
    r += PDLEX_POPCNT(bit_map[i]);
  }
  return r;
}

// return -1 if not found
inline static int BIT_MAP_INDEX(uint64_t* bit_map, char c) {
  uint8_t d = (uint8_t)c;
  int index = 0;
  for (int i = 0; i < 4; i++) {
    if (d < 64) {
      if (bit_map[i] & (1ULL << d)) {
        index += PDLEX_POPCNT(bit_map[i] << (64 - d)); // count bits lower than it
        break;
      } else {
        // not found
        return -1;
      }
    } else {
      index += PDLEX_POPCNT(bit_map[i]);
      d -= 64;
    }
  }
  return index;
}

// alternate func to BIT_MAP_INDEX, which still returns index
inline static bool BIT_MAP_HIT(uint64_t* bit_map, char c, int* index) {
  uint8_t d = (uint8_t)c;
  *index = 0;
  for (int i = 0; i < 4; i++) {
    if (d < 64) {
      *index += PDLEX_POPCNT(bit_map[i] << (64 - d)); // count bits lower than it
      return (bit_map[i] & (1ULL << d));
    } else {
      *index += PDLEX_POPCNT(bit_map[i]);
      d -= 64;
    }
  }
  return false; // impossible here
}

#pragma mark ### helper decl

static bool _bucket_find(Bucket* b, const char* k, size_t ksize, Val* v);
static bool _map_find(Map* m, const char* k, size_t ksize, Val* v);
static Header* _bucket_insert(Bucket* b, const char* k, size_t ksize, Val v, bool* added);
static Header* _map_insert(Map* b, const char* k, size_t ksize, Val v, bool* added);
static Map* _burst(Bucket* b);
static void _bucket_debug(Bucket* b, bool bucket_as_binary);
static void _map_debug(Map* map, bool bucket_as_binary);
static void _dict_destructor(void* dict);
static void _node_release(Header* node);

#pragma mark ### interface

static Val empty_dict;
static void _init() __attribute__((constructor(100)));
static void _init() {
  KLASS_DICT_BUCKET = val_new_class_id();
  KLASS_DICT_MAP = val_new_class_id();

  Dict* d = DICT_NEW();
  val_perm(d);
  d->h.size = 0;
  Bucket* bucket = BUCKET_NEW(2);
  val_perm(bucket);
  BucketIter it = {.pos = 0, .ksize = 0};
  BUCKET_ITER_SET_BACK(bucket, &it);
  d->root = (Header*)bucket;
  empty_dict = (Val)d;

  val_register_destructor_func(KLASS_DICT, _dict_destructor);
}

Val nb_dict_new() {
  return empty_dict;
}

size_t nb_dict_size(Val dict) {
  Dict* d = (Dict*)dict;
  return d->h.size;
}

bool nb_dict_find(Val dict, const char* k, size_t ksize, Val* v) {
  Dict* obj = (Dict*)dict;
  if (obj->h.size == 0) {
    return false;
  }
  if (ksize == 0 && HAS_V(obj)) {
    return obj->h.v;
  }
  Header* h = obj->root;
  bool res;
  if (IS_BUCKET(h)) {
    res = _bucket_find((Bucket*)h, k, ksize, v);
  } else {
    res = _map_find((Map*)h, k, ksize, v);
  }
  if (res) {
    RETAIN(*v);
  }
  return res;
}

Val nb_dict_insert(Val dict, const char* k, size_t ksize, Val v) {
  Dict* r = (Dict*)dict;
  r = val_dup(r, sizeof(Dict), sizeof(Dict));
  Header* h = r->root;

  if (ksize == 0) {
    if (HAS_V(r)) {
      RELEASE(r->h.v);
    }
    HAS_V(r) = 1;
    r->h.v = v;
    return (Val)r;
  }

  bool added;
  if (IS_BUCKET(h)) {
    r->root = _bucket_insert((Bucket*)h, k, ksize, v, &added);
  } else {
    r->root = _map_insert((Map*)h, k, ksize, v, &added);
  }
  if (added) {
    r->h.size++;
  }
  return (Val)r;
}

bool nb_dict_remove(Val dict, const char* k, size_t ksize, Val* v) {
  // todo
  return false;
}

// stored elements:
// a -> [lex=0, ha=1] (2+3+8 + 2+2+8 + 1)
// z -> [not=2, hooz=3] (2+3+8 + 2+4+8 + 1)
Val nb_dict_build_test_map_to_buckets() {
  Dict* d = DICT_NEW();
  d->root = (Header*)MAP_NEW(2);
  d->h.size = 4;
  Map* m = (Map*)d->root;

  BIT_MAP_SET(m->bit_map, 'a', true);
  BIT_MAP_SET(m->bit_map, 'z', true);

  Bucket* b;
  BucketIter it;

  b = BUCKET_NEW(2+3+8 + 2+2+8 + 2);
  it.pos = 0;
  Val v;
# define ADD_KV(_k, _v) it.ksize = strlen(_k); it.k = (char*)_k; v = VAL_FROM_INT(_v); it.v = &v; BUCKET_ITER_SET_BACK(b, &it);
  ADD_KV("lex", 0);
  ADD_KV("ha", 1);
  it.ksize = 0;
  BUCKET_ITER_SET_BACK(b, &it);
  assert(it.pos == b->h.size);
  m->slots[0] = (Header*)b;

  b = BUCKET_NEW(2+3+8 + 2+4+8 + 2);
  it.pos = 0;
  ADD_KV("not", 2);
  ADD_KV("hooz", 3);
  it.ksize = 0;
  BUCKET_ITER_SET_BACK(b, &it);
  assert(it.pos == b->h.size);
  m->slots[1] = (Header*)b;
# undef ADD_KV

  return (Val)d;
}

// full bucket with keys "2" and "4..." (size=x)
// 2+1+8 + 2+x+8 + 2 = BUCKET_MAX
Val nb_dict_build_test_full_bucket() {
  Dict* d = DICT_NEW();
  Bucket* b = BUCKET_NEW(BUCKET_MAX);
  d->root = (Header*)b;
  d->h.size = 2;

  Val v;
  v = VAL_FROM_INT(2);
  BucketIter it = {.pos = 0, .k = "2", .ksize = 1, .v = &v};
  BUCKET_ITER_SET_BACK(b, &it);

  uint16_t ksize = BUCKET_MAX - 2 - (2+8) - (2+1+8);
  char k[ksize];
  memset(k, '4', ksize);
  it.ksize = ksize;
  it.k = k;
  v = VAL_FROM_INT(4);
  it.v = &v;
  BUCKET_ITER_SET_BACK(b, &it);
  it.ksize = 0;
  BUCKET_ITER_SET_BACK(b, &it);

  assert(it.pos == b->h.size);

  return (Val)d;
}

void nb_dict_debug(Val h, bool bucket_as_binary) {
  if (IS_BUCKET((Header*)h)) {
    _bucket_debug((Bucket*)h, bucket_as_binary);
  } else if (IS_MAP((Header*)h)) {
    _map_debug((Map*)h, bucket_as_binary);
  } else {
    Dict* d = (Dict*)h;
    printf("\n=== <Dict#%p size=%llu> ===\n", d, d->h.size);
    nb_dict_debug((Val)d->root, bucket_as_binary);
  }
}

#pragma mark ### helper impl

static bool _bucket_find(Bucket* b, const char* k, size_t ksize, Val* v) {
  if (ksize == 0) {
    if (HAS_V(b)) {
      *v = b->h.v;
      return true;
    } else {
      return false;
    }
  }

  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    if (ksize == it.ksize && strncmp(it.k, k, ksize) == 0) {
      *v = *it.v;
      return true;
    }
  }
  return false;
}

static bool _map_find(Map* m, const char* k, size_t ksize, Val* v) {
  if (ksize == 0) {
    if (HAS_V(m)) {
      *v = m->h.v;
      return true;
    } else {
      return false;
    }
  }

  int index = BIT_MAP_INDEX(m->bit_map, k[0]);
  if (index < 0) {
    return false;
  }
  Header* h = m->slots[index];
  if (IS_BUCKET(h)) {
    return _bucket_find((Bucket*)h, k + 1, ksize - 1, v);
  } else {
    return _map_find((Map*)h, k + 1, ksize - 1, v);
  }
}
#include "utils/backtrace.h"

static Header* _bucket_insert(Bucket* b, const char* k, size_t ksize, Val v, bool* added) {
  assert(b);
  if (ksize == 0) {
    Header* r = (Header*)BUCKET_DUP(b);
    *added = SET_V(r, v);
    return r;
  }

  // locate insertion pos in data
  // if key already exists in the bucket, dup bucket and set value in-place
  int insertion_pos = -1;
  int prev_end_pos = 0;
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    int cmp = strncmp(k, it.k, ksize > it.ksize ? it.ksize : ksize);
    if (cmp > 0 || (cmp == 0 && ksize > it.ksize)) {
      insertion_pos = prev_end_pos;
      break;
    } else if (cmp == 0 && ksize == it.ksize) {
      Bucket* r = BUCKET_DUP(b);
      DATA_SET_VAL(r->data + prev_end_pos + sizeof(uint16_t) + it.ksize, v);
      *added = false;
      return (Header*)r;
    }
    prev_end_pos = it.pos;
  }

  // now a new key is sure to be inserted
  *added = true;
  int new_bucket_size = b->h.size + ksize + (sizeof(uint16_t) + sizeof(Val));
  if (new_bucket_size > BUCKET_MAX) {
    // NOTE one more allocation for much simpler code
    // we can be sure that _burst only create 1 level of buckets
    Map* m = _burst(b);
    Header* ret = _map_insert(m, k, ksize, v, added);
    _node_release((Header*)m);
    return ret;
  } else {
    Bucket* r = BUCKET_NEW(new_bucket_size);
    BUCKET_RETAIN_ALL(b);
    if (insertion_pos == -1) { // append
      memcpy(r->data, b->data, b->h.size);
      BucketIter it = {.pos = b->h.size - 2, .ksize = ksize, .k = k, .v = &v};
      BUCKET_ITER_SET_BACK(r, &it);
      it.ksize = 0;
      BUCKET_ITER_SET_BACK(r, &it);
    } else {
      memcpy(r->data, b->data, insertion_pos);
      BucketIter it = {.pos = insertion_pos, .ksize = ksize, .k = k, .v = &v};
      BUCKET_ITER_SET_BACK(r, &it);
      assert(it.pos + (b->h.size - insertion_pos) == new_bucket_size);
      memcpy(r->data + it.pos, b->data + insertion_pos, b->h.size - insertion_pos);
    }
    return (Header*)r;
  }
}

static Header* _map_insert(Map* m, const char* k, size_t ksize, Val v, bool* added) {
  assert(m);
  assert(m->h.size == BIT_MAP_COUNT(m->bit_map));
  if (ksize == 0) {
    Header* r = (Header*)MAP_DUP(m);
    *added = SET_V(r, v);
    return r;
  }

  char c = k[0];
  int index;
  bool hit = BIT_MAP_HIT(m->bit_map, c, &index);
  Map* r;

  if (!hit) {
    r = MAP_NEW(m->h.size + 1);
    memcpy(r->bit_map, m->bit_map, sizeof(uint64_t) * 4);
    BIT_MAP_SET(r->bit_map, c, true);

    int j = 0;
    for (int i = 0; i < r->h.size; i++) {
      if (i == index) {
        r->slots[i] = (Header*)BUCKET_NEW_KV(k + 1, ksize - 1, v);
      } else {
        r->slots[i] = m->slots[j++];
        RETAIN(r->slots[i]);
      }
    }

    *added = true;
    assert(r->h.size == BIT_MAP_COUNT(r->bit_map));
  } else {
    r = MAP_NEW(m->h.size);
    memcpy(r->bit_map, m->bit_map, sizeof(uint64_t) * 4);

    for (int i = 0; i < r->h.size; i++) {
      if (i == index) {
        Header* h = r->slots[index];
        if (IS_BUCKET(h)) {
          r->slots[index] = _bucket_insert((Bucket*)h, k + 1, ksize - 1, v, added);
        } else {
          r->slots[index] = _map_insert((Map*)h, k + 1, ksize - 1, v, added);
        }
      } else {
        r->slots[i] = m->slots[i];
        RETAIN(r->slots[i]);
      }
    }

    *added = false;
    assert(r->h.size == BIT_MAP_COUNT(r->bit_map));
  }

  return (Header*)r;
}

// burst the bucket into map, the resulting key set of the map also contains bucket for k
static Map* _burst(Bucket* b) {
  uint64_t bit_map[] = {0, 0, 0, 0};

  // to store sizes of each slot, so we can avoid much allocation
  typedef struct {
    int16_t entries;
    int16_t bytes;
  } Sizes;
  Sizes sizes[256];
  memset(sizes, 0, sizeof(Sizes) * 256);

  // set up bit map for the new map
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    BIT_MAP_SET(bit_map, it.k[0], true);
    sizes[(uint8_t)it.k[0]].entries++;
    if (it.ksize > 1) {
      sizes[(uint8_t)it.k[0]].bytes += it.ksize + (sizeof(uint16_t) + sizeof(Val));
    }
  }

  // alloc memory
  // each child node is <= b in bytes, so they are all buckets
  Map* m = MAP_NEW(BIT_MAP_COUNT(bit_map));
  HAS_V(m) = HAS_V(b);
  m->h.v = b->h.v;
  memcpy(m->bit_map, bit_map, sizeof(uint64_t) * 4);
  for (int i = 0, j = 0; i < 256; i++) {
    if (sizes[i].entries) {
      m->slots[j] = (Header*)BUCKET_NEW(sizes[i].bytes + 2);
      m->slots[j]->size = 0; // use size for offset
      j++;
    }
  }

  // copy keys and values
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(b, &it)) {
    int index = BIT_MAP_INDEX(m->bit_map, it.k[0]);
    Bucket* child = (Bucket*)m->slots[index];
    assert(child);
    assert(IS_BUCKET(&child->h));
    if (it.ksize > 1) {
      BucketIter insert_it = {
        .pos = child->h.size,
        .ksize = it.ksize - 1,
        .k = it.k + 1,
        .v = it.v
      };
      BUCKET_ITER_SET_BACK(child, &insert_it);
      child->h.size = insert_it.pos;
    } else {
      HAS_V(child) = 1;
      child->h.v = *it.v;
    }
  }

  // terminate zeros
  for (int i = 0; i < m->h.size; i++) {
    Bucket* child = (Bucket*)m->slots[i];
    assert(child);
    assert(IS_BUCKET(&child->h));
    DATA_SET_SZ(child->data + child->h.size, 0);
    child->h.size += 2;
  }

  return m;
}

static void _bucket_debug(Bucket* b, bool bucket_as_binary) {
  printf("<Bucket#%p ref_count=%llu has_v=%d v=%lu data-size=%lld data=", b, b->h.vh.ref_count, HAS_V(b), b->h.v, b->h.size);
  if (bucket_as_binary) {
    printf("[");
    for (int i = 0; i < b->h.size; i++) {
      printf("%x ", (uint8_t)b->data[i]);
    }
    printf("]");
  } else {
    printf("{");
    for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it) ; BUCKET_ITER_NEXT(b, &it)) {
      printf("\"%.*s\": %lx(klass=%u,rc=%u), ", it.ksize, it.k, *it.v, (uint32_t)VAL_KLASS(*it.v), (uint32_t)VAL_REF_COUNT(*it.v));
    }
    printf("}");
  }
  printf(">\n");
}

static void _map_debug(Map* m, bool bucket_as_binary) {
  // assert(m->h.size == BIT_MAP_COUNT(m->bit_map));
  if (m->h.size != BIT_MAP_COUNT(m->bit_map)) {
    printf("Bad map: size=%lld bit_map_count=%d\n", m->h.size, BIT_MAP_COUNT(m->bit_map));
    exit(-1);
  }
  printf("<Map#%p ref_count=%llu has_v=%d v=%lu slots-size=%lld bitmap=[", m, m->h.vh.ref_count, HAS_V(m), m->h.v, m->h.size);
  for (int i = 0; i < 4; i++) {
    printf("%llx ", m->bit_map[i]);
  }
  printf("] slots={");
  for (int c = 0; c < 256; c++) {
    int i = BIT_MAP_INDEX(m->bit_map, (char)((unsigned char)c));
    if (i >= 0) {
      printf("'%c': %p, ", c, m->slots[i]);
    }
  }
  printf("}>\n");
  for (int i = 0; i < m->h.size; i++) {
    nb_dict_debug((Val)m->slots[i], bucket_as_binary);
  }
}

static void _dict_destructor(void* vdict) {
  Dict* d = (Dict*)vdict;
  if (HAS_V((Header*)d)) {
    RELEASE(d->h.v);
  }
  _node_release(d->root);
}

// NOTE: node is internal data structure, release/destroy/free are completely handled by _node_release()
static void _node_release(Header* h) {
  if (VAL_IS_PERM(h)) {
    return;
  }
  if (h->vh.ref_count > 1) {
    h->vh.ref_count--;
    return;
  }
  assert(h->vh.ref_count == 1);
  if (HAS_V(h)) {
    RELEASE(h->v);
  }
  if (IS_BUCKET(h)) {
    Bucket* b = (Bucket*)h;
    for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it) ; BUCKET_ITER_NEXT(b, &it)) {
      RELEASE(*it.v);
    }
  } else {
    assert(IS_MAP(h));
    Map* m = (Map*)h;
    for (int i = 0; i < m->h.size; i++) {
      _node_release(m->slots[i]);
    }
  }
  val_free(h);
}
