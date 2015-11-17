#include "dict.h"
#include "box.h"
#include <ccut.h>

void dict_suite() {
  ccut_test("find element") {
    val_begin_check_memory();
    // stored elements:
    // alex, aha, znot, zhooz
    Val d = nb_dict_build_test_map_to_buckets();
    Val v;
    assert_true(nb_dict_find(d, "alex", 4, &v), "should contain alex");
    assert_eq(VAL_FROM_INT(0), v);
    assert_true(nb_dict_find(d, "aha", 3, &v), "should contain aha");
    assert_eq(VAL_FROM_INT(1), v);
    assert_true(nb_dict_find(d, "znot", 4, &v), "should contain znot");
    assert_eq(VAL_FROM_INT(2), v);
    assert_true(nb_dict_find(d, "zhooz", 5, &v), "should contain zhooz");
    assert_eq(VAL_FROM_INT(3), v);

    assert_false(nb_dict_find(d, "a", 1, &v), "should not contain a");
    assert_false(nb_dict_find(d, "z", 1, &v), "should not contain z");
    assert_false(nb_dict_find(d, "blex", 4, &v), "should not contain blex");

    RELEASE(d);
    val_end_check_memory();
  }

  ccut_test("bucket insert") {
    val_begin_check_memory();
    Val d = nb_dict_new();
    Val v;
    assert_eq(0, nb_dict_size(d));

    // insert to empty
    char* k = "Ty Lee";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(0)));
    assert_eq(1, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(0), v);

    // insert same k but different value
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(1)));
    assert_eq(1, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(1), v);

    // insert before
    k = "Azula";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(2)));
    assert_eq(2, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(2), v);

    // insert after
    k = "Zuko";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(3)));
    assert_eq(3, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(3), v);

    // search prev inserted key
    k = "Azula";
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(2), v);

    RELEASE(d);
    val_end_check_memory();
  }

  ccut_test("heap val retain in bucket") {
    val_begin_check_memory();

    Val d = nb_dict_new();
    char* keys[] = {"Main", "Lex", "Peg"};
    Val vs[] = {nb_box_new(0), nb_box_new(1), nb_box_new(2)};

    for (int i = 0; i < 3; i++) {
      REPLACE(d, nb_dict_insert(d, keys[i], strlen(keys[i]), vs[i]));
    }
    RELEASE(d);

    val_end_check_memory();
  }

  ccut_test("heap val retain in map") {
    val_begin_check_memory();

    // full bucket with keys "2", "4", "5", ...
    Val d = nb_dict_build_test_full_bucket();

    // burst
    REPLACE(d, nb_dict_insert(d, "3", 1, nb_box_new(0)));

    // map insert
    REPLACE(d, nb_dict_insert(d, "1", 1, nb_box_new(1)));

    RELEASE(d);

    val_end_check_memory();
  }

  ccut_test("bucket burst and map insert") {
    val_begin_check_memory();
    // full bucket with keys "2", "4", "5", ...
    Val d = nb_dict_build_test_full_bucket();
    char* k;
    Val v;

    assert_eq(2, nb_dict_size(d));
    // same key: there should be no burst
    k = "2";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(22)));
    assert_eq(2, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(22), v);

    // burst
    k = "3";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(3)));
    assert_eq(3, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(3), v);

    // map insert
    k = "1";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(1)));
    assert_eq(4, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(1), v);

    RELEASE(d);
    val_end_check_memory();
  }
}
