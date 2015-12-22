#pragma once

// functions that are only used in compile

#include "sb.h"
#include <adt/dict.h>
#include <adt/utils/mut-array.h>

void sb_inline_partial_references(CompileCtx* ctx);
void sb_build_patterns_dict(CompileCtx* ctx);
void sb_build_vars_dict(CompileCtx* ctx);

VmLex* sb_vm_lex_compile(CompileCtx* ctx, Val node, Val* err);
VmPeg* sb_vm_peg_compile(CompileCtx* ctx, Val node, Val* err);
VmRegexp* sb_vm_regexp_compile(CompileCtx* ctx, Val node);

MUT_ARRAY_DECL(Iseq, uint32_t);

#pragma mark ## some helper macros for compiling

#define LOC(tok) nb_token_loc(tok)

#define S nb_string_new_literal_c

#define AT(node, i) nb_struct_get(node, i)

#define IS_A(node, ty) (klass_name(VAL_KLASS(node)) == S(ty))

#define TAIL(node) nb_cons_tail(node)

#define HEAD(node) nb_cons_head(node)

#define COMPILE_ERROR(M, ...) printf(M, ##__VA_ARGS__); _Exit(-1)

static void ENCODE32(struct Iseq* iseq, uint32_t ins) {
  Iseq.push(iseq, ins);
}

static void ENCODE64(struct Iseq* iseq, uint64_t ins) {
  Iseq.push(iseq, 0);
  Iseq.push(iseq, 0);
  uint32_t* pc = Iseq.at(iseq, Iseq.size(iseq) - 2);
  ((uint64_t*)pc)[0] = ins;
}

#define DECODE(ty, pc) ({ty d = *((ty*)pc); pc++; d;})
