#pragma once

// Very simple generic dynamic array, must ensure elem types are POD

// Usage example:
//   #include "utils/mut-array.h"
//   MUT_ARRAY_DECL(Arr, int);
//   ...
//   struct Arr da;
//   Arr.init(&da, 0);
//   Arr.push(&da, 3);
//   Arr.reverse(&da);
//   Arr.at(&da, 0);
//   Arr.cleanup(&da);

// See bottom of this file for API

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MUT_ARRAY_DECL(MutArrayType, ElemType)\
  /* note typedef will cause symbol conflict */\
  struct MutArrayType {size_t size; size_t cap; ElemType* data;};\
  \
  static void MutArrayType##_MUT_ARRAY_init(struct MutArrayType* da, size_t init_cap) {\
    da->size = 0;\
    da->cap = (init_cap ? init_cap : 8);\
    da->data = malloc(sizeof(ElemType) * da->cap);\
  }\
  static void MutArrayType##_MUT_ARRAY_init_dup(struct MutArrayType* to, struct MutArrayType* from) {\
    to->size = from->size;\
    to->cap = from->cap;\
    to->data = malloc(sizeof(ElemType) * to->cap);\
    memcpy(to->data, from->data, sizeof(ElemType) * to->size);\
  }\
  \
  static void MutArrayType##_MUT_ARRAY_cleanup(struct MutArrayType* da) {\
    da->size = 0;\
    da->cap = 0;\
    free(da->data);\
    da->data = NULL;\
  }\
  \
  static void MutArrayType##_MUT_ARRAY_push(struct MutArrayType* da, ElemType e) {\
    if (da->size == da->cap) {\
      da->cap *= 2;\
      da->data = realloc(da->data, sizeof(ElemType) * da->cap);\
    }\
    da->data[da->size++] = e;\
  }\
  \
  static ElemType MutArrayType##_MUT_ARRAY_pop(struct MutArrayType* da) {\
    assert(da->size);\
    return da->data[--da->size];\
  }\
  static ElemType* MutArrayType##_MUT_ARRAY_top(struct MutArrayType* da) {\
    assert(da->size);\
    return da->data + (da->size - 1);\
  }\
  \
  static void MutArrayType##_MUT_ARRAY_remove(struct MutArrayType* da, size_t i) {\
    assert(da->size > i);\
    if (i + 1 != da->size) {\
      memmove(da->data + i, da->data + i + 1, sizeof(ElemType) * (da->size - i - 1));\
    }\
    da->size--;\
  }\
  \
  static void MutArrayType##_MUT_ARRAY_reverse(struct MutArrayType* da) {\
    for (int i = 0, j = da->size - 1; i < j; i++, j--) {\
      ElemType tmp = da->data[j];\
      da->data[j] = da->data[i];\
      da->data[i] = tmp;\
    }\
  }\
  \
  static size_t MutArrayType##_MUT_ARRAY_size(struct MutArrayType* da) {\
    return da->size;\
  }\
  \
  static ElemType* MutArrayType##_MUT_ARRAY_at(struct MutArrayType* da, size_t i) {\
    assert(da->size > i);\
    return da->data + i;\
  }\
  \
  static struct {\
    void (*init)(struct MutArrayType*, size_t);\
    void (*init_dup)(struct MutArrayType* to, struct MutArrayType* from);\
    void (*cleanup)(struct MutArrayType*);\
    void (*push)(struct MutArrayType*, ElemType);\
    ElemType (*pop)(struct MutArrayType*);\
    ElemType* (*top)(struct MutArrayType*);\
    void (*remove)(struct MutArrayType*, size_t);\
    void (*reverse)(struct MutArrayType*);\
    size_t (*size)(struct MutArrayType*);\
    ElemType* (*at)(struct MutArrayType*, size_t);\
  } const MutArrayType = {\
    .init = MutArrayType##_MUT_ARRAY_init,\
    .init_dup = MutArrayType##_MUT_ARRAY_init_dup,\
    .cleanup = MutArrayType##_MUT_ARRAY_cleanup,\
    .push = MutArrayType##_MUT_ARRAY_push,\
    .pop = MutArrayType##_MUT_ARRAY_pop,\
    .top = MutArrayType##_MUT_ARRAY_top,\
    .remove = MutArrayType##_MUT_ARRAY_remove,\
    .reverse = MutArrayType##_MUT_ARRAY_reverse,\
    .size = MutArrayType##_MUT_ARRAY_size,\
    .at = MutArrayType##_MUT_ARRAY_at\
  };\
