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
  bool compiled;
} SpellbreakMData;

typedef struct {
  int32_t pos, size, line, col;
  Val v; // VAL_UNDEF when no associated
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
  int32_t captures[20]; // begin: i*2, end: i*2+1
  struct TokenStream token_stream; // not copied

  Val lex_dict; // {name: lexer*}, from mdata
  Val peg_dict; // {name: parser*}, from mdata

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
  // intermediate data, cleared after constructed
  int64_t success;   // whether parse is success (TODO extend it for more error types)
  Val patterns_dict; // {"name": regexp_node}
  Val vars_dict;     // {"context:name": true}
} CompileCtx;

void sb_compile_main(CompileCtx* ctx);

#pragma mark ### exec functions

struct VmLexStruct;
struct VmPegStruct;
struct VmCallbackStruct;
struct VmRegexpStruct;

typedef struct VmLexStruct VmLex;
typedef struct VmPegStruct VmPeg;
typedef struct VmCallbackStruct VmCallback;
typedef struct VmRegexpStruct VmRegexp;

Val sb_vm_lex_exec(Spellbreak* sb, VmLex* lex, Val* err);
Val sb_vm_peg_exec(Spellbreak* sb, VmPeg* peg, int32_t token_pos, Val* err);
int64_t sb_vm_callback_exec(Spellbreak* sb, VmCallback* callback);
int64_t sb_vm_regexp_exec(Spellbreak* sb, VmRegexp* regexp);
