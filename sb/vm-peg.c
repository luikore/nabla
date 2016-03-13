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
//   pop_br
//   term 0 # always fail
//   L0:

#include "compile.h"
#include "vm-peg-opcodes.h"

typedef struct {
  int32_t offset;
  uint32_t pos;
} Branch;

MUT_ARRAY_DECL(BranchStack, Branch);

MUT_ARRAY_DECL(Stack, Val);

static _report_stack(struct Stack* stack, struct ) {
  
}

ValPair sb_vm_peg_exec(uint16_t* peg, void* arena, int32_t token_size, Token* tokens) {
  struct BranchStack br_stack;
  struct Stack stack;
  uint32_t br_bp;
  uint32_t bp;
  uint16_t* pc = peg;
  int32_t pos = 0;
  Val* memoize_table;

# define MTABLE(pos, rule) memoize_table[pos * rule_size + rule]

  // stack layout:
  //   bp: stack[bp] is current call frame
  //   bp[-3]: return addr
  //   bp[-2]: last bp
  //   bp[-1]: last br_bp
  //   bp[0]: rule_id  # for error reporting
  //   bp[1..]: captures

  BranchStack.init(&br_stack, 5);
  Stack.init(&stack, 10);
  Stack.push(&stack, 0); // main rule_id: 0

# define CASE(op) case op:
# define DISPATCH continue

  // code size;
  uint32_t rule_size = DECODE(ArgU32, pc).arg1;
  memoize_table = malloc(rule_size * token_size * sizeof(Val));
  memset(memoize_table, 0, rule_size * token_size * sizeof(Val));

  for (;;) {
    switch (*pc) {
      CASE(TERM) {
        uint32_t tok = DECODE(ArgU32, pc).arg1;
        if (tok == tokens[pos].ty) {
          DISPATCH;
        }

        // todo update furthest expect
pop_cond:
        if (BranchStack.size(&br_stack) > br_bp) {
          Branch br = BranchStack.pop(&br_stack);
          pc = peg + br.offset;
          pos = br.pos;
        } else if (Stack.size(&stack) == 0) {
          // todo failure
        } else {
          // todo pop both stack frames
          goto pop_cond;
        }
        break;
      }

      CASE(RULE_CALL) {
        // push return addr
        // push bp
        // push br_bp
        // bp = sp
        // br_bp = br_sp
        // push rule_id # so captures start at 1
        
        break;
      }

      CASE(RULE_RET) {
        // res = stack.top
        // sp = bp - 3
        // pc = sp[0]
        // bp = sp[1]
        // br_bp = sp[2]
        // stack.push res

        break;
      }

      CASE(PUSH_BR) {
        break;
      }

      CASE(POP_BR) {
        break;
      }

      CASE(LOOP_UPDATE) {
        break;
      }

      CASE(JMP) {
        break;
      }

      CASE(CAPTURE) {
        break;
      }

      CASE(PUSH) {
        break;
      }

      CASE(POP) {
        break;
      }

      CASE(NODE) {
        break;
      }

      CASE(LIFT) {
        break;
      }

      CASE(LIST) {
        break;
      }

      CASE(R_LIST) {
        break;
      }

      CASE(JIF) {
        break;
      }

      CASE(MATCH) {
        break;
      }

      CASE(FAIL) {
        break;
      }
    }
  }

not_matched:

  BranchStack.cleanup(&br_stack);
  Stack.cleanup(&stack);
  free(memoize_table);
  return (ValPair){VAL_NIL, VAL_NIL};

matched:

  BranchStack.cleanup(&br_stack);
  Stack.cleanup(&stack);
  free(memoize_table);
  return (ValPair){VAL_NIL, VAL_NIL};
}
