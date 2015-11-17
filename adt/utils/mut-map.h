#pragma once

// Simple mutable map implementation with zero overhead (not storing func pointers in the map instance)
// can be used for memmory checker, ... etc
// keys and values are treated as POD

// usage example:
//   #include "utils/mut-map.h"
//   MUT_MAP_DECL(MyMap, KeyType, ValueType, hash_func, eq_func); // declare MyMap with type info
//   ...
//   struct MyMap m;
//   MyMap.init(&m);
//   MyMap.insert(&m, key, value);
//   MyMap.cleanup(&m);

// iter usage example:
//   MyMapIter i;
//   for (MyMap.iter_init(&i, &m); !MyMap.iter_is_end(&i); MyMap.iter_next(&i)) {
//     i.slot->k;
//     i.slot->v;
//   }

// See bottom of this file for API

// NOTE this simple hash map does not take charge of memory management of k and v
// NOTE this is a bit tricky since the type and static object are both named MyMap

// TODO (not very important): use pool to make slot allocation faster
// TODO use val_alloc to track memory

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#pragma mark ### helpers (for internal use only)

#define MUT_MAP_SLOT_GET_NEXT(p) \
  (__typeof__(p))(p->tag & (~1ULL))

#define MUT_MAP_SLOT_SET_NEXT(p, next) \
  p->tag = ((p->tag & 1ULL) | (uintptr_t)next)

#define MUT_MAP_SLOT_GET_HIT(p) \
  p->tag & 1ULL

#define MUT_MAP_SLOT_SET_HIT(p, hit) \
  if (hit) {\
    p->tag |= 1ULL;\
  } else {\
    p->tag &= (~1ULL);\
  }

#define MUT_MAP_CAP(mm) \
  (1ULL << mm->bits)

#define MUT_MAP_FIND_SLOT(mm, k, hash_func) \
  mm->slots + (hash_func(k) & ((1ULL << mm->bits) - 1))

#pragma mark ### interface

