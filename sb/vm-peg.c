// a VM similar to LPEG (https://github.com/LuaDist/lpeg/blob/master/lpvm.c)
// but much simpler since we don't have to handle char matches

#include "compile.h"
#include "vm-peg-op-codes.h"

typedef struct {
  int32_t offset;
  uint32_t pos;
  int32_t stack_offset;
} Branch;

// manage PEG prioritized branches
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

ValPair sb_vm_peg_exec(uint16_t* peg, int32_t token_size, Token* tokens) {
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
# define _TOPN(n) Stack.at(&stack, Stack.size(&stack) - (1 + n))

  BranchStack.init(&br_stack, 5);
  Stack.init(&stack, 10);
  _PUSH(0); // main rule_id: 0

# define CASE(op) case op:
# define DISPATCH continue

  uint32_t rule_size = DECODE(ArgU32, pc).arg1;
  memoize_table = malloc(rule_size * token_size * sizeof(Val));
  for (int i = 0; i < rule_size * token_size; i++) {
    memoize_table[i] = VAL_UNDEF;
  }

  for (;;) {
    // if (val_is_tracing()) {
    //   // debug breakpoint
    // }
    switch (*pc) {
      CASE(TERM) {
        if (pos == token_size) {
          goto pop_cond;
        }
        uint32_t tok = DECODE(ArgU32, pc).arg1;
        if (tok == tokens[pos].ty) {
          _PUSH(tokens[pos].v); // TODO slice token string?
          pos++;
          DISPATCH;
        }

        // todo update furthest expect
pop_cond:
        if (BranchStack.size(&br_stack) > br_bp) {
          Branch br = BranchStack.pop(&br_stack);
          pc = peg + br.offset;
          pos = br.pos;
          stack.size = br.stack_offset; // TODO free to the end
          DISPATCH;
        } else if (bp == 0) {
          goto not_matched;
        } else {
          // TODO also make use of branch stack here?
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

      // We always have stack top here because:
      // - if rule doesn't have action, the default action aggregates all elems into a list
      // - even empty action returns nil
      CASE(RULE_RET) {
        if (bp == 0) {
          pc++;
          result = *_TOP();
          goto matched;
        } else {
          Val res = *_TOP();
          int new_sp = bp - 2;
          pc = (uint16_t*)_SP(new_sp - 1);
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
        Branch br = {offset, pos, Stack.size(&stack)};
        BranchStack.push(&br_stack, br);
        DISPATCH;
      }

      CASE(POP_BR) {
        pc++;
        BranchStack.pop(&br_stack);
        DISPATCH;
      }

      CASE(UNPARSE) {
        pc++;
        Branch br = BranchStack.pop(&br_stack);
        pos = br.pos;
        stack.size = br.stack_offset; // TODO free to the end
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
        pc = peg + offset;
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
        pc++;
        _POP();
        DISPATCH;
      }

      CASE(NODE_BEG) {
        uint32_t klass_id = DECODE(ArgU32, pc).arg1;
        ValPair val_and_size = nb_struct_new_empty(klass_id);
        _PUSH(val_and_size.fst);
        uint64_t limit = (uint32_t)val_and_size.snd;
        _PUSH(limit << 32);
        DISPATCH;
      }

      CASE(NODE_SET) {
        pc++;
        Val e = _POP();
        Val n = *_TOP();
        uint32_t limit = (uint32_t)(n >> 32);
        uint32_t i = (n & 0xFFFFFFFFU);
        if (i >= limit) {
          // TODO raise error
        }
        Val node = *_TOPN(1);
        nb_struct_set(node, i, e);
        _TOP()[0]++;
        DISPATCH;
      }

      CASE(NODE_SETV) {
        pc++;
        Val e = _POP();
        Val n = *_TOP();
        uint32_t limit = (uint32_t)(n >> 32);
        uint32_t i = (n & 0xFFFFFFFFU);
        if (i >= limit) {
          // TODO raise error
        }
        if (n && VAL_KLASS(n) != KLASS_CONS) {
          // TODO raise error splatting non-cons
        }
        Val node = *_TOPN(1);
        for (Val tail = e; tail; tail = nb_cons_tail(tail)) {
          Val head = nb_cons_head(tail);
          nb_struct_set(node, i++, head);
          _TOP()[0]++;
        }
        DISPATCH;
      }

      CASE(NODE_END) {
        pc++;
        Val n = _POP();
        uint32_t limit = (uint32_t)(n >> 32);
        uint32_t i = (n & 0xFFFFFFFFU);
        if (i != limit) {
          // TODO raise error if arg count not match
        }
        DISPATCH;
      }

      CASE(LIST) {
        pc++;
        Val tail = _POP();
        Val head = _POP();
        _PUSH(nb_cons_new(head, tail));
        DISPATCH;
      }

      CASE(LIST_MAYBE) {
        pc++;
        Branch* br;
        if (BranchStack.size(&br_stack)) {
          br = BranchStack.at(&br_stack, BranchStack.size(&br_stack) - 1);
        } else {
          br = NULL;
        }
        if (br && br->stack_offset + 1 == Stack.size(&stack)) {
          Val tail = _POP();
          Val head = _POP();
          _PUSH(nb_cons_new(head, tail));
        }
        DISPATCH;
      }

      CASE(R_LIST) {
        pc++;
        Val head = _POP();
        Val tail = _POP();
        _PUSH(nb_cons_new(head, tail));
        DISPATCH;
      }

      CASE(JIF) {
        Val cond = _POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        // TODO check offset out of bound?
        if (VAL_IS_TRUE(cond)) {
          pc = peg + offset;
        }
        DISPATCH;
      }

      CASE(JUNLESS) {
        Val cond = _POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        if (VAL_IS_FALSE(cond)) {
          pc = peg + offset;
        }
        DISPATCH;
      }

      CASE(CALL) {
        ArgU32U32 data = DECODE(ArgU32U32, pc);
        uint32_t argc = data.arg1;
        assert(argc >= 1); // all operators
        uint32_t method = data.arg2;
        Val* argv = Stack.at(&stack, stack.size - argc);
        void* m = klass_find_method(sb_klass(), method);
        ValPair res = klass_call_method(VAL_NIL, m, argc, argv);
        if (res.snd) {
          // TODO cleanup and raise error
        }
        stack.size -= argc;
        _SP(stack.size - 1) = res.fst;
        // TODO need to decrease ref or all managed by gen?
        DISPATCH;
      }

      CASE(MATCH) {
        pc++;
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
  // todo detailed result, and maybe resumable?
  return (ValPair){VAL_NIL, nb_string_new_literal_c("error")};

matched:

  BranchStack.cleanup(&br_stack);
  Stack.cleanup(&stack);
  free(memoize_table);
  if (token_size != pos) {
    // todo detailed result, and maybe resumable?
    // todo release result?
    return (ValPair){VAL_NIL, nb_string_new_literal_c("unparsed")};
  } else {
    return (ValPair){result, VAL_NIL};
  }
}
