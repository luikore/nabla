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
//   lift # [e]
//   pop_br
//   L0:
//
// e*
//   push nil
//   push_br L0
//   L1: e
//   list # [e, *res]
//   loop_update L1 # L0
//   L0:
//
// e+ # NOTE encode e twice for simplicity,
//           this will not cause much code duplication, since there is no nesting
//   e
//   lift # [e]
//   push_br L0
//   L1: e
//   list # [e, *res]
//   loop_update L1 # L0
//   L0:
//
// a / b
//   push_br L0
//   a
//   pop_br
//   jmp L1
//   L0: b
//   L1:
//
// &e # NOTE epsilon match will also push a result,
//           or `&(&e)` will result in a double pop
//   push_br L0
//   e
//   pop
//   pop_br
//   jmp L1
//   L0: term 0 # always fail
//   L1:
//
// ^e
//   push_br L0
//   e
//   pop
//   term 0 # always fail
//   L0:

#include "compile.h"
#include "vm-peg-opcodes.h"

ValPair sb_vm_peg_exec(uint16_t* peg, void* arena, int32_t token_size, Token* tokens) {
  // branch_stack: (offset, pos)

  // stack layout:
  //   bp: index of current call frame
  //   bp[-2]: return addr
  //   bp[-1]: last bp
  //   bp[0]: magic
  //   bp[1..10]: captures

  // calling convention -- rule_call:
  //   push return addr
  //   push bp
  //   bp = sp
  //   push magic # so captures start at 1, and magic can act as stack checking number

  // calling convention -- rule_ret:
  //   res = stack.top
  //   sp = bp - 2
  //   pc = sp[0]
  //   bp = sp[1]
  //   stack.push res

  return (ValPair){VAL_NIL, VAL_NIL};
}
