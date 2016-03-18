#include "compile.h"
#include "vm-peg-op-codes.h"

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
//   pop_br
//   term 0 # always fail
//   L0:

Val sb_vm_peg_compile(CompileCtx* ctx, Val node) {
  return VAL_NIL;
}

void sb_vm_peg_decompile(struct Iseq* iseq, int32_t start, int32_t size) {
}
