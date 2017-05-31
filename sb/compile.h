#pragma once

// functions that are only used in compile

#include "sb.h"
#include <adt/dict.h>

// sym table for structs data
#include <adt/sym-table.h>

Val sb_check_names_conflict(Val ast);
void sb_inline_partial_references(Compiler* ctx);
void sb_build_patterns_dict(Compiler* ctx);
// collect structs and vars info
void sb_build_symbols(Compiler* ctx);

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

#pragma mark ## symbols management

static Symbols* SYMBOLS_NEW() {
  Symbols* symbols = malloc(sizeof(Symbols));
  VarsTable.init(&symbols->global_vars, 5);
  VarsTableMap.init(&symbols->local_vars_map);
  StructsTable.init(&symbols->structs);
  return symbols;
}

static void SYMBOLS_DELETE(Symbols* symbols) {
  VarsTable.cleanup(&symbols->global_vars);

  VarsTableMapIter it;
  for (VarsTableMap.iter_init(&it, &symbols->local_vars_map); !VarsTableMap.iter_is_end(&it); VarsTableMap.iter_next(&it)) {
    VarsTable.cleanup(it.slot->v);
  }
  VarsTableMap.cleanup(&symbols->local_vars_map);

  // TODO undefine the klasses
  StructsTable.cleanup(&symbols->structs);
}

static int SYMBOLS_LOOKUP_VAR_ID(struct VarsTable* vars_table, Val name) {
  if (!vars_table) {
    return -1;
  }
  for (int i = 0; i < VarsTable.size(vars_table); i++) {
    Val n = *VarsTable.at(vars_table, i);
    if (nb_string_cmp(n, name) == 0) {
      return i;
    }
  }
  return -1;
}

#pragma mark ## helpers

static Val LITERALIZE(Val str) {
  return nb_string_new_literal(nb_string_byte_size(str), nb_string_ptr(str));
}
