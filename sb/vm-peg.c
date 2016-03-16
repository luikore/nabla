// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c)
// but much simpler since we don't have to handle matches

#include "compile.h"
#include "vm-peg-opcodes.h"

typedef struct {
  int32_t offset;
  uint32_t pos;
} Branch;

MUT_ARRAY_DECL(BranchStack, Branch);

MUT_ARRAY_DECL(Stack, Val);

static void _report_stack(struct Stack* stack, uint32_t bp, struct BranchStack* br_stack, uint32_t br_bp) {
  int top;
  top = Stack.size(stack);

  while (bp) {
    printf("%lu: ", *Stack.at(stack, top));
    for (int i = bp; i < top; i++) {
      printf("%lu ", *Stack.at(stack, i));
    }
    printf("\n");
    top = bp - 3;
    bp = *Stack.at(stack, bp - 2);
  }
}

ValPair sb_vm_peg_exec(uint16_t* peg, void* arena, int32_t token_size, Token* tokens) {
  struct BranchStack br_stack;
  struct Stack stack;
  uint32_t br_bp = 0;
  uint32_t bp = 0;
  uint16_t* pc = peg;
  int32_t pos = 0;
  Val* memoize_table;
  Val result;

# define MTABLE(pos, rule) memoize_table[pos * rule_size + rule]

  // Call frame layout:
  //   bp: stack[bp] is current call frame
  //   bp[-3]: return addr
  //   bp[-2]: last bp
  //   bp[-1]: last br_bp
  //   bp[0]: rule_id  # for error reporting
  //   bp[1..]: captures
  //
  // The root frame starts from main rule id

# define _SP(i) *Stack.at(&stack, i)
# define _PUSH(e) Stack.push(&stack, e)
# define _TOP *Stack.at(&stack, Stack.size(&stack) - 1)

  BranchStack.init(&br_stack, 5);
  Stack.init(&stack, 10);
  _PUSH(0); // main rule_id: 0

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
          DISPATCH;
        } else if (bp == 0) {
          goto not_matched;
        } else {
          int new_sp = bp - 3;
          pc = peg + _SP(new_sp);
          bp = _SP(new_sp + 1);
          br_bp = _SP(new_sp + 2);
          stack.size = new_sp;
          goto pop_cond;
        }
      }

      CASE(RULE_CALL) {
        ArgU32U32 payload = DECODE(ArgU32U32, pc); // offset, rule_id
        Val return_addr = (Val)pc;
        pc = peg + payload.arg1;

        _PUSH(return_addr);
        _PUSH(bp);
        _PUSH(br_bp);
        bp = Stack.size(&stack);
        br_bp = BranchStack.size(&br_stack);
        _PUSH((Val)payload.arg2);
        DISPATCH;
      }

      CASE(RULE_RET) {
        if (bp == 0) {
          result = _TOP;
          goto matched;
        } else {
          Val res = _TOP;
          int new_sp = bp - 2;
          pc = peg + _SP(new_sp - 1);
          bp = _SP(new_sp);
          br_bp = _SP(new_sp + 1);
          _SP(new_sp - 1) = res;
          stack.size = new_sp;
          DISPATCH;
        }
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
  return (ValPair){result, VAL_NIL};
}

sb_vm_peg_exec() {
}
