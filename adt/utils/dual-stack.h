#pragma once

// 2 stacks sharing one buffer, advantages:
// - less re-allocation
// - can manage stack frame
// - TODO if likely and fast-push with less checks

// Usage example:
//   #include "utils/dual-stack.h"
//   DUAL_STACK_DECL(DStack, FooType, BarType);
//   ...
//   struct DStack s;
//   DStack.init(&s);
//   DStack.lpush(&s, (FooType){});
//   DStack.rpush(&s, (BarType){});
//   FooType foo = DStack.lpop(&s);
//   BarType bar = DStack.rpop(&s);
//   FooType* foo = DStack.lat(&s, 1); // 0 for top
//   int sz = DStack.lsize(&s);
//   DStack.cleanup(&s);

// Stack frame management (frame is stored at stackl):
//   il = DStack.lsize(&s);
//   ir = DStack.rsize(&s);
//   frame_struct_ptr = DStack.push_frame(&s, frame_struct_size);

// Reset some stack frame:
//   DStack.restore(&s, il, ir);

// To iterate frames:
//   frame_struct_ptr = DStack.lat(&s, bp1);
//   older_bp1 = DStack.lat(&s, frame_struct_ptr->bp1);
//   ...

#include <stdint.h>

#define DUAL_STACK_DECL(DsType, LElemType, RElemType) \
  struct DsType {\
    size_t cap;\
    int ltop;\
    int rtop;\
    void* buf;\
  };\
  static void DsType##_DUAL_STACK_init(struct DsType* ds) {\
    ds->cap = (sizeof(LElemType) + sizeof(RElemType)) * 10;\
    ds->buf = malloc(ds->cap);\
    ds->ltop = 0;\
    ds->rtop = 0;\
  }\
  static void DsType##_DUAL_STACK_cleanup(struct DsType* ds) {\
    free(ds->buf);\
    ds->buf = NULL;\
  }\
  static void DsType##_DUAL_STACK_lpush(struct DsType* ds, LElemType e1) {\
    if (sizeof(LElemType) * (ds->ltop + 1) + sizeof(RElemType) * ds->rtop > ds->cap) {\
      ds->cap *= 2;\
      ds->buf = realloc(ds->buf, ds->cap);\
    }\
    ((LElemType*)ds->buf)[ds->ltop] = e1;\
    ds->ltop++;\
  }\
  static void DsType##_DUAL_STACK_rpush(struct DsType* ds, RElemType e2) {\
    if (sizeof(LElemType) * ds->ltop + sizeof(RElemType) * (ds->rtop + 1) > ds->cap) {\
      ds->cap *= 2;\
      ds->buf = realloc(ds->buf, ds->cap);\
    }\
    ((RElemType*)((char*)ds->buf + ds->cap))[-1 - ds->rtop] = e2;\
    ds->rtop++;\
  }\
  static void* DsType##_DUAL_STACK_push_frame(struct DsType* ds, size_t frame_struct_size) {\
    int ltop_incr = (frame_struct_size + sizeof(LElemType) - 1) / sizeof(LElemType);\
    if (sizeof(LElemType) * (ds->ltop + ltop_incr) + sizeof(RElemType) * ds->rtop > ds->cap) {\
      ds->cap *= 2;\
      ds->buf = realloc(ds->buf, ds->cap);\
    }\
    void* ptr = (LElemType*)ds->buf + ds->ltop;\
    ds->ltop += ltop_incr;\
    return ptr;\
  }\
  static void DsType##_DUAL_STACK_lpop(struct DsType* ds) {\
    ds->ltop--;\
  }\
  static void DsType##_DUAL_STACK_rpop(struct DsType* ds) {\
    ds->rtop--;\
  }\
  static void DsType##_DUAL_STACK_restore(struct DsType* ds, int ltop, int rtop) {\
    assert(ds->ltop >= ltop);\
    assert(ds->rtop >= rtop);\
    ds->ltop = ltop;\
    ds->rtop = rtop;\
  }\
  static void* DsType##_DUAL_STACK_lat(struct DsType* ds, int i) {\
    return ((LElemType*)ds->buf) + i;\
  }\
  static void* DsType##_DUAL_STACK_rat(struct DsType* ds, int i) {\
    return ((RElemType*)((char*)ds->buf + ds->cap)) - (1 + i);\
  }\
  static size_t DsType##_DUAL_STACK_lsize(struct DsType* ds) {\
    return ds->ltop;\
  }\
  static size_t DsType##_DUAL_STACK_rsize(struct DsType* ds) {\
    return ds->rtop;\
  }\
  static struct {\
    void (*init)(struct DsType* ds);\
    void (*cleanup)(struct DsType* ds);\
    void (*lpush)(struct DsType* ds, LElemType e1);\
    void (*rpush)(struct DsType* ds, RElemType e2);\
    void* (*push_frame)(struct DsType* ds, size_t frame_struct_size);\
    void (*lpop)(struct DsType* ds);\
    void (*rpop)(struct DsType* ds);\
    void (*restore)(struct DsType* ds, int ltop, int rtop);\
    void* (*lat)(struct DsType* ds, int i);\
    void* (*rat)(struct DsType* ds, int i);\
    size_t (*lsize)(struct DsType* ds);\
    size_t (*rsize)(struct DsType* ds);\
  } const DsType = {\
    .init = DsType##_DUAL_STACK_init,\
    .cleanup = DsType##_DUAL_STACK_cleanup,\
    .lpush = DsType##_DUAL_STACK_lpush,\
    .rpush = DsType##_DUAL_STACK_rpush,\
    .push_frame = DsType##_DUAL_STACK_push_frame,\
    .lpop = DsType##_DUAL_STACK_lpop,\
    .rpop = DsType##_DUAL_STACK_rpop,\
    .restore = DsType##_DUAL_STACK_restore,\
    .lat = DsType##_DUAL_STACK_lat,\
    .rat = DsType##_DUAL_STACK_rat,\
    .lsize = DsType##_DUAL_STACK_lsize,\
    .rsize = DsType##_DUAL_STACK_rsize\
  };
