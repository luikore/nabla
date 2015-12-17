#include "cons.h"
#include <ccut.h>

void cons_suite() {
  ccut_test("cons node new") {
    void* arena = val_arena_new();

    Val node;
    node = nb_cons_anew(arena, VAL_FROM_INT(1), VAL_NIL);
    node = nb_cons_anew(arena, VAL_FROM_INT(2), node);
    node = nb_cons_anew(arena, VAL_FROM_INT(3), node);

    node = nb_cons_areverse(arena, node);

    int i = 1;
    for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr), i++) {
      assert_eq(VAL_FROM_INT(i), nb_cons_head(curr));
    }

    val_arena_delete(arena);
  }

  ccut_test("cons node new rev") {
    val_begin_check_memory();

    Val node;
    REPLACE(node, nb_cons_new_rev(VAL_NIL, VAL_FROM_INT(1)));
    REPLACE(node, nb_cons_new_rev(node, VAL_FROM_INT(2)));
    REPLACE(node, nb_cons_new_rev(node, VAL_FROM_INT(3)));

    int i = 1;
    for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr), i++) {
      assert_eq(VAL_FROM_INT(i), nb_cons_head(curr));
    }

    RELEASE(node);

    val_end_check_memory();
  }

  ccut_test("build list") {
    val_begin_check_memory();

    Val list = nb_cons_list(3, (Val[]){ VAL_FROM_INT(0), VAL_FROM_INT(1), VAL_FROM_INT(2) });
    Val tail = list;
    for (int i = 0; i < 3; i++) {
      Val head = nb_cons_head(tail);
      assert_eq(VAL_FROM_INT(2 - i), head);
      tail = nb_cons_tail(tail);
    }

    RELEASE(list);

    val_end_check_memory();
  }

}
