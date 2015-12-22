#include "compile.h"
#include <adt/dict.h>

void sb_build_vars_dict(CompileCtx* ctx) {
  for (Val lines = AT(ctx->ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (IS_A(e, "PatternIns") || IS_A(e, "Lex") || IS_A(e, "Peg") || e == VAL_UNDEF) {
    } else if (IS_A(e, "VarDecl")) {
      Val var_name = AT(e, 0);
      size_t size = nb_string_byte_size(var_name);
      char name_buf[size + 2];
      name_buf[0] = name_buf[1] = ':'; // ::var_name
      memcpy(name_buf + 2, nb_string_ptr(var_name), size);
      REPLACE(ctx->vars_dict, nb_dict_insert(ctx->vars_dict, name_buf, size + 2, VAL_TRUE));
    } else {
      COMPILE_ERROR("unrecognized node type");
    }
  }
}
