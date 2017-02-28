#include "compile.h"
#include <adt/dict.h>
#include <adt/sym-table.h>

static void _add_var(void* vars_table, Val context_name, Val var_name) {
  size_t context_size = (context_name ? nb_string_byte_size(context_name) : 0);
  size_t size = nb_string_byte_size(var_name);
  char name_buf[context_size + 2 + size];
  if (context_name) {
    memcpy(name_buf, nb_string_ptr(context_name), context_size);
  }
  name_buf[context_size] = name_buf[context_size + 1] = ':'; // ::var_name
  memcpy(name_buf + context_size + 2, nb_string_ptr(var_name), size);

  // TODO raise error for dup var name?
  nb_sym_table_get_set(vars_table, context_size + 2 + size, name_buf, NULL);
}

void sb_build_vars_dict(CompileCtx* ctx) {
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
