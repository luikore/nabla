#pragma once

// functions that are only used in compile

#include "sb.h"
#include <adt/dict.h>

void sb_inline_partial_references(CompileCtx* ctx);
void sb_build_patterns_dict(CompileCtx* ctx);
void sb_build_vars_dict(CompileCtx* ctx);

#pragma mark ## some helper macros for compiling

#define LOC(tok) nb_token_loc(tok)

#define S nb_string_new_literal_c

#define AT(node, i) nb_struct_get(node, i)

#define IS_A(node, ty) (klass_name(VAL_KLASS(node)) == S(ty))

#define TAIL(node) nb_cons_tail(node)

#define HEAD(node) nb_cons_head(node)

#define COMPILE_ERROR(M, ...) printf(M, ##__VA_ARGS__); _Exit(-1)