#define MUT_MAP_DECL(MutMapType, KeyType, ValueType, hash_func, eq_func) \
  typedef struct { uintptr_t tag; KeyType k; ValueType v; } MutMapType##Slot;\
  struct MutMapType { size_t size; size_t bits; MutMapType##Slot* slots; };\
  typedef struct { size_t i; struct MutMapType* mm; MutMapType##Slot* slot; } MutMapType##Iter;\
  \
  static void MutMapType##_MUT_MAP_init(struct MutMapType* mm) {\
    mm->size = 0;\
    mm->bits = 4;\
    mm->slots = malloc(sizeof(MutMapType##Slot) * MUT_MAP_CAP(mm));\
    memset(mm->slots, 0, sizeof(MutMapType##Slot) * MUT_MAP_CAP(mm));\
  }\
  \
  static size_t MutMapType##_MUT_MAP_size(struct MutMapType* mm) {\
    return mm->size;\
  }\
  \
  static bool MutMapType##_MUT_MAP_find(struct MutMapType* mm, KeyType k, ValueType* v) {\
    for (MutMapType##Slot* slot = MUT_MAP_FIND_SLOT(mm, k, hash_func); slot; slot = MUT_MAP_SLOT_GET_NEXT(slot)) {\
      if (!MUT_MAP_SLOT_GET_HIT(slot)) {\
        break;\
      }\
      if (eq_func(k, slot->k)) {\
        if (v) {\
          *v = slot->v;\
        }\
        return true;\
      }\
    }\
    return false;\
  }\
  \
  static void MutMapType##_MUT_MAP_rehash(struct MutMapType* mm, size_t bits);\
  static void MutMapType##_MUT_MAP_insert(struct MutMapType* mm, KeyType k, ValueType v) {\
    if (MUT_MAP_CAP(mm) * 8 < mm->size * 10) { /*factor of 0.8*/\
      MutMapType##_MUT_MAP_rehash(mm, mm->bits + 1);\
    }\
    \
    MutMapType##Slot* slot = MUT_MAP_FIND_SLOT(mm, k, hash_func);\
    if (MUT_MAP_SLOT_GET_HIT(slot)) {\
      for (MutMapType##Slot* col_slot = slot; col_slot; col_slot = MUT_MAP_SLOT_GET_NEXT(col_slot)) {\
        if (eq_func(k, col_slot->k)) {\
          col_slot->v = v;\
          return;\
        }\
      }\
      \
      /* add collision chain */\
      MutMapType##Slot* col_slot = malloc(sizeof(MutMapType##Slot));\
      *col_slot = *slot;\
      slot->k = k;\
      slot->v = v;\
      MUT_MAP_SLOT_SET_NEXT(slot, col_slot);\
      mm->size++;\
    } else {\
      slot->k = k;\
      slot->v = v;\
      MUT_MAP_SLOT_SET_HIT(slot, true);\
      mm->size++;\
    }\
  }\
  \
  static void MutMapType##_MUT_MAP_remove(struct MutMapType* mm, KeyType k) {\
    MutMapType##Slot* slot = MUT_MAP_FIND_SLOT(mm, k, hash_func);\
    if (!MUT_MAP_SLOT_GET_HIT(slot)) {\
      return;\
    }\
    \
    if (eq_func(k, slot->k)) {\
      MutMapType##Slot* next_slot = MUT_MAP_SLOT_GET_NEXT(slot);\
      if (next_slot) {\
        *slot = *next_slot;\
        free(next_slot);\
      } else {\
        MUT_MAP_SLOT_SET_HIT(slot, false);\
      }\
      mm->size--;\
      return;\
    }\
    \
    MutMapType##Slot* prev_slot = slot;\
    for (MutMapType##Slot* col_slot = MUT_MAP_SLOT_GET_NEXT(slot); col_slot; col_slot = MUT_MAP_SLOT_GET_NEXT(col_slot)) {\
      if (eq_func(k, col_slot->k)) {\
        MUT_MAP_SLOT_SET_NEXT(prev_slot, MUT_MAP_SLOT_GET_NEXT(col_slot));\
        free(col_slot);\
        mm->size--;\
        break;\
      }\
      prev_slot = col_slot;\
    }\
  }\
  \
  static void MutMapType##_MUT_MAP_cleanup(struct MutMapType* mm) {\
    size_t cap = MUT_MAP_CAP(mm);\
    for (size_t i = 0; i < cap; i++) {\
      MutMapType##Slot* slot = mm->slots + i;\
      if (MUT_MAP_SLOT_GET_HIT(slot)) {\
        MutMapType##Slot* col_slot = MUT_MAP_SLOT_GET_NEXT(slot);\
        while (col_slot) {\
          MutMapType##Slot* next = MUT_MAP_SLOT_GET_NEXT(col_slot);\
          free(col_slot);\
          col_slot = next;\
        }\
      }\
    }\
    free(mm->slots);\
    mm->slots = NULL;\
  }\
  \
  static void MutMapType##_MUT_MAP_iter_init(MutMapType##Iter* it, struct MutMapType* mm) {\
    it->i = 0;\
    it->mm = mm;\
    size_t cap = MUT_MAP_CAP(mm);\
    for (; it->i < cap; it->i++) {\
      MutMapType##Slot* slot = mm->slots + it->i;\
      if (MUT_MAP_SLOT_GET_HIT(slot)) {\
        it->slot = slot;\
        return;\
      }\
    }\
    it->slot = NULL;\
  }\
  \
  static void MutMapType##_MUT_MAP_iter_next(MutMapType##Iter* it) {\
    MutMapType##Slot* next = MUT_MAP_SLOT_GET_NEXT(it->slot);\
    if (next) {\
      it->slot = next;\
      return;\
    }\
    \
    size_t cap = MUT_MAP_CAP(it->mm);\
    for (it->i++; it->i < cap; it->i++) {\
      MutMapType##Slot* slot = it->mm->slots + it->i;\
      if (MUT_MAP_SLOT_GET_HIT(slot)) {\
        it->slot = slot;\
        return;\
      }\
    }\
    it->slot = NULL;\
  }\
  \
  static bool MutMapType##_MUT_MAP_iter_is_end(MutMapType##Iter* it) {\
    return it->slot == NULL;\
  }\
  \
  static void MutMapType##_MUT_MAP_rehash(struct MutMapType* mm, size_t bits) {\
    struct MutMapType new_mm = {\
      .size = 0,\
      .bits = bits,\
      .slots = malloc(sizeof(MutMapType##Slot) * (1ULL << bits))\
    };\
    memset(new_mm.slots, 0, sizeof(MutMapType##Slot) * (1ULL << bits));\
    \
    MutMapType##Iter it;\
    for (MutMapType##_MUT_MAP_iter_init(&it, mm); !MutMapType##_MUT_MAP_iter_is_end(&it); MutMapType##_MUT_MAP_iter_next(&it)) {\
      MutMapType##_MUT_MAP_insert(&new_mm, it.slot->k, it.slot->v);\
    }\
    MutMapType##_MUT_MAP_cleanup(mm);\
    *mm = new_mm;\
  }\
  \
  static struct {\
    void (*init)(struct MutMapType* mm);\
    size_t (*size)(struct MutMapType* mm);\
    bool (*find)(struct MutMapType* mm, KeyType k, ValueType* v);\
    void (*rehash)(struct MutMapType* mm, size_t bits);\
    void (*insert)(struct MutMapType* mm, KeyType k, ValueType v);\
    void (*remove)(struct MutMapType* mm, KeyType k);\
    void (*cleanup)(struct MutMapType* mm);\
    void (*iter_init)(MutMapType##Iter* it, struct MutMapType* mm);\
    void (*iter_next)(MutMapType##Iter* it);\
    bool (*iter_is_end)(MutMapType##Iter* it);\
  } const MutMapType = {\
    .init = MutMapType##_MUT_MAP_init,\
    .size = MutMapType##_MUT_MAP_size,\
    .find = MutMapType##_MUT_MAP_find,\
    .rehash = MutMapType##_MUT_MAP_rehash,\
    .insert = MutMapType##_MUT_MAP_insert,\
    .remove = MutMapType##_MUT_MAP_remove,\
    .cleanup = MutMapType##_MUT_MAP_cleanup,\
    .iter_init = MutMapType##_MUT_MAP_iter_init,\
    .iter_next = MutMapType##_MUT_MAP_iter_next,\
    .iter_is_end = MutMapType##_MUT_MAP_iter_is_end\
  };
