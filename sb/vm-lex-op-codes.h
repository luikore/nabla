#pragma once

#include "op-code-helper.h"

// ins: aligned by 16bits
// some note for alignment: http://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/
enum OpCodes {
  PUSH,           // val:(Val)                              # push literal
  POP,            //                                        # pure pop
  LOAD,           // id:(uint32)                            # push onto stack
  STORE,          // id:(uint32)                            # pop stack and set var
  CALL,           // argc:(uint32), func:(void*)            # pop args, push res
  NODE,           // argc:(uint32), klass:(uint32)          # pop args, push res
  LIST,           //                                        # pop b:Val, a:Val, push [a, *b]
  R_LIST,         //                                        # pop b:Val, a:Val, push [*a, b]
  JIF,            // false_clause:(uint32)                  # pop cond
  JMP,            // offset:(uint32)                        # unconditional jump
  MATCH_RE,       // cb_offset:(int32), next_offset:(int32) # then the bytecode goes into regexp mode
  MATCH_STR,      // cb_offset:(int32), next_offset:(int32) # pop s:(val_str)
  CTX_CALL,       // name_str:(uint32)                      # push context stack
  END,            //                                        # end loop matching, if no match in the round, pop context
  NOP,            //                                        # do nothing
  OP_CODES_SIZE   //
};
