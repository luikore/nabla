#include <ccut.h>
#include "sb.h"
#include "vm-peg-op-codes.h"

void vm_peg_suite() {
  ccut_test("peg match term") {
    void* arena = val_arena_new();

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
    ValPair res = sb_vm_peg_exec(peg, arena, sizeof(tokens) / sizeof(Token), tokens);
    assert_eq(VAL_NIL, res.snd);

    val_arena_delete(arena);
  }

  ccut_test("peg match term*") {
  }
}
