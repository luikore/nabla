#include "map.h"
#include <ccut.h>
#include "box.h"

static NbMapEachRet sum_cb(Val k, Val v, Val udata) {
  *((Val*)udata) += v;
  return NB_MAP_NEXT;
}

void map_suite() {
  ccut_test("map of 1 key") {
    val_begin_check_memory();

    Val map;
    Val v;

    map = nb_map_new();
    assert_true(!VAL_IS_IMM(map), "should not be immediate");
    assert_eq(KLASS_MAP, VAL_KLASS(map));

    REPLACE(map, nb_map_insert(map, VAL_NIL, VAL_TRUE));
    v = nb_map_find(map, VAL_NIL);
    assert_eq(VAL_TRUE, v);

    REPLACE(map, nb_map_insert(map, VAL_NIL, VAL_FALSE));
    v = nb_map_find(map, VAL_NIL);
    assert_eq(VAL_FALSE, v);

    REPLACE(map, nb_map_remove(map, VAL_NIL, &v));
    assert_eq(VAL_FALSE, v);
    v = nb_map_find(map, VAL_NIL);
    assert_eq(VAL_UNDEF, v);

    RELEASE(map);
    val_end_check_memory();
  }

  ccut_test("map of many keys and int values") {
    val_begin_check_memory();
    Val map = nb_map_new_i();
    int sz = 10000;
    for (long i = 0; i < sz; i++) {
      REPLACE(map, nb_map_insert(map, VAL_FROM_INT(i), i));
      Val v = nb_map_find(map, VAL_FROM_INT(i));
      if (i != v) {
        nb_map_debug(map);
        assert_true(false, "fail to find inserted value %ld, which has hash: %llu", i, val_hash(VAL_FROM_INT(i)));
      }
    }
    assert_eq(sz, nb_map_size(map));
    for (long i = 0; i < sz; i++) {
      Val v = nb_map_find(map, VAL_FROM_INT(i));
      if (i != v) {
        nb_map_debug(map);
        assert_true(false, "can not find inserted value %ld, which has hash: %llu", i, val_hash(VAL_FROM_INT(i)));
      }
    }
    RELEASE(map);
    val_end_check_memory();
  }

  ccut_test("iter empty map") {
    val_begin_check_memory();
    Val map = nb_map_new();

    Val sum = 0;
    NbMapEachRet ret = nb_map_each(map, (Val)&sum, sum_cb);
    assert_eq(NB_MAP_FIN, ret);
    assert_eq(0, sum);

    RELEASE(map);
    val_end_check_memory();
  }

  ccut_test("iter flat map") {
    val_begin_check_memory();
    Val map = nb_map_new_i();
    REPLACE(map, nb_map_insert(map, VAL_FROM_INT(0), 1));
    REPLACE(map, nb_map_insert(map, VAL_FROM_INT(1), 4));
    REPLACE(map, nb_map_insert(map, VAL_FROM_INT(2), 7));

    Val sum = 0;
    NbMapEachRet ret = nb_map_each(map, (Val)&sum, sum_cb);
    assert_eq(NB_MAP_FIN, ret);
    assert_eq(1 + 4 + 7, sum);

    RELEASE(map);
    val_end_check_memory();
  }

  ccut_test("deep tree iter") {
    val_begin_check_memory();
    Val map = nb_map_new_i();
    int expected_sum = 0;
    for (int i = 0; i < 400; i++) {
      REPLACE(map, nb_map_insert(map, VAL_FROM_INT(i), i));
      expected_sum += i;
    }

    Val sum = 0;
    NbMapEachRet ret = nb_map_each(map, (Val)&sum, sum_cb);
    assert_eq(NB_MAP_FIN, ret);
    assert_eq(expected_sum, sum);

    RELEASE(map);
    val_end_check_memory();
  }

}
