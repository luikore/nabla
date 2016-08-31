#include "compile.h"
#include "vm-lex-op-codes.h"

// vm for lex and callback (not for peg's callback)

ValPair sb_vm_lex_exec(Spellbreak* sb) {
  static const void* labels[] = {
    [MATCH_RE] = &&label_MATCH_RE,
    [MATCH_STR] = &&label_MATCH_STR,
    [CALLBACK] = &&label_CALLBACK,
    [CTX_CALL] = &&label_CTX_CALL,
    [CTX_END] = &&label_CTX_END,
    [JMP] = &&label_JMP
  };

  bool matched;
  uint16_t* pc;
  Val err;

# define DISPATCH goto *labels[*pc]
# define CASE(l) label_##l: case l:
# define STACK_PUSH(v) Vals.push(&sb->stack, (v))
# define STACK_POP() Vals.pop(&sb->stack)
# define STACK_TOP() Vals.at(&sb->stack, Vals.size(&sb->stack) - 1)
# define CTX_PUSH(name) ({\
  ContextEntry ce = {\
    .name_str = name,\
    .token_pos = 0,\
    .curr = sb->curr\
  };\
  int32_t offset = sb_compile_context_dict_find(sb->context_dict, name, 'l');\
  if (offset < 0) {\
    err = nb_string_new_f("can't find lex: %.*s", (int)val_strlit_byte_size(name), val_strlit_ptr(name));\
    goto terminate;\
  }\
  pc = Iseq.at(sb->iseq, offset);\
  ContextStack.push(&sb->context_stack, ce);\
})
# define CTX_POP() ContextStack.pop(&sb->context_stack)

  CTX_PUSH(val_strlit_new_c("Main"));
  for (;;) {
begin:
    matched = false;
    pc = Iseq.at(sb->iseq, 0);
    DISPATCH;
    switch(*pc) {

      CASE(MATCH_RE) {
        // todo check eof
        Arg3232 offsets = DECODE(Arg3232, pc);
        matched = sb_vm_regexp_exec(pc, sb->s + sb->size - sb->curr, sb->curr, sb->captures);
        if (matched) {
          sb->curr += sb->captures[1];
          pc = Iseq.at(sb->iseq, offsets.arg1);
        } else {
          pc = Iseq.at(sb->iseq, offsets.arg2);
        }
        DISPATCH;
      }

      CASE(MATCH_STR) {
        // todo check eof
        Arg3232 offsets = DECODE(Arg3232, pc);
        Val str = STACK_POP();
        matched = sb_string_match(str, sb->s + sb->size - sb->curr, sb->curr, &sb->capture_size, sb->captures);
        if (matched) {
          sb->curr += sb->captures[1];
          pc = Iseq.at(sb->iseq, offsets.arg1);
        } else {
          pc = Iseq.at(sb->iseq, offsets.arg2);
        }
        DISPATCH;
      }

      CASE(CALLBACK) {
      }

      CASE(CTX_CALL) {
        uint32_t ctx_name_str = DECODE(ArgU32, pc).arg1;
        CTX_PUSH(ctx_name_str);
        DISPATCH;
      }

      CASE(CTX_END) {
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

      CASE(JMP) {
        int32_t offset = DECODE(Arg32, pc).arg1;
        pc = Iseq.at(sb->iseq, offset);
        DISPATCH;
      }
    }
  }

terminate:

  return (ValPair){err ? VAL_NIL : Vals.pop(&sb->stack), VAL_NIL};
}

bool sb_string_match(Val pattern_str, int64_t size, const char* str, int32_t* capture_size, int32_t* captures) {
  // todo
  return false;
}
