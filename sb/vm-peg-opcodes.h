enum OpCodes {
  // op        // args                        # description

  // metadata
  RULE_SIZE,   // sz:uint32                   # size of total rules, for initialize memoize table size

  // rule & aux ops, operates on stack / br stack
  TERM=1,      // str:uint32                  # match a terminal, push stack, else handle error[*]
  RULE_CALL,   // offset:uint32, id:uint32    # call another rule, pushes call frame
  RULE_RET,    //                             # res = top, pop call frame, push res, memoize
  PUSH_BR,     // offset:int32                # push br stack (offset, curr)
  POP_BR,      //                             # pop br stack
  LOOP_UPDATE, // offset:int32                # try goto offset for looping [**]
  JMP,         // offset:int32                # jump to an offset in bytecode

  // callback ops, similar to the ones in vm-lex, operates on stack
  CAPTURE,     // n:uint16                    # load capture at bp[n]
  PUSH,        // val:Val                     # push literal
  POP,         //                             # pop top of stack
  NODE,        // argc:uint32, klass:uint32   # pop args, push node
  LIFT,        //                             # pop e, push [e]
  LIST,        //                             # pop b:Val, a:Val, push [a, *b]
  R_LIST,      //                             # pop b:Val, a:Val, push [*a, b]
  JIF,         // false_clause:uint32         # pop cond

  // terminating
  MATCH,       // id:uint32                   # end parsing, check if token stream is terminated, and replace it
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

static const char* op_code_names[] = {
  [RULE_SIZE] = "rule_size",
  [TERM] = "term",
  [RULE_CALL] = "rule_call",
  [RULE_RET] = "rule_ret",
  [PUSH_BR] = "push_br",
  [POP_BR] = "pop_br",
  [LOOP_UPDATE] = "loop_update",
  [JMP] = "jmp",
  [CAPTURE] = "capture",
  [PUSH] = "push",
  [POP] = "pop",
  [NODE] = "node",
  [LIFT] = "lift",
  [LIST] = "list",
  [R_LIST] = "r_list",
  [JIF] = "jif",
  [MATCH] = "match",
  [FAIL] = "fail",
  [END] = "end"
};

typedef struct {
  uint16_t op;
  int32_t arg1;
} __attribute__((packed)) Arg32;

typedef struct {
  uint16_t op;
  uint32_t arg1;
} __attribute__((packed)) ArgU32;

typedef struct {
  uint16_t op;
  uint32_t arg1;
  uint32_t arg2;
} __attribute__((packed)) ArgU32U32;

typedef struct {
  uint16_t op;
  Val arg1;
} __attribute__((packed)) ArgVal;
