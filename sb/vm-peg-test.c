#include <ccut.h>
#include "sb.h"
#include "vm-peg-op-codes.h"

void vm_peg_suite() {
  ccut_test("peg match term") {
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
    ValPair res = sb_vm_peg_exec(peg, sizeof(tokens) / sizeof(Token), tokens);
    assert_eq(VAL_NIL, res.snd);

    val_gens_set_current(0);
    val_gens_drop();
  }

  ccut_test("peg match term*") {
  }
}
