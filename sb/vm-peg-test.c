#include <ccut.h>
#include "sb.h"
#include "vm-peg-op-codes.h"

static Val _struct(const char* name, int argc, Val* argv) {
  uint32_t namespace = sb_klass();
  uint32_t klass = klass_find_c(name, namespace);
  return nb_struct_new(klass, argc, argv);
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
      SPLIT_META(15, 1),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      MATCH, END
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
      SPLIT_META(24, 1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      PUSH_BR, SPLIT_ARG32(22),
      /*15*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      LOOP_UPDATE, SPLIT_ARG32(15),
      /*22*/ MATCH, END
    };
    // sb_vm_peg_decompile(peg);

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
      SPLIT_META(22, 1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      PUSH_BR, SPLIT_ARG32(20),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      POP_BR,
      /*20*/ MATCH, END
    };
    // sb_vm_peg_decompile(peg);

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
      SPLIT_META(28, 1),
      PUSH, SPLIT_ARG64(VAL_NIL),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      PUSH_BR, SPLIT_ARG32(26),
      /*19*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      LIST_MAYBE,
      LOOP_UPDATE, SPLIT_ARG32(19),
      /*26*/ MATCH, END
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
    // foo / bar
    //   push_br L0
    //   foo
    //   pop_br
    //   jmp L1
    //   L0: bar
    //   L1: match
    //   end
    //
    uint16_t peg[] = {
      SPLIT_META(22, 1),
      PUSH_BR, SPLIT_ARG32(17),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      POP_BR,
      JMP, SPLIT_ARG32(20),
      /*17*/ TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      /*20*/ MATCH, END
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
      SPLIT_META(25, 1),
      PUSH_BR, SPLIT_ARG32(17),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      JMP, SPLIT_ARG32(20),
      /*17*/ TERM, 0, 0,
      /*20*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      MATCH, END
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
      SPLIT_META(22, 1),
      PUSH_BR, SPLIT_ARG32(17),
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      TERM, 0, 0,
      /*17*/ TERM, SPLIT_ARG32(val_strlit_new_c("bar")),
      MATCH, END
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

  ccut_test("sb_vm_peg_exec invoking sub-rule") {
    // Main(0): Foo Bar
    // Foo(1): &foo
    // Bar(2): foo
    uint16_t peg[] = {
      SPLIT_META(42, 3),
      RULE_CALL, SPLIT_ARG32(18), SPLIT_ARG32(1), // calls Foo(1)
      RULE_CALL, SPLIT_ARG32(37), SPLIT_ARG32(2), // calls Bar(2)
      // XXX insert RULE_RET here?
      MATCH,

      /*18*/ PUSH_BR, SPLIT_ARG32(28),   // ref L0
      TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      UNPARSE,
      JMP, SPLIT_ARG32(31),              // ref L1
      /*28*/ TERM, SPLIT_ARG32(0),       // L0
      /*31*/ PUSH, SPLIT_ARG64(VAL_NIL), // L1; action returns nil
      RULE_RET,

      /*37*/ TERM, SPLIT_ARG32(val_strlit_new_c("foo")),
      RULE_RET,

      END
    };

    // sb_vm_peg_decompile(peg);
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
