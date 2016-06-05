#pragma once
#include <adt/utils/dual-stack.h>

// provide label management functions
// lstack stores num => offset
// rstack stores offsets that references labels that require translation
// label num is stored into the iseq, and then we go through the whole iseq to concretize num to offsets.

DUAL_STACK_DECL(Labels, int, int);

static int LABEL_NEW_NUM(struct Labels* labels) {
  int i = Labels.lsize(labels);
  Labels.lpush(labels, 0);
  return i;
}

static void LABEL_DEF(struct Labels* labels, int label_num, int offset) {
  ((int*)Labels.lat(labels, label_num))[0] = offset;
}

static void LABEL_REF(struct Labels* labels, int offset) {
  Labels.rpush(labels, offset);
}

static void LABEL_TRANSLATE(struct Labels* labels, struct Iseq* iseq) {
  int refs_size = Labels.rsize(labels);
  for (int i = 0; i < refs_size; i++) {
    int* j = Labels.rat(labels, i);
    int32_t* ptr = (int32_t*)Iseq.at(iseq, *j);
    ptr[0] = *((int*)Labels.lat(labels, ptr[0]));
  }
}
