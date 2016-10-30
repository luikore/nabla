#include "compile.h"
#include <adt/sym-table.h>

void sb_build_vars_table(CompileCtx* ctx) {
  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns") || IS_A(e, "Peg") || IS_A(e, "StructIns") || e == VAL_UNDEF) {

    } else if (IS_A(e, "Lex")) {
      Val context_name = AT(e, 0);
      Val rules = AT(e, 1);

      for (; rules != VAL_NIL; rules = TAIL(rules)) {
        Val rule = HEAD(rule);

        if (IS_A(rule, "BeginCallback")) {
          // BeginCallback[first_cb, rules]
          // TODO do not allow var decl in following rules

          Val first_cb = AT(rule, 0);
          Val stmts = AT(first_cb, 0);
          for (; stmts != VAL_NIL; stmts = TAIL(stmts)) {
            Val stmt = HEAD(stmts);
            if (IS_A(stmt, "VarDecl")) {
              Val var_name = AT(stmt, 0);
              int var_name_size = (int)nb_string_byte_size(var_name);
              const char* var_name_ptr = nb_string_ptr(var_name);
              int context_name_size = (int)nb_string_byte_size(context_name);
              const char* context_name_ptr = nb_string_ptr(context_name);
              int scoped_var_name_size = context_name_size + 1 + var_name_size;
              char scoped_var_name[scoped_var_name_size + 1];
              sprintf(scoped_var_name, "%.*s:%.*s", context_name_size, context_name_ptr, var_name_size, var_name_ptr);
              // allow re-definition of same var
              nb_sym_table_get_set(ctx->vars_table, scoped_var_name_size, scoped_var_name, NULL);
            }
          }
        }
      }

    } else if (IS_A(e, "VarDecl")) {
      Val var_name = AT(e, 0);
      int var_name_size = nb_string_byte_size(var_name);
      const char* var_name_ptr = nb_string_ptr(var_name);
      int scoped_var_name_size = 2 + var_name_size;
      char scoped_var_name[scoped_var_name_size + 1];
      sprintf(scoped_var_name, "::%.*s", var_name_size, var_name_ptr);
      nb_sym_table_get_set(ctx->vars_table, scoped_var_name_size, scoped_var_name, NULL);

    } else {
      COMPILE_ERROR("unrecognized node type");
    }
  }
}
