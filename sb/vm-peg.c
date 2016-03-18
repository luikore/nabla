// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c)
// but much simpler since we don't have to handle matches

#include "compile.h"
#include "vm-peg-op-codes.h"

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
  //   bp[0]: rule_id  # for memoizing & error reporting
  //   bp[1..]: captures
  //
  // The root frame starts from main rule id

# define _SP(i) *Stack.at(&stack, i)
# define _PUSH(e) Stack.push(&stack, e)
# define _POP() Stack.pop(&stack)
# define _TOP() Stack.at(&stack, Stack.size(&stack) - 1)

  BranchStack.init(&br_stack, 5);
  Stack.init(&stack, 10);
  _PUSH(0); // main rule_id: 0

# define CASE(op) case op:
# define DISPATCH continue

  // code size;
  uint32_t rule_size = DECODE(ArgU32, pc).arg1;
  memoize_table = malloc(rule_size * token_size * sizeof(Val));
  for (int i = 0; i < rule_size * token_size; i++) {
    memoize_table[i] = VAL_UNDEF;
  }

  for (;;) {
    switch (*pc) {
      CASE(TERM) {
        if (pos == token_size) {
          goto not_matched;
        }
        uint32_t tok = DECODE(ArgU32, pc).arg1;
        if (tok == tokens[pos].ty) {
          pos++;
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
        if (MTABLE(pos, payload.arg2) != VAL_UNDEF) {
          _PUSH(MTABLE(pos, payload.arg2));
        } else {
          Val return_addr = (Val)pc;
          pc = peg + payload.arg1;

          _PUSH(return_addr);
          _PUSH(bp);
          _PUSH(br_bp);
          bp = Stack.size(&stack);
          br_bp = BranchStack.size(&br_stack);
          _PUSH((Val)payload.arg2);
        }
        DISPATCH;
      }

      CASE(RULE_RET) {
        if (bp == 0) {
          result = *_TOP();
          goto matched;
        } else {
          Val res = *_TOP();
          int new_sp = bp - 2;
          pc = peg + _SP(new_sp - 1);
          bp = _SP(new_sp);
          br_bp = _SP(new_sp + 1);
          Val rule_id = _SP(new_sp + 2);
          _SP(new_sp - 1) = res;
          stack.size = new_sp;
          MTABLE(pos, rule_id) = res;
          DISPATCH;
        }
      }

      CASE(PUSH_BR) {
        int32_t offset = DECODE(Arg32, pc).arg1;
        Branch br = {pc - peg, pos};
        BranchStack.push(&br_stack, br);
        DISPATCH;
      }

      CASE(POP_BR) {
        BranchStack.pop(&br_stack);
        DISPATCH;
      }

      CASE(LOOP_UPDATE) {
        int32_t offset = DECODE(Arg32, pc).arg1;
        Branch* top_br = BranchStack.at(&br_stack, BranchStack.size(&br_stack) - 1);
        if (pos == top_br->pos) { // no advance, stop loop
          pc = peg + top_br->offset;
          BranchStack.pop(&br_stack);
        } else { // loop
          top_br->pos = pos;
          pc = peg + offset;
        }
        DISPATCH;
      }

      CASE(JMP) {
        int32_t offset = DECODE(Arg32, pc).arg1;
        pc += offset;
        DISPATCH;
      }

      CASE(CAPTURE) {
        pc++;
        uint16_t index = *pc;
        pc++;
        _PUSH(_SP(bp + index));
        DISPATCH;
      }

      CASE(PUSH) {
        Val val = DECODE(ArgVal, pc).arg1;
        _PUSH(val);
        DISPATCH;
      }

      CASE(POP) {
        _POP();
        DISPATCH;
      }

      CASE(NODE) {
        ArgU32U32 data = DECODE(ArgU32U32, pc);
        stack.size -= data.arg1;
        _PUSH(nb_struct_anew(arena, data.arg2, data.arg1, _TOP()));
        DISPATCH;
      }

      CASE(LIFT) {
        pc++;
        _PUSH(nb_cons_anew(arena, _POP(), VAL_NIL));
        DISPATCH;
      }

      CASE(LIST) {
        pc++;
        Val tail = _POP();
        Val head = _POP();
        _PUSH(nb_cons_anew(arena, head, tail));
        DISPATCH;
      }

      CASE(R_LIST) {
        pc++;
        Val head = _POP();
        Val tail = _POP();
        _PUSH(nb_cons_anew(arena, head, tail));
        DISPATCH;
      }

      CASE(JIF) {
        Val cond = _POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        if (!VAL_IS_TRUE(cond)) {
          pc += offset;
        }
        DISPATCH;
      }

      CASE(MATCH) {
        goto matched;
        DISPATCH;
      }

      CASE(FAIL) {
        goto not_matched;
        DISPATCH;
      }
    }
  }

not_matched:

  BranchStack.cleanup(&br_stack);
  Stack.cleanup(&stack);
  free(memoize_table);
  return (ValPair){VAL_NIL, nb_string_new_literal_c("error")};

matched:

  BranchStack.cleanup(&br_stack);
  Stack.cleanup(&stack);
  free(memoize_table);
  return (ValPair){result, VAL_NIL};
}
