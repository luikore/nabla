#pragma once

#include "op-code-helper.h"

enum OpCodes {
  // op        // args                        # description

  // metadata
  RULE_SIZE,   // sz:uint32                   # size of total rules, for initialize memoize table size

  // rule & aux ops, operates on stack / br stack
  TERM=1,      // str:uint32                  # match a terminal, push stack, else handle error [*]
  RULE_CALL,   // offset:uint32, id:uint32    # call another rule, pushes call frame [***]
  RULE_RET,    //                             # res = top, pop call frame, push res, memoize
  PUSH_BR,     // offset:int32                # push br stack (pc_offset, curr, stack_offset)
  POP_BR,      //                             # pop br stack, but not restoring any registers
  UNPARSE,     //                             # pop br stack, restore `curr` and `stack_offset`, but NOT `pc_offset`
  LOOP_UPDATE, // offset:int32                # try goto offset for looping [**]
  JMP,         // offset:int32                # jump to an offset in bytecode

  // callback ops, similar to the ones in vm-lex, operates on stack
  CAPTURE,     // n:uint16                    # load capture at bp[n]
  PUSH,        // val:Val                     # push literal
  POP,         //                             # pop top of stack
  NODE_BEG,    // klass_id:uint32             # push [node, (limit, counter=0)] [*****]
  NODE_SET,    //                             # (assume stack top is [node, (limit, counter), val]) node[counter++] = val
  NODE_SETV,   //                             # (assume stack top is [node, (limit, counter), *vals]) node[counter..counter+vals.size] = *vals
  NODE_END,    //                             # (assume stack top is [node, (limit, counter)]) finish building node, remove counter from stack top
  LIST,        //                             # pop b:Cons, a:Val, push [a, *b] (members are pushed from left to right)
  LIST_MAYBE,  //                             # similar to list, do nothing if br_stack.top.stack_offset + 1 != stack.offset
  LISTV,       //                             # pop b:Cons, a:Cons, push [*a, *b] (members are pushed from left to right)
  JIF,         // true_clause:uint32          # pops cond [****]
  JUNLESS,     // false_clause:uint32         # pops cond
  CALL,        // argc:uint32, fname:uint32   # invoke a method (only pure builtin operators are supported), argc includes receiver obj

  // terminating
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
// [****] if we don't pop cond, the following expression is not right: `[(if foo, bar), (if foo, bar)]`

// [*****] For node building: we can't use LIST/LISTV tricks here...
//         we allocate the node first, and then set attrs one by one or put several attrs by a splat.
//         if attr size exceeds limit of the node, deallocate the node and raise error.
//         (TODO we need some extra matching if node is defined like Foo[bar, *baz])

static const char* op_code_names[] = {
  [RULE_SIZE] = "rule_size",
  [TERM] = "term",
  [RULE_CALL] = "rule_call",
  [RULE_RET] = "rule_ret",
  [PUSH_BR] = "push_br",
  [POP_BR] = "pop_br",
  [UNPARSE] = "unparse",
  [LOOP_UPDATE] = "loop_update",
  [JMP] = "jmp",
  [CAPTURE] = "capture",
  [PUSH] = "push",
  [POP] = "pop",
  [NODE_BEG] = "node_beg",
  [NODE_SET] = "node_set",
  [NODE_SETV] = "node_setv",
  [NODE_END] = "node_end",
  [LIST] = "list",
  [LIST_MAYBE] = "list_maybe",
  [LISTV] = "listv",
  [JIF] = "jif",
  [JUNLESS] = "junless",
  [CALL] = "call",
  [MATCH] = "match",
  [FAIL] = "fail",
  [END] = "end"
};
