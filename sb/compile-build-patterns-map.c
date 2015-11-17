#include "compile.h"
#include <adt/dict.h>

// build patterns map, not checking loop dependency, because only regexp knows the AST structure
void nb_spellbreak_build_patterns_map(PdlexNodeArena* arena, Val main_node, Spellbreak* spellbreak) {
  Val lines = AT(main_node, 0);

  for(; lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);
    if (e == VAL_UNDEF) {
      continue;
    }
    if (IS_A(e, "PatternIns")) {
      TokenNode* name_tok = (TokenNode*)AT(e, 0);
      Val pattern = AT(e, 1);
      REPLACE(spellbreak->patterns_dict, nb_dict_insert(spellbreak->patterns_dict, name_tok->loc.s, name_tok->loc.size, pattern));
    }
  }
}
