#pragma once

#include "op-code-helper.h"

enum OpCodes {
  // op        // args                        # description
  META,        // size:uint32, data:void*     # rule_size = (uint32_t)data

  // rule & aux ops, operates on stack / br stack
  TERM=1,      // str:uint32                  # match a terminal, push stack, else handle error [*]
  RULE_CALL,   // offset:uint32, id:uint32    # call another rule, pushes call frame [***]
  RULE_RET,    //                             # res = top, pop call frame, push res, memoize
  PUSH_BR,     // offset:int32                # push br stack (pc_offset, curr, stack_offset)
  POP_BR,      //                             # pop br stack, but not restoring any registers
  UNPARSE,     //                             # pop br stack, restore `curr` and `stack_offset`, but NOT `pc_offset`
  LOOP_UPDATE, // offset:int32                # try goto offset for looping [**]
  JMP,         // offset:int32                # jump to an offset in bytecode
  LIST_MAYBE,  //                             # if br_stack.top.stack_offset + 1 == stack.offset, create list
               //                             # else do nothing
  PUSH,        // val:Val                     # push val on to stack
  POP,         //                             # pop stack

  CALLBACK,    // next_offset:uint32          # invoke callback VM, then move to pc + next_offset
  MATCH,       //                             # end parsing, check if token stream is terminated, and replace it
  FAIL,        // info:uint32                 # quickly fail parsing and report
  END,         //                             # end of iseq, won't reach

  OP_CODES_SIZE
};

// [*] if fail, a lot of heavy lifting will be done here:
//     first it tries to update the furthest-expect
//     (a level-2 furthest-expect can be reported like "expect token foo.bar in Baz in Xip"),
//     and try to pop (offset, curr) to update pc and curr pos,
//     if reaches call frame (=br_bp), pop it, and do fail check again,
//     if no call frame to pop, tell error of max expect.
//
// [**] if curr == old_curr, goto pop_br.old_offset,
//      else update top of br stack with (old_offset, curr), then goto offset.
// [***] the first arg of RULE_CALL, PUSH_BR, LOOP_UPDATE, JMP, JIF are all offsets,
//       code generator can take this advantage to use a single LABEL_REF to compute the offsets

static const char* op_code_names[] = {
  [META] = "meta",
  [TERM] = "term",
  [RULE_CALL] = "rule_call",
  [RULE_RET] = "rule_ret",
  [PUSH_BR] = "push_br",
  [POP_BR] = "pop_br",
  [UNPARSE] = "unparse",
  [LOOP_UPDATE] = "loop_update",
  [JMP] = "jmp",
  [LIST_MAYBE] = "list_maybe",
  [PUSH] = "push",
  [POP] = "pop",
  [CALLBACK] = "callback",
  [MATCH] = "match",
  [FAIL] = "fail",
  [END] = "end"
};
