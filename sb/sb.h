#pragma once

#include <adt/token.h>
#include <adt/struct.h>
#include <adt/string.h>
#include <adt/cons.h>
#include <adt/utils/mut-array.h>
#include <adt/utils/mut-map.h>
#include <adt/sym-table.h>

// How the lex-peg machine works:
// lex VM generates token stream, and in the "end {}" callback, there can be a :peg call which may reduce tokens.
// if there is no :peg call, the lex machine will produce an array of tokens.
// with the :peg calls, after all tokens feeded, an AST is placed at the head of the token array.
//
// lex machine is not limited to :peg parsers, there can be easier or faster way to process the token results,
// so lex machine can produce other results with :yield call, which generates a token with a type (matching current
// context), and a value in it.
//
// peg machine parses based on the type of token slot. a token type can be string segment type,
// or a context name. the token can be generated from peg parsing, or :yield call.

MUT_ARRAY_DECL(Iseq, uint16_t);

// symbols
MUT_ARRAY_DECL(VarsTable, Val); // array of name ids, usually the var number is not big, so don't need a sym-table
MUT_MAP_DECL(VarsTableMap, Val, struct VarsTable*, val_hash, val_eq); // {context_name: vars_table}
typedef struct {
  uint32_t klass_id;
  int32_t min_elems;
  int32_t max_elems;
} StructsTableValue;
MUT_MAP_DECL(StructsTable, Val, StructsTableValue, val_hash, val_eq);
typedef struct {
  struct VarsTable global_vars;
  struct VarsTableMap local_vars_map;
  struct StructsTable structs;
} Symbols;

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

  Symbols* symbols;

  bool compiled;
} SpellbreakKlassData;

typedef struct {
  int32_t pos, size, line; // col can be computed with pos and lines index
  uint32_t ty; // name str
  Val v; // associated value, VAL_UNDEF when no associated
} Token;

// one entry in context stack
typedef struct {
  uint32_t name_str;
  uint32_t var_size;
  int32_t token_pos; // start index of token stream
  const char* s;  // start src ptr
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
  int32_t captures[20]; // begin: i*2, end: i*2+1, lex VM will need to prepare captures values with this data.
  struct TokenStream token_stream; // not copied

  // following fields are copied from SpellbreakKlassData
  Val context_dict;
  struct Iseq* iseq;

  // TODO use dual stack
  struct Vals stack; // all globals and locals are numbered and put into the top of stack
  struct ContextStack context_stack;

  int64_t global_vars_size;
  Val global_vars[];
} Spellbreak;

#define CAPTURE_BEGIN(c, i) (c)->captures[(i) * 2]
#define CAPTURE_END(c, i) (c)->captures[(i) * 2 + 1]

Val sb_bootstrap_ast(uint32_t namespace);

void sb_init_module(void);

// returns the klass representing Spellbreak syntax
uint32_t sb_klass();

// returns syntax klass id with name
uint32_t sb_syntax_new(uint32_t name_str);

// To generate a class as a parser for a new syntax:
//   klass = sb_syntax_new(...);
//   ... define additional actions on it, actions can be invoked in lexer callback
//   sb = sb_new(spellbreak_klass);
//   ast = sb_parse(sb, src, size);
//   sb_syntax_generate(ast, klass);
//   val_free(sb);
void sb_syntax_generate(Val ast, uint32_t target_klass);

// NOTE separated for online-parsing
Spellbreak* sb_new(uint32_t klass);

// reset lexer status
void sb_reset(Spellbreak* s);

// parse call-seq:
//   sb = sb_new(klass);
//   ast = sb_parse(klass, src, size);
Val sb_parse(Spellbreak* sb, const char* src, int64_t size);

#pragma mark ### compile functions

typedef struct {
  // intermediate data structures
  Val ast; // we may transform ast
  Val patterns_dict;   // {"name": regexp_node}, built from ast
  uint32_t namespace_id;

  // compile results
  Symbols* symbols;
  Val context_dict;
  struct Iseq iseq;
} Compiler;

// returns compile error
Val sb_compile_main(Compiler* compiler);

#pragma mark ### operations to context_dict

void sb_compile_context_dict_insert(Compiler* compiler, Val name, char kind, int32_t offset);

int32_t sb_compile_context_dict_find(Val context_dict, Val name, char kind);

#pragma mark ### vm functions

// updates iseq, returns err
Val sb_vm_lex_compile(struct Iseq* iseq, Val patterns_dict, struct VarsTable* global_vars, struct VarsTable* local_vars, Val node);

// returns {res, err}
ValPair sb_vm_lex_exec(Spellbreak* sb);

// stmts: list of statements
// peg_mode: if set to true, will forbid variables (TODO local variables still makes it pure)
// see compile.h for label table type
Val sb_vm_callback_compile(struct Iseq* iseq, Val stmts, int32_t terms_size, void* label_table,
                           void* structs_table, struct VarsTable* global_vars, struct VarsTable* local_vars);

void sb_vm_callback_decompile(uint16_t* pc);

// replaces stack, returns err | TODO need stack from position
ValPair sb_vm_callback_exec(uint16_t* pc, struct Vals* stack, Val* global_vars, Val* vars);

// updates iseq, returns err
Val sb_vm_peg_compile(struct Iseq* iseq, Val patterns_dict, struct StructsTable* structs_table, Val node);

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
