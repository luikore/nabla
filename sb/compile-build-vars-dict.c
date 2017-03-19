#include "compile.h"
#include <adt/dict.h>
#include <adt/sym-table.h>



void sb_build_vars_dict(Compiler* ctx) {
  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns") || IS_A(e, "Peg") || IS_A(e, "StructIns") || e == VAL_UNDEF) {

    } else if (IS_A(e, "Lex")) {
      Val context = AT(e, 0);
      for (Val rule_lines = AT(e, 1); rule_lines != VAL_NIL; rule_lines = TAIL(rule_lines)) {
        Val rule_line = HEAD(rule_lines);
        if (IS_A(rule_line, "BeginCallback")) {
          Val first_cb = AT(rule_line, 0);
          for (Val stmts = AT(first_cb, 0); stmts != VAL_NIL; stmts = TAIL(stmts)) {
            Val stmt = HEAD(stmts);
            if (IS_A(stmt, "VarDecl")) {
              Val var_name = AT(stmt, 0);
              _add_var(ctx->vars_table, context, var_name);
            }
          }
        }
      }

    } else if (IS_A(e, "VarDecl")) {
      Val var_name = AT(e, 0);
      _add_var(ctx->vars_table, VAL_NIL, var_name);
    } else {
      COMPILE_ERROR("unrecognized node type");
    }
  }
}
