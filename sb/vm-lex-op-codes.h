#pragma once

#include "op-code-helper.h"

// ins: aligned by 16bits
// some note for alignment: http://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/

// I may question:
// this vm is so simple and it is so high level, why not use a tree walker?
// But I think the regexp patterns still allocate a lot.
// And the bytecode format makes loading / dumping easier.
//
// compiling also takes less resource since we can share a same labels instance.
//

/* op code example:

meta bytecodesize

lex-Foo:
  // a begin callback
  ... callback code
  match_re begin_match, begin_unmatch
    ... re code
  begin_unmatch:
    ... error report
  begin_match:

  // another begin callback
  ...

  jmp first_loop
  loop:
  check_advance
  first_loop:

  // a rule
  match_re re_match, re_unmatch
    ... re code
    re_match:
      callback mask, callback_end
      ... callback code
      callback_end:
      ...
      jmp loop
    re_unmatch:

  // call another lex
  ctx_call Bar, 3

  // an end rule
  match_re end_match, end_unmatch
    ... re code
    end_match:
      ctx_end
    end_unmatch:

  jmp loop

lex-Bar:
  ...

*/
enum OpCodes {
  META,          // size:uint32, data:void*

  MATCH_RE,      // match:uint32, unmatch:uint32    # match_re (..regexp bytecode..) (..cb..), if not matched, go to unmatch [*]
  MATCH_STR,     // match:uint32, unmatch:uint32    # match_str (..string..) (..cb..) if not matched, go to unmatch
  CALLBACK,      // captures_mask: uint16, next_offset:uint32
                 //                                 # invoke vm callback (lex variation) with previous captures and shared stack
  CTX_CALL,      // name_str:uint32, nvars:uint32   # reserve local vars space in stack, pushes context stack
  CTX_END,       //                                 # end loop matching, if no match in the round, pop context
  JMP,           // offset:int32

  OP_CODES_SIZE  //
};

// [*] if matched, pc += match, else pc += unmatch. and sets the captures

static const char* op_code_names[] = {
  [META] = "meta",
  [MATCH_RE] = "match_re",
  [MATCH_STR] = "match_str",
  [CALLBACK] = "callback",
  [CTX_CALL] = "ctx_call",
  [CTX_END] = "ctx_end",
  [JMP] = "jmp"
};
