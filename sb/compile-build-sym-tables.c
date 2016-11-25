#include "compile.h"
#include <adt/sym-table.h>

void sb_build_sym_tables(CompileCtx* ctx) {
  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns") || IS_A(e, "Peg") || e == VAL_UNDEF) {
      // skip

    } else if (IS_A(e, "StructIns")) {
      // StructIns[name, name.arg*]
      Val struct_name = AT(e, 0);
      Val elems = AT(e, 1);
      if (StructsTable.find(ctx->structs_table, struct_name, NULL)) {
        fatal_err("re-definition of struct: %.*s", (int)nb_string_byte_size(struct_name), nb_string_ptr(struct_name));
      }
      StructsTableValue v = {
        .min_elems = 0,
        .max_elems = 0,
      };
      // TODO support splat elements
      Val elems_list;
      for (elems_list = elems; elems_list != VAL_NIL; elems_list = TAIL(elems_list)) {
        v.min_elems++;
        v.max_elems++;
      }
      NbStructField fields[v.min_elems];
      elems_list = elems;
      for (int i = v.min_elems - 1; i >= 0; i--) {
        Val elem = HEAD(elems_list);
        fields[i] = (NbStructField){.matcher = VAL_UNDEF, .field_id = VAL_TO_STR(elem)};
        elems_list = TAIL(elems_list);
      }
      nb_struct_def(struct_name, ctx->namespace_id, v.min_elems, fields);
      StructsTable.insert(ctx->structs_table, struct_name, v);

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
