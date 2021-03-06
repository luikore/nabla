#include "struct.h"
#include "string.h"
#include "klass.h"
#include <ccut.h>

static ValPair foo_sum(Val self, int32_t argc, Val* argv) {
  assert(2 == argc);
  int64_t res = VAL_TO_INT(argv[0]) + VAL_TO_INT(argv[1]);
  return (ValPair){VAL_FROM_INT(res), VAL_NIL};
}

static bool klass_prepared = false;

void struct_suite() {
  if (!klass_prepared) {
    NbStructField fields[] = {
      {.matcher = VAL_UNDEF, .field_id = val_strlit_new_c("foo")},
      {.matcher = VAL_UNDEF, .field_id = val_strlit_new_c("bar")},
      {.matcher = VAL_UNDEF, .field_id = val_strlit_new_c("baz")}
    };
    uint32_t klass = nb_struct_def(nb_string_new_literal_c("Foo"), 0, 3, fields);
    klass_def_method_v(klass, val_strlit_new_c("sum"), 2, 2, foo_sum, true);
    klass_prepared = true;
  }

  ccut_test("defined klass") {
    uint32_t klass_id = klass_find(nb_string_new_literal_c("Foo"), 0);
    assert_true(klass_id, "klass should be defined");
    Klass* k = (Klass*)klass_val(klass_id);
    assert_eq(3, Fields.size(&k->fields));
  }

  ccut_test("new") {
    val_begin_check_memory();

    uint32_t klass_id = klass_find(nb_string_new_literal_c("Foo"), 0);
    Val attrs[] = {VAL_TRUE, VAL_FALSE, VAL_NIL};
    Val st = nb_struct_new(klass_id, 3, attrs);
    assert_eq(VAL_TRUE, nb_struct_get(st, 0));
    assert_eq(VAL_FALSE, nb_struct_get(st, 1));
    assert_eq(VAL_NIL, nb_struct_get(st, 2));

    ValPair res = val_send(st, val_strlit_new_c("sum"), 2, (Val[]){VAL_FROM_INT(3), VAL_FROM_INT(4)});
    assert_eq(VAL_FROM_INT(7), res.fst);
    assert_eq(VAL_NIL, res.snd);

    RELEASE(st);
    val_end_check_memory();
  }

  ccut_test("set field") {
    val_begin_check_memory();

    uint32_t klass_id = klass_find(nb_string_new_literal_c("Foo"), 0);
    Val attrs[] = {VAL_TRUE, VAL_FALSE, VAL_NIL};
    Val st = nb_struct_new(klass_id, 3, attrs);

    REPLACE(st, nb_struct_set(st, 1, VAL_FROM_INT(2)));
    assert_eq(VAL_TRUE, nb_struct_get(st, 0));
    assert_eq(VAL_FROM_INT(2), nb_struct_get(st, 1));
    assert_eq(VAL_NIL, nb_struct_get(st, 2));

    RELEASE(st);
    val_end_check_memory();
  }

  ccut_test("mset") {
    val_begin_check_memory();

    uint32_t klass_id = klass_find(nb_string_new_literal_c("Foo"), 0);
    Val attrs[] = {VAL_TRUE, VAL_FALSE, VAL_NIL};
    Val st = nb_struct_new(klass_id, 3, attrs);

    nb_struct_mset(st, 1, VAL_FROM_INT(2));
    assert_eq(VAL_TRUE, nb_struct_get(st, 0));
    assert_eq(VAL_FROM_INT(2), nb_struct_get(st, 1));
    assert_eq(VAL_NIL, nb_struct_get(st, 2));

    RELEASE(st);
    val_end_check_memory();
  }
}
