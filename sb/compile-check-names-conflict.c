#include "compile.h"
#include <adt/utils/mut-map.h>

typedef enum {PATTERN_NAME, VAR_NAME, STRUCT_NAME, LEX_NAME, PEG_NAME} UsedNameType;

typedef struct {
  UsedNameType type;
  Val name;
} UsedName;

static uint64_t _used_key_hash(UsedName k) {
  return k.type ^ val_hash(k.name);
}

static uint64_t _used_key_eq(UsedName k1, UsedName k2) {
  return k1.type == k2.type && val_eq(k1.name, k2.name);
}

MUT_MAP_DECL(UsedNames, UsedName, int, _used_key_hash, _used_key_eq);

Val sb_check_names_conflict(Val ast) {
  struct UsedNames used_names;
  UsedNames.init(&used_names);

  UsedName k;
  Val err = VAL_NIL;

  for (Val lines = AT(ast, 0); lines != VAL_NIL; lines = TAIL(lines)) {
    Val e = HEAD(lines);

    if (IS_A(e, "PatternIns")) {
      k.type = PATTERN_NAME;
    } else if (IS_A(e, "VarDecl")) {
      k.type = VAR_NAME;
    } else if (IS_A(e, "StructIns")) {
      k.type = STRUCT_NAME;
    } else if (IS_A(e, "Lex")) {
      k.type = LEX_NAME;
    } else if (IS_A(e, "Peg")) {
      k.type = PEG_NAME;
    } else {
      Val node_klass_name = klass_name(VAL_KLASS(e));
      err = nb_string_new_f("unrecognized node: %.*s", (int)nb_string_byte_size(node_klass_name), nb_string_ptr(node_klass_name));
      goto terminate;
    }

    k.name = AT(e, 0);
    int v;
    if (UsedNames.find(&used_names, k, &v)) {
      err = nb_string_new_f("name already used: %.*s", (int)nb_string_byte_size(k.name), nb_string_ptr(k.name));
      goto terminate;
    }
    UsedNames.insert(&used_names, k, 1);
  }

terminate:
  UsedNames.cleanup(&used_names);
  return err;
}
