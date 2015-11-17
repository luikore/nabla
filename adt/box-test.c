#include "box.h"
#include <ccut.h>

void box_suite() {
  ccut_test("box test") {
    val_begin_check_memory();
    Val box = nb_box_new(3);
    assert_eq(true, nb_val_is_box(box));
    assert_eq(3, nb_box_get(box));
    nb_box_set(box, 4);
    assert_eq(4, nb_box_get(box));
    nb_box_delete(box);
    val_end_check_memory();
  }
}
