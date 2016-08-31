#pragma once

// functions that are only used in compile

#include "sb.h"
#include <adt/dict.h>
#include <adt/sym-table.h>

Val sb_check_names_conflict(Val ast);
void sb_inline_partial_references(CompileCtx* ctx);
void sb_build_patterns_dict(CompileCtx* ctx);
void sb_build_vars_dict(CompileCtx* ctx);

#pragma mark ## some helper macros for compiling

#define S nb_string_new_literal_c

#define AT(node, i) nb_struct_get(node, i)

#define IS_A(node, ty) (klass_name(VAL_KLASS(node)) == S(ty))

#define TAIL(node) nb_cons_tail(node)

#define HEAD(node) nb_cons_head(node)

#define COMPILE_ERROR(M, ...) printf(M, ##__VA_ARGS__); _Exit(-1)

#define DECODE(ty, pc) ({ty res = *((ty*)pc); pc = (uint16_t*)((ty*)pc + 1); res;})

#define ENCODE(iseq, ty, data) do {\
  uint16_t args[sizeof(ty) / sizeof(uint16_t)];\
  ((ty*)args)[0] = data;\
  for (int _i = 0; _i < (sizeof(ty) / sizeof(uint16_t)); _i++) {\
    Iseq.push(iseq, args[_i]);\
  }\
} while (0)

#define ENCODE_META(iseq) do {\
  ENCODE(iseq, uint16_t, META);\
  ENCODE(iseq, uint32_t, 0);\
  ENCODE(iseq, void*, NULL);\
} while (0)

#define ENCODE_FILL_META(iseq, original_pos, data) do {\
  ((uint32_t*)Iseq.at(iseq, original_pos + 1))[0] = Iseq.size(iseq) - original_pos;\
  ((void**)Iseq.at(iseq, original_pos + 3))[0] = data;\
} while (0)

#pragma mark ## label management

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

#pragma mark ## symbol management

// vars are pre-allocated, so we generate the var table at compile time.

