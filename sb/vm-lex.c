#include "compile.h"

// vm for lex and callback

// aligned in 32bits
enum OpCodes {
  PUSH,           // val:(Val)
  POP,
  LOAD,           // id:(uint32) # push onto stack
  STORE,          // id:(uint32) # pop stack and set var
  CALL,           // argc:(uint32), func:(void*)   # pop args, push res
  NODE,           // argc:(uint32), klass:(uint32) # pop args, push res
  LIST,           // [*a, b] # pop b:Val, a:Val
  RLIST,          // [a, *b] # pop b:Val, a:Val
  JIF,            // offset:(int32) # pop cond
  MATCH_RE,       // cb_offset:(int32), next_offset:(int32) # then the bytecode is interpreted as regexp
  MATCH_STR,      // s:(val_str)
  RET,            // pop context stack and replace with result value (if not VAL_UNDEF)
  OP_CODES_SIZE   //
};

// lexer
struct VmLexStruct {
  struct Iseq iseq;
};

VmLex* sb_vm_lex_compile(CompileCtx* ctx, Val lex_node, Val* err) {
  return NULL;
}

Val sb_vm_lex_exec(Spellbreak* sb, VmLex* lex, Val* err) {
  return 0;
}
