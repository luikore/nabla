// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c) but much simpler

// list and node instructions

/*
- ret : return from a rule
- choice offset : stack a choice, next fail will jump to offset
- fail: 
*/

#include "compile.h"

Val sb_vm_peg_compile(CompileCtx* ctx, Val node) {
  return VAL_NIL;
}

ValPair sb_vm_peg_exec(void* peg, void* arena, int32_t token_size, Token* tokens) {
  return (ValPair){VAL_NIL, VAL_NIL};
}
