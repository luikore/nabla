#include "map-cola.h"
#include <ccut.h>
#include "box.h"

void map_cola_suite() {
  ccut_test("collision array insert") {
    val_begin_check_memory();

    Val k1 = VAL_NIL;
    Val k2 = VAL_TRUE;
    Val k3 = VAL_FROM_INT(3);
    Val v1 = VAL_FROM_INT(0);
    Val v2 = nb_box_new(1); // the only heap value
    Val v3 = VAL_FALSE;
    Val v;
    Cola* c;

    {
      Kv kv = {.k = k1, .v = v1};
      c = COLA_NEW2(kv, k2, v2, false);
      bool size_increased;
      REPLACE(AS_VAL(c), (Val)COLA_INSERT(c, k3, v3, &size_increased));
      assert_true(size_increased, "should increase size");
    }

    assert_eq(3, SIZE(c));

    assert_true(COLA_FIND(c, k1, &v), "should contain k1");
    assert_true(VAL_EQ(v1, v), "should contain v1");
    RELEASE(v);

    assert_true(COLA_FIND(c, k2, &v), "should contain k2");
    assert_true(VAL_EQ(v2, v), "should contain v2");
    RELEASE(v);

    assert_true(COLA_FIND(c, k3, &v), "should contain k3");
    assert_true(VAL_EQ(v3, v), "should contain v3");
    RELEASE(v);

    RELEASE((Val)c);
    RELEASE(v2);

    val_end_check_memory();
  }

  ccut_test("collision array remove") {
    val_begin_check_memory();

    Val k1 = VAL_NIL;
    Val k2 = VAL_TRUE;
    Val v1 = VAL_FROM_INT(0);
    Val v2 = nb_box_new(1); // the only heap value
    Val k3 = VAL_FROM_INT(3);
    Val v3 = VAL_FALSE;
    Val v;
    Cola* c;

    {
      Kv kv = {.k = k1, .v = v1};
      c = COLA_NEW2(kv, k2, v2, false);
      bool size_changed;
      REPLACE(AS_VAL(c), (Val)COLA_INSERT(c, k3, v3, &size_changed));

      Slot new_slot = COLA_REMOVE(c, k2, &v, &size_changed);
      assert_true(size_changed, "should change size");
      REPLACE(AS_VAL(c), new_slot.h);
    }

    assert_eq(2, SIZE(c));
    assert_true(COLA_FIND(c, k1, &v), "should contain k1");
    assert_true(VAL_EQ(v1, v), "should contain v1");
    RELEASE(v);

    assert_true(!COLA_FIND(c, k2, &v), "should not contain k2");

    assert_true(COLA_FIND(c, k3, &v), "should contain k3");
    assert_true(VAL_EQ(v3, v), "should contain v3");
    RELEASE(v);

    RELEASE((Val)c);
    RELEASE(v2);
    val_end_check_memory();
  }

}
