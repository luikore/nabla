// compiler backend to transform AST then build result

#include "compile.h"
#include <stdlib.h>
#include <adt/box.h>

#define ERROR(info) // todo

static void _compile_pattern_ins(void* arena, Val e, Spellbreak* spellbreak) {
  TokenNode* name = (TokenNode*)AT(e, 0);
  Val pattern = AT(e, 1);
  Val res;
  if (IS_A(pattern, "Regexp") || IS_WRAPPER(pattern)) {
    res = nb_node_to_val(pattern);
  } else {
    ERROR("expect regexp or string");
  }

  REPLACE(spellbreak->patterns_dict, nb_dict_insert(spellbreak->patterns_dict, name->loc.s, name->loc.size, res));
}

#pragma mark ## exposed interfaces

// returns pointer, not boxed
Spellbreak* nb_spellbreak_compile_main(void* arena, Val node) {
  Spellbreak* spellbreak = nb_spellbreak_new();

  nb_spellbreak_inline_partial_references(arena, node);
  nb_spellbreak_build_patterns_map(arena, node, spellbreak);

  for (Val lines = AT(node, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns")) {
      _compile_pattern_ins(arena, e, spellbreak);
    } else if (IS_A(e, "VarDecl")) {
      TokenNode* name_tok = (TokenNode*)AT(e, 0);
      size_t size = name_tok->loc.size + 1;
      char name_buf[size];
      name_buf[0] = ':';
      memcpy(name_buf + 1, name_tok->loc.s, size - 1);
      REPLACE(spellbreak->vars_dict, nb_dict_insert(spellbreak->vars_dict, name_buf, size, VAL_TRUE));
    } else if (IS_A(e, "Lex") || IS_A(e, "Peg") || e == VAL_UNDEF) {
    } else {
      ERROR("unrecognized node type");
    }
  }

  for (Val lines = AT(node, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "Lex")) {
      TokenNode* name_tok = (TokenNode*)AT(e, 0);
      // printf("lex: %.*s\n", name_tok->loc.size, name_tok->loc.s);
      VmLex* vm_lex = nb_vm_lex_compile(arena, AT(e, 1), spellbreak);
      Val lex = nb_box_new((uint64_t)vm_lex);
      REPLACE(spellbreak->lex_dict, nb_dict_insert(spellbreak->lex_dict, name_tok->loc.s, name_tok->loc.size, lex));
    } else if (IS_A(e, "Peg")) {
      TokenNode* name_tok = (TokenNode*)AT(e, 0);
      // printf("peg: %.*s\n", name_tok->loc.size, name_tok->loc.s);
      VmPeg* vm_peg = nb_vm_peg_compile(arena, AT(e, 1), spellbreak);
      Val peg = nb_box_new((uint64_t)vm_peg);
      REPLACE(spellbreak->peg_dict, nb_dict_insert(spellbreak->peg_dict, name_tok->loc.s, name_tok->loc.size, peg));
    }
  }

  return spellbreak;
}
