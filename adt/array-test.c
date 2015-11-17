#include "array.h"
#include <ccut.h>

void array_suite() {
  // the following get tests use prepared arrays, to validate that get operations behaves correctly

  ccut_test("get on array of depth 0W") {
    val_begin_check_memory();

    Val a = nb_array_build_test_10();
    assert_eq(10, nb_array_size(a));
    for (long i = 0; i < 10; i++) {
      assert_eq(i, VAL_TO_INT(nb_array_get(a, i)));
    }
    RELEASE(a);
    val_end_check_memory();
  }

  ccut_test("get on array of depth 1W") {
    val_begin_check_memory();

    Val a = nb_array_build_test_546();
    assert_eq(546, nb_array_size(a));
    for (long i = 0; i < 546; i++) {
      if (i != VAL_TO_INT(nb_array_get(a, i))) {
        assert_true(false, "%ld != %lld", i, VAL_TO_INT(nb_array_get(a, i)));
      }
    }
    RELEASE(a);
    val_end_check_memory();
  }

  // element removal tests
  // todo move structure-validations tests into array.c

  ccut_test("removal of elements in root") {
    val_begin_check_memory();

    Val a = nb_array_build_test_10();
    REPLACE(a, nb_array_remove(a, 10));
    assert_eq(10, nb_array_size(a));

    REPLACE(a, nb_array_remove(a, 0));
    assert_eq(10 - 1, nb_array_size(a));
    assert_eq(1, VAL_TO_INT(nb_array_get(a, 0)));

    REPLACE(a, nb_array_remove(a, 1));
    assert_eq(10 - 2, nb_array_size(a));
    assert_eq(1, VAL_TO_INT(nb_array_get(a, 0)));
    assert_eq(3, VAL_TO_INT(nb_array_get(a, 1)));

    RELEASE(a);
    val_end_check_memory();
  }

  ccut_test("removal of single element array") {
    val_begin_check_memory();

    Val a = nb_array_build_test_10();
    for (long i = 0; i < 10; i++) {
      REPLACE(a, nb_array_remove(a, 0));
    }
    assert_eq(0, nb_array_size(a));

    RELEASE(a);
    val_end_check_memory();
  }

  ccut_test("removal of elements in leaf") {
    val_begin_check_memory();

    Val a = nb_array_build_test_546();
    REPLACE(a, nb_array_remove(a, 2));
    assert_eq(546 - 1, nb_array_size(a));
    assert_eq(0, VAL_TO_INT(nb_array_get(a, 0)));
    assert_eq(1, VAL_TO_INT(nb_array_get(a, 1)));
    assert_eq(3, VAL_TO_INT(nb_array_get(a, 2)));

    RELEASE(a);
    val_end_check_memory();
  }

  // new

  ccut_test("array new test") {
    val_begin_check_memory();

    Val a = nb_array_new(4, VAL_TRUE, VAL_FALSE, VAL_NIL, VAL_UNDEF);
    assert_eq(4, nb_array_size(a));
    assert_eq(VAL_TRUE, nb_array_get(a, 0));
    assert_eq(VAL_FALSE, nb_array_get(a, 1));
    assert_eq(VAL_NIL, nb_array_get(a, 2));
    assert_eq(VAL_UNDEF, nb_array_get(a, 3));

    RELEASE(a);
    val_end_check_memory();
  }

  // integration tests

  ccut_test("append") {
    val_begin_check_memory();

    long sz = 300;
    Val a = nb_array_new_empty();
    for (long i = 0; i < sz; i++) {
      REPLACE(a, nb_array_set(a, i, VAL_FROM_INT(i)));
      if (i != VAL_TO_INT(nb_array_get(a, i))) {
        assert_true(false, "%ld != %lld", i, VAL_TO_INT(nb_array_get(a, i)));
      }
    }
    for (long i = 0; i < sz; i++) {
      if (i != VAL_TO_INT(nb_array_get(a, i))) {
        assert_true(false, "%ld != %lld", i, VAL_TO_INT(nb_array_get(a, i)));
      }
    }
    RELEASE(a);
    val_end_check_memory();
  }

  ccut_test("set data on slice") {
    val_begin_check_memory();

    Val a = nb_array_build_test_546();
    REPLACE(a, nb_array_slice(a, 3, 10));
    assert_eq(10, nb_array_size(a));

    Val b = nb_array_set(a, 0, VAL_FROM_INT(-1));
    assert_eq(10, nb_array_size(b));
    assert_eq(-1, VAL_TO_INT(nb_array_get(b, 0)));

    Val c = nb_array_set(a, 10, VAL_FROM_INT(10));
    assert_eq(11, nb_array_size(c));
    assert_eq(10, VAL_TO_INT(nb_array_get(c, 10)));

    RELEASE(c);
    RELEASE(b);
    RELEASE(a);
    val_end_check_memory();
  }

  ccut_test("set 3-layered data") {
    val_begin_check_memory();
    Val a = nb_array_new_empty();
    for (long i = 0; i < 70000; i++) {
      REPLACE(a, nb_array_set(a, i, VAL_FROM_INT(i)));
    }
    assert_eq(70000, nb_array_size(a));

    RELEASE(a);
    val_end_check_memory();
  }
}
