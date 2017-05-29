// compiler backend to transform AST then build result

#include "compile.h"
#include <stdlib.h>

#pragma mark ## exposed interfaces

Val sb_compile_main(Compiler* compiler) {
  Val err = sb_check_names_conflict(compiler->ast);
  if (err) {
    return err;
  }

  sb_inline_partial_references(compiler);
  sb_build_patterns_dict(compiler);
  sb_build_symbols(compiler);
  // TODO check if tokens in PEG matches tokens emitted from lexer

  for (Val lines = AT(compiler->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    int32_t iseq_start = Iseq.size(&compiler->iseq);
    if (IS_A(e, "Lex")) {
      Val lex_name = AT(e, 0);
      struct VarsTable* global_vars = &compiler->symbols->global_vars;
      struct VarsTable* vars_table = NULL;
      VarsTableMap.find(&compiler->symbols->local_vars_map, LITERALIZE(lex_name), &vars_table);
      Val err = sb_vm_lex_compile(&compiler->iseq, compiler->patterns_dict, global_vars, vars_table, AT(e, 1));
      if (err) {
        return err;
      } else {
        sb_compile_context_dict_insert(compiler, lex_name, 'l', iseq_start);
      }
    } else if (IS_A(e, "Peg")) {
      Val peg_name = AT(e, 0);
      Val err = sb_vm_peg_compile(&compiler->iseq, compiler->patterns_dict, &compiler->symbols->structs, AT(e, 1));
      if (err) {
        return err;
      } else {
        sb_compile_context_dict_insert(compiler, peg_name, 'p', iseq_start);
      }
    } else {
      // todo other instructions
    }
  }

  return VAL_NIL;
}
