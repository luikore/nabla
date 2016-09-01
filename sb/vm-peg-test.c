#include <ccut.h>
#include "sb.h"
#include "vm-peg-op-codes.h"

static Val _struct(const char* name, int argc, Val* argv) {
  uint32_t namespace = sb_klass();
  uint32_t klass = klass_find_c(name, namespace);
  return nb_struct_new(klass, argc, argv);
}

static void _decompile(uint16_t* peg, int bytesize) {
  struct Iseq iseq;
  Iseq.init(&iseq, 5);
  int size = bytesize / sizeof(uint16_t);
  for (int i = 0; i < size; i++) {
    Iseq.push(&iseq, peg[i]);
  }
  sb_vm_peg_decompile(&iseq, 0, size);
  Iseq.cleanup(&iseq);
}

#define ASSERT_PARSE(size, tokens) {\
  ValPair res = sb_vm_peg_exec(peg, (size), tokens);\
  assert_eq(VAL_NIL, res.snd);\
}

#define ASSERT_PARSE_FAIL(size, tokens) {\
  ValPair res = sb_vm_peg_exec(peg, (size), tokens);\
  assert_neq(VAL_NIL, res.snd);\
}

void vm_peg_suite() {
  ccut_test("sb_vm_peg_exec term seq") {
    int32_t gen = val_gens_new_gen();
    val_gens_set_current(gen);

    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      MATCH
    };
    Token tokens[] = {
      {.ty = val_strlit_new_c("foo")},
      {.ty = val_strlit_new_c("bar")}
    };

    ASSERT_PARSE(sizeof(tokens) / sizeof(Token), tokens);

    val_gens_set_current(0);
    val_gens_drop();
  }

  ccut_test("sb_vm_peg_exec term*") {
    // e*
    //   push nil
    //   push_br L0
    //   L1: e
    //   list_maybe # [e, *res]
    //   loop_update L1 # L0
    //   L0:
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      PUSH_BR, SPLIT_ARG32(18),
      /*11*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      LOOP_UPDATE, SPLIT_ARG32(11),
      /*18*/ MATCH
    };
    // _decompile(peg, sizeof(peg));

    Token tokens[] = {
      {.ty = val_strlit_new_c("foo")},
      {.ty = val_strlit_new_c("foo")}
    };

    ASSERT_PARSE(0, tokens);
    ASSERT_PARSE(1, tokens);
    ASSERT_PARSE(2, tokens);

    Token tokens2[] = {
      {.ty = val_strlit_new_c("bar")}
    };
    ASSERT_PARSE_FAIL(1, tokens2);
  }

  ccut_test("sb_vm_peg_exec term?") {
    // e?
    //   push nil
    //   push_br L0
    //   e
    //   list_maybe # [e]
    //   pop_br
    //   L0:
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      PUSH_BR, SPLIT_ARG32(16),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      POP_BR,
      /*16*/ MATCH
    };
    // _decompile(peg, sizeof(peg));

    Token tokens[] = {
      {.ty = val_strlit_new_c("foo")},
      {.ty = val_strlit_new_c("foo")}
    };

    ASSERT_PARSE(0, tokens);
    ASSERT_PARSE(1, tokens);
    ASSERT_PARSE_FAIL(2, tokens);
  }

  ccut_test("sb_vm_peg_exec term+") {
    // e+
    //   push nil
    //   e
    //   list_maybe # [e]
    //   push_br L0
    //   L1: e
    //   list_maybe # [e, *res]
    //   loop_update L1 # L0
    //   L0:
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      PUSH_BR, SPLIT_ARG32(22),
      /*15*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      LOOP_UPDATE, SPLIT_ARG32(15),
      /*22*/ MATCH
    };

    Token tokens[] = {
      {.ty = val_strlit_new_c("foo")},
      {.ty = val_strlit_new_c("foo")},
      {.ty = val_strlit_new_c("foo")}
    };

    ASSERT_PARSE_FAIL(0, tokens);
    ASSERT_PARSE(1, tokens);
    ASSERT_PARSE(3, tokens);
  }

  ccut_test("sb_vm_peg_exec foo / bar") {
    // a / b
    //   push_br L0
    //   a
    //   pop_br
    //   jmp L1
    //   L0: b
    //   L1:
    //
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH_BR, SPLIT_ARG32(13),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      POP_BR,
      JMP, SPLIT_ARG32(16),
      /*13*/ TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      /*16*/ MATCH
    };
    // _decompile(peg, sizeof(peg));

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("foo")}
      };
      ASSERT_PARSE(1, tokens);
    }

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("bar")}
      };
      ASSERT_PARSE(1, tokens);
    }
  }

  ccut_test("sb_vm_peg_exec &term term") {
    // &e # NOTE epsilon match will also push a result,
    //           or `&(&e)` will result in a double pop
    //   push_br L0
    //   e
    //   unparse
    //   jmp L1
    //   L0: term 0 # always fail
    //   L1:
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH_BR, SPLIT_ARG32(13),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      JMP, SPLIT_ARG32(16),
      /*13*/ TERM, 0, 0,
      /*16*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      MATCH
    };

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("foo")}
      };
      ASSERT_PARSE(1, tokens);
    }

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("bar")}
      };
      ASSERT_PARSE_FAIL(1, tokens);
    }
  }

  ccut_test("sb_vm_peg_exec !term term") {
    // !e
    //   push_br L0
    //   e
    //   unparse
    //   term 0 # always fail
    //   L0:
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH_BR, SPLIT_ARG32(13),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      TERM, 0, 0,
      /*13*/ TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      MATCH
    };

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("foo")}
      };
      ASSERT_PARSE_FAIL(1, tokens);
    }

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("bar")}
      };
      ASSERT_PARSE(1, tokens);
    }
  }

  ccut_test("sb_vm_peg_exec jif") {
    // term foo
    // jif L0:
    // term bar
    // L0: match
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      JIF, SPLIT_ARG32(12),
      TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      /*12*/
      MATCH
    };

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("foo"), .v = VAL_TRUE}
      };
      ASSERT_PARSE(1, tokens);
    }

    {
      Token tokens[] = {
        {.ty = val_strlit_new_c("foo"), .v = VAL_FALSE}
      };
      ASSERT_PARSE_FAIL(1, tokens);
    }
  }

  ccut_test("sb_vm_peg_exec create node") {
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
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      NODE_BEG, SPLIT_ARG32(klass_id),
      PUSH, SPLIT_ARG64(VAL_NIL),
      NODE_SET,
      PUSH, SPLIT_ARG64(list),
      NODE_SETV,
      NODE_END,
      RULE_RET,
      MATCH
    };

    ValPair res = sb_vm_peg_exec(peg, 0, NULL);
    assert_eq(VAL_NIL, res.snd);
    Val res_node = res.fst;
    assert_eq(klass_id, VAL_KLASS(res_node));

    RELEASE(list);
  }

  ccut_test("sb_vm_peg_exec create list") {
    // push [1]
    // push 2
    // push nil
    // list
    // listv
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(1),
      PUSH, SPLIT_ARG64(nb_cons_new(VAL_FROM_INT(1), VAL_NIL)),
      PUSH, SPLIT_ARG64(VAL_FROM_INT(2)),
      PUSH, SPLIT_ARG64(VAL_NIL),
      LIST,
      LISTV,
      RULE_RET,
      MATCH
    };

    ValPair res = sb_vm_peg_exec(peg, 0, NULL);
    assert_eq(VAL_NIL, res.snd);
    Val res_list = res.fst;
    assert_eq(KLASS_CONS, VAL_KLASS(res_list));
    assert_eq(VAL_FROM_INT(1), nb_cons_head(res_list));
    assert_eq(VAL_FROM_INT(2), nb_cons_head(nb_cons_tail(res_list)));
    assert_eq(VAL_NIL, nb_cons_tail(nb_cons_tail(res_list)));
  }

  ccut_test("sb_vm_peg_exec invoking sub-rule") {
    // Main(0): Foo Bar
    // Foo(1): &foo
    // Bar(2): foo
    uint16_t peg[] = {
      RULE_SIZE, SPLIT_ARG32(3),
      RULE_CALL, SPLIT_ARG32(14), SPLIT_ARG32(1), // calls Foo(1)
      RULE_CALL, SPLIT_ARG32(33), SPLIT_ARG32(2), // calls Bar(2)
      // XXX insert RULE_RET here?
      MATCH,

      /*14*/ PUSH_BR, SPLIT_ARG32(24),   // ref L0
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      JMP, SPLIT_ARG32(27),              // ref L1
      /*24*/ TERM, SPLIT_ARG32(0),       // L0
      /*27*/ PUSH, SPLIT_ARG64(VAL_NIL), // L1; action returns nil
      RULE_RET,

      /*33*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      RULE_RET
    };

    // _decompile(peg, sizeof(peg));
    Token tokens[] = {
      {.ty = val_strlit_new_c("foo")}
    };
    ASSERT_PARSE(1, tokens);
  }

  ccut_test("sb_vm_peg_compile term*") {
  }

  ccut_test("sb_vm_peg_compile term+") {
  }

  ccut_test("sb_vm_peg_compile term?") {
  }

  ccut_test("sb_vm_peg_compile &term") {
  }

  ccut_test("sb_vm_peg_compile !term") {
  }

  ccut_test("sb_vm_peg_compile a / b") {
  }

  ccut_test("sb_vm_peg_compile a /* b") {
  }

  ccut_test("sb_vm_peg_compile a /+ b") {
  }

  ccut_test("sb_vm_peg_compile a /? b") {
  }

  ccut_test("sb_vm_peg_compile expr") {
  }
}
