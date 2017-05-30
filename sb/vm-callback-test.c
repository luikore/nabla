#include <ccut.h>
#include "sb.h"
#include "vm-callback-op-codes.h"

void vm_callback_suite() {
  ccut_test("sb_vm_callback_exec jif true") {
    // push true
    // jif L0:
    // push 3
    // jmp L1
    // L0: push 4
    // L1: end
    uint16_t callback[] = {
      SPLIT_META(29, 0),
      PUSH, SPLIT_ARG64(VAL_TRUE),
      JIF, SPLIT_ARG32(23),
      PUSH, SPLIT_ARG64(VAL_FROM_INT(3)),
      JMP, SPLIT_ARG32(28),
      /*23*/ PUSH, SPLIT_ARG64(VAL_FROM_INT(4)),
      /*28*/ END
    };

    ValPair res = sb_vm_callback_exec(callback, NULL, NULL, 0);
    assert_eq(VAL_NIL, res.snd);
    assert_eq(VAL_FROM_INT(4), res.fst);
  }

  ccut_test("sb_vm_callback_exec jif false") {
    // push false
    // jif L0:
    // push 3
    // jmp L1
    // L0: push 4
    // L1: end
    uint16_t callback[] = {
      SPLIT_META(29, 0),
      PUSH, SPLIT_ARG64(VAL_FALSE),
      JIF, SPLIT_ARG32(23),
      PUSH, SPLIT_ARG64(VAL_FROM_INT(3)),
      JMP, SPLIT_ARG32(28),
      /*23*/ PUSH, SPLIT_ARG64(VAL_FROM_INT(4)),
      /*28*/ END
    };

    ValPair res = sb_vm_callback_exec(callback, NULL, NULL, 0);
    assert_eq(VAL_NIL, res.snd);
    assert_eq(VAL_FROM_INT(3), res.fst);
  }

  ccut_test("sb_vm_callback_exec create node") {
    uint32_t foo_id = val_strlit_new_c("foo");
    uint32_t bar_id = val_strlit_new_c("bar");
    uint32_t baz_id = val_strlit_new_c("baz");
    NbStructField fields[] = {
      {.matcher = VAL_NIL, .field_id = foo_id, .is_splat = false},
      {.matcher = VAL_NIL, .field_id = bar_id, .is_splat = false},
      {.matcher = VAL_NIL, .field_id = baz_id, .is_splat = false}
    };
    uint32_t klass_id = nb_struct_def(nb_string_new_literal_c("Foo"), 0, 3, fields);
    Val list = nb_cons_new(VAL_FALSE, VAL_NIL);
    REPLACE(list, nb_cons_new(VAL_TRUE, list));

    // node_beg klass
    // push nil
    // node_set
    // push [true, false]
    // node_setv
    // node_end
    // end
    uint16_t callback[] = {
      SPLIT_META(24, 0),
      NODE_BEG, SPLIT_ARG32(klass_id),
      PUSH, SPLIT_ARG64(VAL_NIL),
      NODE_SET,
      PUSH, SPLIT_ARG64(list),
      NODE_SETV,
      NODE_END,
      END
    };

    ValPair res = sb_vm_callback_exec(callback, NULL, NULL, 0);
    assert_eq(VAL_NIL, res.snd);
    Val res_node = res.fst;
    assert_eq(klass_id, VAL_KLASS(res_node));

    RELEASE(list);
  }

  ccut_test("sb_vm_callback_exec create list") {
    // push [1]
    // push 2
    // push nil
    // list
    // listv
    uint16_t callback[] = {
      SPLIT_META(25, 0),
      PUSH, SPLIT_ARG64(nb_cons_new(VAL_FROM_INT(1), VAL_NIL)),
      PUSH, SPLIT_ARG64(VAL_FROM_INT(2)),
      PUSH, SPLIT_ARG64(VAL_NIL),
      LIST,
      LISTV,
      END
    };

    ValPair res = sb_vm_callback_exec(callback, NULL, NULL, 0);
    assert_eq(VAL_NIL, res.snd);
    Val res_list = res.fst;
    assert_eq(KLASS_CONS, VAL_KLASS(res_list));
    assert_eq(VAL_FROM_INT(1), nb_cons_head(res_list));
    assert_eq(VAL_FROM_INT(2), nb_cons_head(nb_cons_tail(res_list)));
    assert_eq(VAL_NIL, nb_cons_tail(nb_cons_tail(res_list)));
  }

  ccut_test("sb_vm_callback_compile if") {
  }
}
