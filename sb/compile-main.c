// compiler backend to transform AST then build result

#include "compile.h"
#include <stdlib.h>

#pragma mark ## exposed interfaces

Val sb_compile_main(CompileCtx* ctx) {
  Val err = sb_check_names_conflict(ctx->ast);
  if (err) {
    return err;
  }

  sb_inline_partial_references(ctx);
  sb_build_patterns_dict(ctx);
  sb_build_vars_dict(ctx);
  // TODO check if tokens in PEG matches tokens emitted from lexer

  struct KlassRefs klass_refs;
  KlassRefs.init(&klass_refs, 5);

  // prepare klass defs (for arity check only)
  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    val e = HEAD(lines);
    if (IS_A(e, "StructIns")) {
      // 
    }
  }

  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    int32_t iseq_start = Iseq.size(&ctx->iseq);
    if (IS_A(e, "Lex")) {
      Val lex_name = AT(e, 0);
      Val err = sb_vm_lex_compile(&ctx->iseq, ctx->patterns_dict, ctx->vars_table, AT(e, 1), &klass_refs);
      if (err) {
        return err;
      } else {
        sb_compile_context_dict_insert(ctx, lex_name, 'l', iseq_start);
      }
    } else if (IS_A(e, "Peg")) {
      Val peg_name = AT(e, 0);
      Val err = sb_vm_peg_compile(&ctx->iseq, ctx->patterns_dict, AT(e, 1), &klass_refs);
      if (err) {
        return err;
      } else {
        sb_compile_context_dict_insert(ctx, peg_name, 'p', iseq_start);
      }
    } else {
      // todo other instructions
    }
  }

  // fill klass refs
  // TODO

  return VAL_NIL;
}
