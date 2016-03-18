#pragma once

#include <adt/token.h>
#include <adt/struct.h>
#include <adt/string.h>
#include <adt/cons.h>
#include <adt/utils/mut-array.h>

// klass data
// definition of a language
// NOTE:
//   actions are defined as instance methods
//   nodes are defined as structs inside the klass
typedef struct {
  // for initializing Ctx
  Val lex_dict; // perm {name: lexer*}
  Val peg_dict; // perm {name: parser*}
  int32_t vars_size;
  bool compiled;
} SpellbreakMData;

typedef struct {
  int32_t pos, size, line; // col can be computed with pos and lines index
  uint32_t ty;
  Val v; // associated value, VAL_UNDEF when no associated
} Token;

typedef struct {
  uint32_t name_str;
  int32_t token_pos; // start index of token stream
  const char* curr;  // start src ptr
  Val result;        // from yield or parse
} ContextEntry;

MUT_ARRAY_DECL(TokenStream, Token);
MUT_ARRAY_DECL(ContextStack, ContextEntry);
MUT_ARRAY_DECL(Vals, Val);

// instance data
// to make online-parser efficient, spellbreak should be able to be easily copied
// assume each instance consumes 200 bytes, 1k save points takes about ~200k memory, which is sufficient for in-screen
typedef struct {
  ValHeader h;
  const char* s; // src init pointer
  int64_t size;  // src size
  const char* curr; // curr src

  void* arena;
  int32_t capture_size;
  int32_t captures[20]; // begin: i*2, end: i*2+1
  struct TokenStream token_stream; // not copied

  Val lex_dict; // {name: lexer*}, from mdata
  Val peg_dict; // {name: parser*}, from mdata

  struct Vals stack;
  struct ContextStack context_stack;
  struct Vals vars; // for all globals and locals
} Spellbreak;

#define CAPTURE_BEGIN(c, i) (c)->captures[(i) * 2]
#define CAPTURE_END(c, i) (c)->captures[(i) * 2 + 1]

Val sb_bootstrap_ast(void* arena, uint32_t namespace);

void sb_init_module(void);

// returns the klass representing Spellbreak syntax
uint32_t sb_klass();

// returns syntax klass by generating from node
uint32_t sb_new_syntax(uint32_t name_str);

// read *.sb call-seq:
//   klass = sb_new_syntax(...);
//   ... define additional actions on it
//   sb = sb_new_sb();
//   ast = sb_parse(sb, src, size);
//   sb_syntax_compile(sb->arena, src, size, klass);
//   val_free(sb);
void sb_syntax_compile(void* arena, Val ast, uint32_t target_klass);

// NOTE separated for online-parsing
Spellbreak* sb_new(uint32_t klass);

// reset lexer status
void sb_reset(Spellbreak* s);

Spellbreak* sb_new_sb();

// parse call-seq:
//   sb = sb_new(klass);
//   ast = sb_parse(klass, src, size);
Val sb_parse(Spellbreak* sb, const char* src, int64_t size);

#pragma mark ### compile functions

typedef struct {
  Val ast; // we may transform ast

  Val lex_dict;
  Val peg_dict;

  void* arena;
  Val patterns_dict; // {"name": regexp_node}
  Val vars_dict;     // {"context:name": true}
} CompileCtx;

// returns compile error
Val sb_compile_main(CompileCtx* ctx);

#pragma mark ### vm functions

MUT_ARRAY_DECL(Iseq, uint16_t);

// updates ctx->lex_dict, returns err
Val sb_vm_lex_compile(CompileCtx* ctx, Val lex_node);
// returns {res, err}
ValPair sb_vm_lex_exec(Spellbreak* sb);

// updates ctx->peg_dict, returns err
Val sb_vm_peg_compile(CompileCtx* ctx, Val node);

void sb_vm_peg_decompile(struct Iseq* iseq, int32_t start, int32_t size);

// returns {res, err}
ValPair sb_vm_peg_exec(uint16_t* pc, void* arena, int32_t token_size, Token* tokens);

// updates iseq, returns err
Val sb_vm_regexp_compile(struct Iseq* iseq, void* arena, Val patterns_dict, Val node);

void sb_vm_regexp_decompile(struct Iseq* iseq, int32_t start, int32_t size);

Val sb_vm_regexp_from_string(struct Iseq* iseq, Val s);

// captures.size stored in captures[0]
// matched string size stored in captures[1]
bool sb_vm_regexp_exec(uint16_t* pc, int64_t size, const char* str, int32_t* captures);

bool sb_string_match(Val pattern_str, int64_t size, const char* str, int32_t* capture_size, int32_t* captures);
