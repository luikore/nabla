// compiler backend to transform AST then build result

#include "compile.h"
#include <stdlib.h>

static void _compile_pattern_ins(void* arena, Val e, Spellbreak* spellbreak) {
  Val name = AT(e, 0);
  Val pattern = AT(e, 1);
  Val res;
  if (IS_A(pattern, "Regexp") || IS_A(pattern, "String")) {
    res = pattern;
  } else {
    ERROR("expect regexp or string");
  }

  REPLACE(spellbreak->patterns_dict, nb_dict_insert(spellbreak->patterns_dict, nb_token_loc(name)->s, nb_token_loc(name)->size, res));
}

#pragma mark ## exposed interfaces

// returns pointer, not boxed
void sb_compile_main(CompileCtx* ctx, Val node) {
  sb_inline_partial_references(ctx->arena, node);
  sb_build_patterns_map(ctx, node);

  for (Val lines = AT(node, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns")) {
      _compile_pattern_ins(arena, e, spellbreak);
    } else if (IS_A(e, "VarDecl")) {
      Val name_tok = AT(e, 0);
      size_t size = nb_token_loc(name_tok)->size + 1;
      char name_buf[size];
      name_buf[0] = ':';
      memcpy(name_buf + 1, nb_token_loc(name_tok)->s, size - 1);
      REPLACE(spellbreak->vars_dict, nb_dict_insert(spellbreak->vars_dict, name_buf, size, VAL_TRUE));
    } else if (IS_A(e, "Lex") || IS_A(e, "Peg") || e == VAL_UNDEF) {
    } else {
      ERROR("unrecognized node type");
    }
  }

  for (Val lines = AT(node, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "Lex")) {
      Val name_tok = AT(e, 0);
      // printf("lex: %.*s\n", nb_token_loc(name_tok)->size, nb_token_loc(name_tok)->s);
      VmLex* vm_lex = sb_vm_lex_compile(arena, AT(e, 1), spellbreak);
      Val lex = nb_box_new((uint64_t)vm_lex);
      REPLACE(spellbreak->lex_dict, nb_dict_insert(spellbreak->lex_dict, nb_token_loc(name_tok)->s, nb_token_loc(name_tok)->size, lex));
    } else if (IS_A(e, "Peg")) {
      Val name_tok = AT(e, 0);
      // printf("peg: %.*s\n", nb_token_loc(name_tok)->size, nb_token_loc(name_tok)->s);
      VmPeg* vm_peg = sb_vm_peg_compile(arena, AT(e, 1), spellbreak);
      Val peg = nb_box_new((uint64_t)vm_peg);
      REPLACE(spellbreak->peg_dict, nb_dict_insert(spellbreak->peg_dict, nb_token_loc(name_tok)->s, nb_token_loc(name_tok)->size, peg));
    }
  }

  return spellbreak;
}
