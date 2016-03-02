// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c)
// but much simpler since we don't have to handle matches

// Rules are encoded one by one.
// Each rule is similar to a method and we have call/ret
// The memoize table is of rule_size * input_size

// Encoding of PEG constructs:
//
// e?
//   push_br L0
//   e
//   pop_br
//   L0:
//
// e*
//   push_br L0
//   L1: e
//   update
//   jmp L1
//   L0:
//
// a / b
//   push_br L0
//   a
//   jmp L1
//   L0: b
//   L1:

enum OpCodes {
  // op     // args           // description
  TERM=1,   // str:uint32     // matching a terminal
  PUSH_BR,  // offset:int32   // push (branch, curr)
  POP_BR,   //                // pop (branch, curr)
  UPDATE,   //                // replace top curr
  JMP,      // offset:int32   // jump to an offset
  CALL,     // offset:int32   // call another code
  RET,      //                // return from call, and set memoize table
  MATCH,    // id:uint32      // match id
  FAIL,
  OP_CODES_SIZE
};

#include "compile.h"

Val sb_vm_peg_compile(CompileCtx* ctx, Val node) {
  return VAL_NIL;
}

ValPair sb_vm_peg_exec(void* peg, void* arena, int32_t token_size, Token* tokens) {
  return (ValPair){VAL_NIL, VAL_NIL};
}
