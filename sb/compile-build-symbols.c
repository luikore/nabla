#include "compile.h"
#include <adt/sym-table.h>

static Val _literalize(Val str) {
  return nb_string_new_literal(nb_string_byte_size(str), nb_string_ptr(str));
}

static void _add_var(VarsTable* vars_table, Val var_name) {
  var_name = _literalize(var_name);

  for (int i = 0; i < VarsTable.size(vars_table); i++) {
    if (VarsTable.at(vars_table, i) == var_name) {
      // todo resumable error handling
      fatal_err("variable already defined!");
    }
  }
  VarsTable.push(vars_table, var_name);
}

static void _def_local_var(Symbols* symbols, Val context_name, Val var_name) {
  context_name = _literalize(context_name);

  VarsTable* vars_table = NULL;
  if (!VarsTableMap.find(&symbols->local_vars_map, context_name, &vars_table)) {
    vars_table = malloc(sizeof(VarsTable));
    VarsTable.init(vars_table);
    VarsTableMap.insert(&symbols->local_vars_map, context_name, vars_table);
  }
  _add_var(vars_table, var_name);
}

void sb_build_symbols(Compiler* compiler) {
  for (Val lines = AT(compiler->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns") || IS_A(e, "Peg") || e == VAL_UNDEF) {
      // skip

    } else if (IS_A(e, "StructIns")) {
      // StructIns[name, name.arg*]
      Val struct_name = _literalize(AT(e, 0));
      Val elems = AT(e, 1);
      if (StructsTable.find(compiler->structs_table, struct_name, NULL)) {
        // todo resumable error handling
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
        Val elem = _literalize(HEAD(elems_list));
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
              _def_local_var(&compiler->symbols->local_vars_map, context_name, var_name);
            }
          }
        }
      }

    } else if (IS_A(e, "VarDecl")) {
      Val var_name = AT(e, 0);
      _add_var(&compiler->symbols->global_vars, var_name);

    } else {
      COMPILE_ERROR("unrecognized node type");
    }
  }
}
