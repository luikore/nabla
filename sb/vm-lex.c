#include "compile.h"

struct LexerRuleStruct;
typedef struct LexerRuleStruct LexerRule;
struct LexerRuleStruct {
  LexerRule* next; // point to next start-rule. (start-rule means it is the first rule in a rule sequence)
  void* regexp;
  void* callback;
  bool is_end;
};

// lexer
// all rules are flatten into an array
struct VmLexStruct {
  Val begin_actions;
  size_t rule_size;
  LexerRule rules[];
};

VmLex* nb_vm_lex_compile(void* arena, Val lex_node, Spellbreak* spellbreak) {
  return NULL;
}

int64_t nb_vm_lex_exec(VmLex* lex, Ctx* ctx) {
  return 0;
}
