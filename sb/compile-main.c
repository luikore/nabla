// compiler backend to transform AST then build result

#include "compile.h"
#include <stdlib.h>

#pragma mark ## exposed interfaces

Val sb_compile_main(CompileCtx* ctx) {
  sb_inline_partial_references(ctx);
  sb_build_patterns_dict(ctx);
  sb_build_vars_dict(ctx);

  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "Lex")) {
      Val lex_name = AT(e, 0);
      Val err = sb_vm_lex_compile(ctx, AT(e, 1));
      // if (err == VAL_UNDEF) {
      //   REPLACE(ctx->lex_dict, nb_dict_insert(ctx->lex_dict, nb_string_ptr(lex_name), nb_string_byte_size(lex_name), res.fst));
      // }
      if (err) {
        return err;
      }
    } else if (IS_A(e, "Peg")) {
      Val peg_name = AT(e, 0);
      Val err = sb_vm_peg_compile(ctx, AT(e, 1));
      // if (err == VAL_UNDEF) {
      //   REPLACE(ctx->peg_dict, nb_dict_insert(ctx->peg_dict, nb_string_ptr(peg_name), nb_string_byte_size(peg_name), (Val)vm_peg));
      // }
      if (err) {
        return err;
      }
    }
  }
  return VAL_NIL;
}
