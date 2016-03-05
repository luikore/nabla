#include "compile.h"
#include "vm-lex-opcodes.h"

// vm for lex and callback (not for peg's callback)

ValPair sb_vm_lex_exec(Spellbreak* sb) {
  static const void* labels[] = {
    [PUSH] = &&label_PUSH,
    [POP] = &&label_POP,
    [LOAD] = &&label_LOAD,
    [STORE] = &&label_STORE,
    [CALL] = &&label_CALL,
    [NODE] = &&label_NODE,
    [LIST] = &&label_LIST,
    [R_LIST] = &&label_R_LIST,
    [JIF] = &&label_JIF,
    [JMP] = &&label_JMP,
    [MATCH_RE] = &&label_MATCH_RE,
    [MATCH_STR] = &&label_MATCH_STR,
    [CTX_CALL] = &&label_CTX_CALL,
    [END] = &&label_END,
    [NOP] = &&label_NOP
  };

  Val iseq;
  bool matched;
  uint16_t* pc;
  Val err;

# define DISPATCH goto *labels[*pc]
# define CASE(l) label_##l: case l
# define STACK_PUSH(v) Vals.push(&sb->stack, (v))
# define STACK_POP() Vals.pop(&sb->stack)
# define STACK_TOP() Vals.at(&sb->stack, Vals.size(&sb->stack) - 1)
# define CTX_PUSH(name) ({\
  ContextEntry ce = {\
    .name_str = name,\
    .token_pos = 0,\
    .curr = sb->curr\
  };\
  if (!nb_dict_find(sb->lex_dict, val_strlit_ptr(name), val_strlit_byte_size(name), &iseq)) {\
    err = nb_string_new_f("can't find lex: %.*s", (int)val_strlit_byte_size(name), val_strlit_ptr(name));\
    goto terminate;\
  }\
  ContextStack.push(&sb->context_stack, ce);\
})
# define CTX_POP() ContextStack.pop(&sb->context_stack)

  CTX_PUSH(val_strlit_new_c("Main"));
  for (;;) {
begin:
    matched = false;
    pc = Iseq.at((struct Iseq*)iseq, 0);
    DISPATCH;
    switch(*pc) {
      CASE(PUSH): {
        STACK_PUSH(DECODE(ArgVal, pc).val);
        DISPATCH;
      }

      CASE(POP): {
        pc++;
        STACK_POP();
        DISPATCH;
      }

      CASE(LOAD): {
        uint32_t i = DECODE(ArgU32, pc).arg1;
        STACK_PUSH(*Vals.at(&sb->vars, i));
        DISPATCH;
      }

      CASE(STORE): {
        uint32_t i = DECODE(ArgU32, pc).arg1;
        *Vals.at(&sb->vars, i) = STACK_POP();
        DISPATCH;
      }

      CASE(CALL): {
        ArgU32U32 data = DECODE(ArgU32U32, pc);
        sb->stack.size -= data.arg1;
        ValPair res = val_send((Val)sb, data.arg2, data.arg1, STACK_TOP());
        if (res.snd) {
          goto terminate;
        } else {
          STACK_PUSH(res.fst);
        }
        DISPATCH;
      }

      CASE(NODE): {
        ArgU32U32 data = DECODE(ArgU32U32, pc);
        sb->stack.size -= data.arg1;
        STACK_PUSH(nb_struct_anew(sb->arena, data.arg2, data.arg1, STACK_TOP()));
        DISPATCH;
      }

      CASE(LIST): {
        pc++;
        Val tail = STACK_POP();
        Val head = STACK_POP();
        STACK_PUSH(nb_cons_anew(sb->arena, head, tail));
        DISPATCH;
      }

      CASE(R_LIST): {
        pc++;
        Val last = STACK_POP();
        Val init = STACK_POP();
        STACK_PUSH(nb_cons_anew_rev(sb->arena, init, last));
        DISPATCH;
      }

      CASE(JIF): {
        Val cond = STACK_POP();
        int32_t offset = DECODE(Arg32, pc).arg1;
        if (!VAL_IS_TRUE(cond)) {
          pc += offset;
        }
        DISPATCH;
      }

      CASE(JMP): {
        int32_t offset = DECODE(Arg32, pc).arg1;
        pc += offset;
        DISPATCH;
      }

      CASE(MATCH_RE): {
        // todo check eof
        Arg3232 offsets = DECODE(Arg3232, pc);
        matched = sb_vm_regexp_exec(pc, sb->s + sb->size - sb->curr, sb->curr, sb->captures);
        if (matched) {
          sb->curr += sb->captures[1];
          pc += offsets.arg1;
        } else {
          pc += offsets.arg2;
        }
        DISPATCH;
      }

      CASE(MATCH_STR): {
        // todo check eof
        Arg3232 offsets = DECODE(Arg3232, pc);
        Val str = STACK_POP();
        matched = sb_string_match(str, sb->s + sb->size - sb->curr, sb->curr, &sb->capture_size, sb->captures);
        if (matched) {
          sb->curr += sb->captures[1];
          pc += offsets.arg1;
        } else {
          pc += offsets.arg2;
        }
        DISPATCH;
      }

      CASE(CTX_CALL): {
        uint32_t ctx_name_str = DECODE(ArgU32, pc).arg1;
        CTX_PUSH(ctx_name_str);
        DISPATCH;
      }

      CASE(END): {
        if (matched) {
          // todo check curr advancement
          goto begin;
        } else {
          if (ContextStack.size(&sb->context_stack) == 1) {
            goto terminate;
          } else {
            CTX_POP();
            goto begin;
          }
        }
        DISPATCH;
      }

      CASE(NOP): {
        pc++;
        DISPATCH;
      }
    }
  }

terminate:

  return (ValPair){err ? VAL_NIL : Vals.pop(&sb->stack), VAL_NIL};
}

bool sb_string_match(Val pattern_str, int64_t size, const char* str, int32_t* capture_size, int32_t* captures) {
  return false;
}
