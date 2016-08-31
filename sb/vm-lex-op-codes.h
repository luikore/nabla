#pragma once

#include "op-code-helper.h"

// ins: aligned by 16bits
// some note for alignment: http://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/
enum OpCodes {
  META,         // size:uint32, data:void*

  MATCH_RE,     // match:uint32, unmatch:uint32  # match_re (..regexp bytecode..) (..cb..), if not matched, go to unmatch [*]
  MATCH_STR,    // match:uint32, unmatch:uint32  # match_str (..string..) (..cb..) if not matched, go to unmatch
  CALLBACK,     // next_offset:uint32            # invoke vm callback (lex variation) with previous captures and shared stack
  CTX_CALL,     // name_str:uint32               # pushes context stack
  CTX_END,      //                               # end loop matching, if no match in the round, pop context
  JMP,

  OP_CODES_SIZE //
};

// [*] if matched, pc += match, else pc += unmatch

static const char* op_code_names[] = {
  [META] = "meta",
  [MATCH_RE] = "match_re",
  [MATCH_STR] = "match_str",
  [CALLBACK] = "callback",
  [CTX_CALL] = "ctx_call",
  [CTX_END] = "ctx_end",
  [JMP] = "jmp"
};
