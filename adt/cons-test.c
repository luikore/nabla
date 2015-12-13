#include "cons.h"
#include <ccut.h>

void cons_suite() {
  ccut_test("cons node new") {
    void* arena = val_arena_new();

    Val node;
    node = nb_cons_anew(arena, VAL_FROM_INT(1), VAL_NIL);
    node = nb_cons_anew(arena, VAL_FROM_INT(2), node);
    node = nb_cons_anew(arena, VAL_FROM_INT(3), node);

    node = nb_cons_arena_reverse(arena, node);

    int i = 1;
    for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr), i++) {
      assert_eq(VAL_FROM_INT(i), nb_cons_head(curr));
    }

    val_arena_delete(arena);
  }
}