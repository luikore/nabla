#pragma once

// functions that are only used in compile

#include "sb.h"
#include <adt/dict.h>

struct VmLexStruct;
struct VmPegStruct;
struct VmCallbackStruct;
struct VmRegexpStruct;

typedef struct VmLexStruct VmLex;
typedef struct VmPegStruct VmPeg;
typedef struct VmCallbackStruct VmCallback;
typedef struct VmRegexpStruct VmRegexp;

VmLex* sb_vm_lex_compile(void* arena, Val node, Spellbreak* spellbreak);
VmPeg* sb_vm_peg_compile(void* arena, Val node, Spellbreak* spellbreak);
VmCallback* sb_vm_callback_compile(void* arena, Val node, Spellbreak* spellbreak, Val lex_name);
VmRegexp* sb_vm_regexp_compile(void* arena, Val node, Spellbreak* spellbreak);
VmRegexp* sb_vm_regexp_from_string(Val node);

int64_t sb_vm_lex_exec(VmLex* lex, Spellbreak* ctx);
int64_t sb_vm_peg_exec(VmPeg* peg, Spellbreak* ctx);
int64_t sb_vm_callback_exec(VmCallback* callback, Spellbreak* ctx);
int64_t sb_vm_regexp_exec(VmRegexp* regexp, Spellbreak* ctx);

#pragma mark ## some helper macros for compiling

#define LOC(tok) nb_token_loc(tok)

#define S nb_string_new_literal_c

#define AT(node, i) nb_struct_get(node, i)

#define IS_A(node, ty) (klass_name(VAL_KLASS(node)) == S(ty))

#define TAIL(node) nb_cons_tail(node)

#define HEAD(node) nb_cons_head(node)

#define COMPILE_ERROR(M, ...) printf(M, ##__VA_ARGS__); _Exit(-1)
