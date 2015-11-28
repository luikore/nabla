#pragma once

// dict-maps are good at storing large number of short keys

#include "val.h"

typedef struct {
  ValHeader h; // flags: size of slots, user1: should not release value (TODO), user2: has value
  Val v;       // unified access for Map/Bucket/Dict
  uint64_t bit_map[4]; // the HAMT way of encoding, slots size = popcnt(bit_map), and they are sorted by the byte
  Val slots[];         // each slot can be either map/bucket/value
} Map;

#pragma mark ## map bit operation

static void BIT_MAP_SET(uint64_t* bit_map, int c, bool set) {
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

static int BIT_MAP_COUNT(uint64_t* bit_map) {
  int r = 0;
  for (int i = 0; i < 4; i++) {
    r += NB_POPCNT(bit_map[i]);
  }
  return r;
}

// return -1 if not found
static int BIT_MAP_INDEX(uint64_t* bit_map, int c) {
  uint8_t d = (uint8_t)c;
  int index = 0;
  for (int i = 0; i < 4; i++) {
    if (d < 64) {
      if (bit_map[i] & (1ULL << d)) {
        index += NB_POPCNT(bit_map[i] << (64 - d)); // count bits lower than it
        break;
      } else {
        // not found
        return -1;
      }
    } else {
      index += NB_POPCNT(bit_map[i]);
      d -= 64;
    }
  }
  return index;
}

// alternate func to BIT_MAP_INDEX, which still returns index
static bool BIT_MAP_HIT(uint64_t* bit_map, char c, int* index) {
  uint8_t d = (uint8_t)c;
  *index = 0;
  for (int i = 0; i < 4; i++) {
    if (d < 64) {
      *index += NB_POPCNT(bit_map[i] << (64 - d)); // count bits lower than it
      return (bit_map[i] & (1ULL << d));
    } else {
      *index += NB_POPCNT(bit_map[i]);
      d -= 64;
    }
  }
  return false; // impossible here
}

#pragma mark ## map

#define MAP_SIZE(m) ((ValHeader*)(m))->flags

static Map* MAP_NEW(size_t sz) {
  Map* m = val_alloc(sizeof(Map) + sizeof(Val) * sz);
  MAP_SIZE(m) = sz;
  m->h.klass = KLASS_DICT_MAP;
  m->v = VAL_UNDEF;
  return m;
}

static Map* MAP_DUP(Map* m) {
  size_t size = sizeof(Map) + sizeof(Val) * MAP_SIZE(m);
  Map* r = val_dup(m, size, size);
  for (int i = 0; i < MAP_SIZE(m); i++) {
    RETAIN(r->slots[i]);
  }
  RETAIN(r->v);
  return r;
}

static void MAP_DESTROY(void* node) {
  Map* m = node;
  for (int i = 0; i < MAP_SIZE(m); i++) {
    RELEASE(m->slots[i]);
  }
  RELEASE(m->v);
}

static Val* MAP_SLOT(Map* m, int c) {
  int i = BIT_MAP_INDEX(m->bit_map, c);
  return m->slots + i;
}
