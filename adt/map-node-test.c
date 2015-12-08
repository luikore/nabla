#include "map-node.h"
#include <ccut.h>
#include "box.h"

static void create_kvs(Val* ks, Val* vs) {
  ks[0] = VAL_FROM_INT(0);
  vs[0] = nb_box_new(0);
  ks[1] = nb_box_new(1);
  vs[1] = VAL_FROM_INT(1);
  ks[2] = VAL_FROM_INT(2);
  vs[2] = nb_box_new(2);
}

static void destroy_kvs(Val* ks, Val* vs) {
  RELEASE(vs[0]);
  RELEASE(ks[1]);
  RELEASE(vs[2]);
}

void map_node_suite() {
  ccut_test("node bm insert") {
    val_begin_check_memory();
    Val ks[3], vs[3];
    create_kvs(ks, vs);

    Kv kv = {.k = ks[0], .v = vs[0]};
    NodeBm nb = NODE_NEW2(kv, val_hash(ks[1]), ks[1], vs[1], W, false);
    Node* n = nb.n;
    // preserve slot for k2
    nb = NODE_BM_INSERT(nb, val_hash(ks[2]));
    REPLACE(AS_VAL(n), (Val)nb.n);

    assert_true(NODE_FIND_SLOT(n, nb.b, val_hash(ks[0])), "should contain ks[0]");
    assert_true(NODE_FIND_SLOT(n, nb.b, val_hash(ks[1])), "should contain ks[1]");

    // fill slot with k2, v2
    Slot* slot = NODE_FIND_SLOT(n, nb.b, val_hash(ks[2]));
    assert_true(slot, "should contain ks[2]");
    RETAIN(ks[2]);
    RETAIN(vs[2]);
    slot->kv.k = ks[2];
    slot->kv.v = vs[2];

    RELEASE(n);
    destroy_kvs(ks, vs);
    val_end_check_memory();
  }

  ccut_test("node dup replace") {
    val_begin_check_memory();
    Val ks[3], vs[3];
    create_kvs(ks, vs);

    // prepare { k0 => v0, k1 => v2 }
    Kv kv = {.k = ks[0], .v = vs[0]};
    NodeBm nb = NODE_NEW2(kv, val_hash(ks[1]), ks[1], vs[1], W, false);
    Slot replacement = {.kv = {.k = ks[1], .v = vs[2]}};
    RETAIN(replacement.kv.k);
    RETAIN(replacement.kv.v);
    Node* n = NODE_DUP_REPLACE(nb.n, nb.b, val_hash(ks[1]), replacement);
    REPLACE(AS_VAL(nb.n), (Val)n);
    assert_eq(2, SIZE(n));

    Slot* slot = NODE_FIND_SLOT(n, nb.b, val_hash(ks[0]));
    assert_true(slot, "should contain ks[0]");
    assert_true(val_eq(slot->kv.v, vs[0]), "should contain vs[0]");

    slot = NODE_FIND_SLOT(n, nb.b, val_hash(ks[1]));
    assert_true(slot, "should contain ks[1]");
    assert_true(val_eq(slot->kv.v, vs[2]), "should contain vs[2]");

    RELEASE(n);
    destroy_kvs(ks, vs);
    val_end_check_memory();
  }

  ccut_test("node remove to kv") {
    val_begin_check_memory();
    Val ks[3], vs[3];
    create_kvs(ks, vs);

    Kv kv = {.k = ks[0], .v = vs[0]};
    NodeBm nb = NODE_NEW2(kv, val_hash(ks[1]), ks[1], vs[1], W, false);

    NodeBm new_nb = NODE_BM_REMOVE(nb, val_hash(ks[1]));
    assert_eq(1, SIZE(new_nb.n));
    assert_true(NODE_FIND_SLOT(new_nb.n, new_nb.b, val_hash(ks[0])), "should contain ks[0]");
    assert_true(!NODE_FIND_SLOT(new_nb.n, new_nb.b, val_hash(ks[1])), "should not contain ks[1]");

    RELEASE(new_nb.n);
    RELEASE(nb.n);
    destroy_kvs(ks, vs);
    val_end_check_memory();
  }

  ccut_test("node remove to node") {
    pending;
  }
}
