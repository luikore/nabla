// a VM similar to execute callback code in LEX/PEG

#include "compile.h"
#include "vm-callback-op-codes.h"

typedef struct {
  uint32_t limit;
  uint32_t i;
} ContainerInfo;
MUT_ARRAY_DECL(ContainerInfos, ContainerInfo);

ValPair sb_vm_callback_exec(uint16_t* pc, struct Vals* stack, uint32_t bp) {
  // TODO use stack allocation when container_infos is not too large
  struct ContainerInfos container_infos;
  ContainerInfos.init(&container_infos, 5);
  struct Vals stack_storage; // in case stack given
  bool use_stack_storage = false;
  Val ret = VAL_NIL;
  uint16_t* callback = pc;

  if (!stack) {
    use_stack_storage = true;
    Vals.init(&stack_storage, 10);
    stack = &stack_storage;
  }

# define _SP(i) *Vals.at(stack, i)
# define _PUSH(e) Vals.push(stack, e)
# define _POP() Vals.pop(stack)
# define _TOP() Vals.at(stack, Vals.size(stack) - 1)
# define _TOPN(n) Vals.at(stack, Vals.size(stack) - n)
# define _NODE_PUSH(limit) ContainerInfos.push(&container_infos, ((ContainerInfo){.limit = limit, .i = 0}))
# define _NODE_POP() do {\
  ContainerInfo* ci = ContainerInfos.at(&container_infos, ContainerInfos.size(&container_infos) - 1);\
  if (ci->i != ci->limit) {\
    /* todo raise error */\
  }\
  ContainerInfos.pop(&container_infos);\
} while (0)
# define _NODE_TOP() ContainerInfos.at(&container_infos, ContainerInfos.size(&container_infos) - 1)
# define CASE(op) case op:
# define DISPATCH continue

  // skip meta
  DECODE(ArgU32, pc);
  DECODE(void*, pc);

  for (;;) {
    switch (*pc) {
      CASE(LOAD) {
        uint32_t var_id = DECODE(ArgU32, pc).arg1;
        _PUSH(_SP(var_id));
        DISPATCH;
      }

      CASE(STORE) {
        uint32_t var_id = DECODE(ArgU32, pc).arg1;
        _SP(var_id) = _POP();
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
        _NODE_PUSH(limit);
        DISPATCH;
      }

      CASE(NODE_SET) {
        pc++;
        Val e = _POP();
        ContainerInfo* ci = _NODE_TOP();
        if (ci->i >= ci->limit) {
          // TODO raise error
        }
        Val node = *_TOP();
        nb_struct_set(node, ci->i, e);
        ci->i++;
        DISPATCH;
      }

      CASE(NODE_SETV) {
        pc++;
        Val e = *_TOP();
        if (e && VAL_KLASS(e) != KLASS_CONS) {
          // TODO raise error splatting non-cons or just set single value for convenience?
        }
        _POP();

        ContainerInfo* ci = _NODE_TOP();
        Val node = *_TOP();
        for (Val tail = e; tail; tail = nb_cons_tail(tail)) {
          if (ci->i >= ci->limit) {
            // TODO raise error
          }
          Val head = nb_cons_head(tail);
          nb_struct_set(node, ci->i, head);
          ci->i++;
        }
        DISPATCH;
      }

      CASE(NODE_END) {
        pc++;
        _NODE_POP();
        DISPATCH;
      }

      CASE(LIST) {
        pc++;
        Val b = _POP();
        Val a = _POP();
        _PUSH(nb_cons_new(a, b));
        DISPATCH;
      }

      CASE(LISTV) {
        pc++;
        Val b = _POP();
        Val a = _POP();
        int n = 0;
        for (Val tail = a; tail; tail = nb_cons_tail(tail)) {
          _PUSH(nb_cons_head(a));
          n++;
        }
        for (int i = 0; i < n; i++) {
          Val e = _POP();
          b = nb_cons_new(e, b);
        }
        _PUSH(b);
        DISPATCH;
      }

      CASE(JIF) {
        Val cond = _POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        // TODO check offset out of bound?
        if (VAL_IS_TRUE(cond)) {
          pc = callback + offset;
        }
        DISPATCH;
      }

      CASE(JUNLESS) {
        Val cond = _POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        if (VAL_IS_FALSE(cond)) {
          pc = callback + offset;
        }
        DISPATCH;
      }

      CASE(JMP) {
        int32_t offset = DECODE(Arg32, pc).arg1;
        pc = callback + offset;
        DISPATCH;
      }

      CASE(CALL) {
        ArgU32U32 data = DECODE(ArgU32U32, pc);
        uint32_t argc = data.arg1;
        assert(argc >= 1); // all operators
        uint32_t method = data.arg2;
        Val* argv = _TOPN(argc);
        void* m = klass_find_method(sb_klass(), method);
        ValPair res = klass_call_method(VAL_NIL, m, argc, argv);
        if (res.snd) {
          // TODO cleanup and raise error
        }
        stack->size -= argc;
        _PUSH(res.fst); // may need allocate when argc=0
        // TODO need to decrease ref or all managed by gen?
        DISPATCH;
      }

      CASE(END) {
        // TODO cleanup stack
        goto terminate;
      }

      default: {
        // TODO raise error
      }
    }
  }

terminate:

  ret = *Vals.at(stack, bp);
  if (use_stack_storage) {
    Vals.cleanup(stack);
  }
  return (ValPair){ret, VAL_NIL};
}
