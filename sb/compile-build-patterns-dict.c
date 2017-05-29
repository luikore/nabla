#include "compile.h"
#include <adt/dict.h>

// build patterns map, not checking loop dependency, because only regexp knows the AST structure
void sb_build_patterns_dict(Compiler* compiler) {
  Val lines = AT(compiler->ast, 0);

  for(; lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (e == VAL_UNDEF) {
      continue;
    }
    if (IS_A(e, "PatternIns")) {
      Val name = AT(e, 0);
      Val pattern = AT(e, 1);
      REPLACE(compiler->patterns_dict, nb_dict_insert(compiler->patterns_dict, nb_string_ptr(name), nb_string_byte_size(name), pattern));
    }
  }
}
