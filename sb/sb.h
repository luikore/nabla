#pragma once

#include "node.h"
#include <adt/string.h>

// represent a language
typedef struct {
  // intermediate data, clear after parse
  int64_t success;  // whether parse is success (TODO extend it for more error types)
  Val patterns_dict; // {"name": box(regexp*)}
  Val actions_dict;  // {"fname/arity": box(function_pointer*)}
  Val vars_dict;     // {"context:name": true}

  // for initializing Ctx
  Val lex_dict; // {name: box(lexer*)}, copied when initializing
  Val peg_dict; // {name: box(parser*)}, copied when initializing
} Spellbreak;

// the structure of token stream:
//   [e1, e2, e3, ...]
// memoize table:
//   array of tok_stream_size * rule_size
// parser checking types:
//   if type is token or context, check the entry in token stream
//   else check memoize table (uint32_t for each slot)
//   else parse
typedef struct {
  int32_t token;
  int32_t pos;
  int32_t size;
  int32_t line;
  Val v; // VAL_UNDEF when not containing value
         // NOTE no need to save a little bit space for token stream, the memoize table takes much more
} TokenStreamEntry;

// running Ctx
typedef struct {
  const char* s;
  size_t pos;
  size_t size;

  PdlexNodeMeta* meta;
  PdlexNodeArena* arena;
  Val lex_dict; // {name: box(lexer*)}, copied when initializing
  Val peg_dict; // {name: box(parser*)}, copied when initializing

  Val captures[10];
  Val context_stack;
  Val context_start_pos_stack;

  TokenStreamEntry* token_stream;
  size_t token_stream_size;
  size_t token_stream_cap;

  Val vars[]; // vars array for all globals and locals
} Ctx;

Spellbreak* nb_spellbreak_new(void);

void* nb_spellbreak_find_action(Spellbreak* sb, char* name, int32_t name_size, int8_t arity);

void nb_spellbreak_delete(Spellbreak* sb);

// return boot strap ast
Val nb_spellbreak_bootstrap();

// returns pointer of spellbreak, not boxed
Spellbreak* nb_spellbreak_compile_main(PdlexNodeMeta* meta, PdlexNodeArena* arena, Val main_node);
