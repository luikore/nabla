#pragma once

#include <adt/token.h>
#include <adt/struct.h>
#include <adt/string.h>
#include <adt/cons.h>
#include <adt/utils/mut-array.h>

MUT_ARRAY_DECL(Iseq, uint16_t);

// klass data
// definition of a language
// NOTE:
//   actions are defined as instance methods
//   nodes are defined as structs inside the klass
typedef struct {
  struct Iseq iseq;

  // {kind('l' for lex, 'p' for peg) + name => pc_in_int}
  // it can be built with either:
  // - parsing spellbreak file
  // - loading a bytecode dump
  // using a dict instead of mutable map reduces code complexity of rebuilding a map keyed by string & arena allocation optimizations
  Val context_dict;

  void* vars_table;
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

  int32_t capture_size;
  int32_t captures[20]; // begin: i*2, end: i*2+1
  struct TokenStream token_stream; // not copied

  // following fields are copied from SpellbreakMData
  Val context_dict;
  struct Iseq* iseq;

  // TODO use dual stack
  struct Vals stack; // all globals and locals are numbered and put into the top of stack
  struct ContextStack context_stack;
} Spellbreak;

#define CAPTURE_BEGIN(c, i) (c)->captures[(i) * 2]
#define CAPTURE_END(c, i) (c)->captures[(i) * 2 + 1]

Val sb_bootstrap_ast(uint32_t namespace);

void sb_init_module(void);

// returns the klass representing Spellbreak syntax
uint32_t sb_klass();

// returns syntax klass id with name
uint32_t sb_new_syntax(uint32_t name_str);

// To generate a class as a parser for a new syntax:
//   klass = sb_new_syntax(...);
//   ... define additional actions on it, actions can be invoked in lexer callback
//   sb = sb_new(spellbreak_klass);
//   ast = sb_parse(sb, src, size);
//   sb_syntax_compile(ast, klass);
//   val_free(sb);
void sb_syntax_compile(Val ast, uint32_t target_klass);

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
  Val patterns_dict;   // {"name": regexp_node}, built from ast
  void* vars_table;    // symtable of {"context:name": position_in_stack}, for global vars, key is "::name"
  void* structs_table; // dict of {"name": arity}, for re-definition check and arity check
  uint32_t namespace_id;

  // the following fields will be passed to runtime Spellbreak

  Val context_dict;
  struct Iseq iseq;
} CompileCtx;

// returns compile error
Val sb_compile_main(CompileCtx* ctx);

#pragma mark ### operations to context_dict

void sb_compile_context_dict_insert(CompileCtx* ctx, Val name, char kind, int32_t offset);

int32_t sb_compile_context_dict_find(Val context_dict, Val name, char kind);

#pragma mark ### vm functions

// updates iseq, returns err
Val sb_vm_lex_compile(struct Iseq* iseq, Val patterns_dict, void* vars_table, Val node);

// returns {res, err}
ValPair sb_vm_lex_exec(Spellbreak* sb);

// stmts: list of statements
// peg_mode: if set to true, will forbid variables (TODO local variables still makes it pure)
// see compile.h for label table type
Val sb_vm_callback_compile(struct Iseq* iseq, Val stmts, int32_t terms_size, void* label_table, bool is_peg, void* ctx);

void sb_vm_callback_decompile(uint16_t* pc);

// replaces stack, returns err | TODO need stack from position
ValPair sb_vm_callback_exec(uint16_t* pc, struct Vals* stack, uint32_t bp);

// updates iseq, returns err
Val sb_vm_peg_compile(struct Iseq* iseq, Val patterns_dict, void* structs_table, Val node);

void sb_vm_peg_decompile(uint16_t* pc);

// returns {res, err}
// klass is for looking up pure functions (they can use a nil as receiver)
ValPair sb_vm_peg_exec(uint16_t* pc, int32_t token_size, Token* tokens);

// updates iseq, returns err
Val sb_vm_regexp_compile(struct Iseq* iseq, Val patterns_dict, Val node);

void sb_vm_regexp_decompile(uint16_t* pc);

Val sb_vm_regexp_from_string(struct Iseq* iseq, Val s);

// captures.size stored in captures[0]
// matched string size stored in captures[1]
bool sb_vm_regexp_exec(uint16_t* pc, int64_t str_size, const char* str, int32_t* captures);

bool sb_string_match(Val pattern_str, int64_t str_size, const char* str, int32_t* capture_size, int32_t* captures);
