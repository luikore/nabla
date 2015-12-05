#pragma once

// (dict internal node)
// buckets are good at storing little number of long keys

#include "val.h"
#include "utils/str.h"

// NOTE: in a bucket, keys are sorted by lexical order
//       and, one key can not be suffix of another key
typedef struct {
  ValHeader h;  // flags: entry size (same as in Map), user1: release value
  Val v;
  // NOTE: sometimes it is acceptable to create a bucket larger than BUCKET_MAX for simpler implementation
  //       so we waste 8 bytes more for total bytes and save the ending zero
  uint64_t bytes;
  char data[];
} Bucket;

typedef struct {
  int pos; // pos of current entry = -1 if end
  int ksize;
  const char* k;
  Val* v;
} BucketIter;

static void DATA_SET_SZ(char* data, uint16_t sz) {
  *((uint16_t*)data) = sz;
}

static void DATA_SET_STR(char* data, int sz, const char* str) {
  assert((const char*)data != str);
  memcpy(data, str, sz);
}

static void DATA_SET_VAL(char* data, Val v) {
  *((Val*)data) = v;
}

static int DATA_SET(char* data, int pos, int sz, const char* str, Val v) {
  DATA_SET_SZ(data + pos, sz);
  pos += sizeof(uint16_t);
  DATA_SET_STR(data + pos, sz, str);
  pos += sz;
  DATA_SET_VAL(data + pos, v);
  return pos + sizeof(Val);
}

#define BUCKET_MAX_ENTRIES 30

#define BUCKET_ENTRIES(b) ((ValHeader*)(b))->flags

#define BUCKET_MAX_BYTES 16384

#define BUCKET_BYTES(b) ((Bucket*)(b))->bytes

#define BUCKET_ENTRY_BYTES(ksize) ((ksize) + (sizeof(uint16_t) + sizeof(Val)))

static BucketIter BUCKET_ITER_NEW(Bucket* b) {
  int ksize = *((uint16_t*)b->data);
  BucketIter it = {.ksize = ksize};
  if (it.ksize) {
    it.k = b->data + sizeof(uint16_t);
    it.v = (Val*)(it.k + it.ksize);
  }
  return it;
}

static bool BUCKET_ITER_IS_END(BucketIter* it) {
  return it->pos < 0;
}

static void BUCKET_ITER_NEXT(Bucket* b, BucketIter* it) {
  assert(!BUCKET_ITER_IS_END(it));

  uint16_t current_ksize = *((uint16_t*)(b->data + it->pos));
  it->pos += BUCKET_ENTRY_BYTES(current_ksize);
  if (it->pos == BUCKET_BYTES(b)) {
    it->pos = -1;
    return;
  }
  it->ksize = *((uint16_t*)(b->data + it->pos));
  it->k = b->data + it->pos + sizeof(uint16_t);
  it->v = (Val*)(it->k + it->ksize);
}

static Bucket* BUCKET_NEW(size_t bytes) {
  Bucket* b = val_alloc(sizeof(Bucket) + bytes);
  BUCKET_BYTES(b) = bytes;
  b->h.klass = KLASS_DICT_BUCKET;
  b->v = VAL_UNDEF;
  return b;
}

// new with single content k and v
static Bucket* BUCKET_NEW_KV(const char* k, size_t ksize, Val v) {
  assert(ksize > 0);

  // key sz + leading sz + val sz + terminate sz
  size_t size = BUCKET_ENTRY_BYTES(ksize);
  Bucket* b = BUCKET_NEW(size);
  BUCKET_ENTRIES(b) = 1;
  DATA_SET(b->data, 0, ksize, k, v);
  RETAIN(v);
  return b;
}

// prereq: k not in bucket
static Bucket* BUCKET_NEW_INSERT(Bucket* prev, const char* k, size_t ksize, Val v) {
  size_t new_size = BUCKET_BYTES(prev) + BUCKET_ENTRY_BYTES(ksize);
  Bucket* b = BUCKET_NEW(new_size);
  BUCKET_ENTRIES(b) = BUCKET_ENTRIES(prev) + 1;

  int pos = 0;
  int prev_cmp = -1;
  for (BucketIter it = BUCKET_ITER_NEW(prev); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(prev, &it)) {
    if (prev_cmp < 0) {
      int cmp = str_compare(it.ksize, it.k, ksize, k);
      assert(cmp);
      if (cmp > 0) {
        RETAIN(v);
        pos = DATA_SET(b->data, pos, ksize, k, v);
        prev_cmp = cmp;
      }
    }
    RETAIN(*it.v);
    pos = DATA_SET(b->data, pos, it.ksize, it.k, *it.v);
  }
  if (prev_cmp < 0) {
    RETAIN(v);
    pos = DATA_SET(b->data, pos, ksize, k, v);
  }
  assert(pos == new_size);
  return b;
}

static Bucket* BUCKET_NEW_2(Val v0, const char* k, size_t ksize, Val v) {
  Bucket* b = BUCKET_NEW_KV(k, ksize, v);
  b->v = v0;
  RETAIN(v0);
  RETAIN(v);
  return b;
}

static Bucket* BUCKET_DUP(Bucket* b) {
  size_t size = sizeof(Bucket) + BUCKET_BYTES(b);
  Bucket* r = val_dup(b, size, size);
  RETAIN(r->v);
  for (BucketIter it = BUCKET_ITER_NEW(r); !BUCKET_ITER_IS_END(&it); BUCKET_ITER_NEXT(r, &it)) {
    RETAIN(*it.v);
  }
  return r;
}

static void BUCKET_DESTROY(void* bucket) {
  Bucket* b = bucket;
  RELEASE(b->v);
  for (BucketIter it = BUCKET_ITER_NEW(b); !BUCKET_ITER_IS_END(&it) ; BUCKET_ITER_NEXT(b, &it)) {
    RELEASE(*it.v);
  }
}
