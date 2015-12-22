// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c) but much simpler

// list and node instructions

/*
- ret : return from a rule
- choice offset : stack a choice, next fail will jump to offset
- fail: 
*/

#include "compile.h"

VmPeg* sb_vm_peg_compile(CompileCtx* ctx, Val peg_node, Val* err) {
  return NULL;
}

Val sb_vm_peg_exec(Spellbreak* sb, VmPeg* peg, int32_t token_pos, Val* err) {
  return 0;
}
