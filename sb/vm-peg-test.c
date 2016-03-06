#include <ccut.h>
#include "sb.h"
#include "vm-peg-opcodes.h"

void vm_peg_suite() {
  ccut_test("peg match term") {
    void* arena = val_arena_new();

    uint16_t peg[] = {
    };
    Token tokens[] = {
    };
    ValPair res = sb_vm_peg_exec(peg, arena, token_size, tokens);

    val_arena_delete(arena);
  }
}
