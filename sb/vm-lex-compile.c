#include "compile.h"
#include "vm-lex-op-codes.h"

uint32_t kRefContext    = 0;
uint32_t kSeqLexRules   = 0;
uint32_t kBeginCallback = 0;
uint32_t kEndCallback   = 0;
uint32_t kLexRule       = 0;
uint32_t kVarDecl       = 0;
uint32_t kVarRef        = 0;
uint32_t kGlobalVarRef  = 0;

static void _cache_klasses() {
  if (!kRefContext) {
    uint32_t sb    = sb_klass();
    kRefContext    = klass_find_c("kRefContext", sb); assert(kRefContext);
    kSeqLexRules   = klass_find_c("SeqLexRules", sb);
    kBeginCallback = klass_find_c("BeginCallback", sb);
    kEndCallback   = klass_find_c("EndCallback", sb);
    kLexRule       = klass_find_c("LexRule", sb);
    kVarDecl       = klass_find_c("VarDecl", sb);
    kVarRef        = klass_find_c("VarRef", sb);
    kGlobalVarRef  = klass_find_c("GlobalVarRef", sb);
  }
}

typedef struct {
  struct Iseq* iseq;
  Val patterns_dict;
  struct VarsTable* global_vars;
  struct VarsTable* local_vars;
} LexCompiler;

static void _encode_begin_rule(LexCompiler* compiler, Val begin_rule) {
  // BeginCallback[Callback?, LexRule+]
}

static void _encode_normal_rule(LexCompiler* compiler, Val rule) {
  // EndCallback[Callback?, LexRule+]
  // SeqLexRules[LexRule+]
  // LexRule[(VarRef[name] | GlobalVarRef[name] | String | Regexp), Callback?]
  uint32_t klass = VAL_KLASS(rule);
  if (klass == kEndCallback) {
    //content
  } else if (klass == kSeqLexRules) {
    //content
  } else if (klass == kLexRule) {
    //content
  }
}

Val sb_vm_lex_compile(struct Iseq* iseq, Val patterns_dict, struct VarsTable* global_vars, struct VarsTable* local_vars, Val node) {
  _cache_klasses();
  LexCompiler compiler = {
    .iseq = iseq,
    .patterns_dict = patterns_dict,
    .global_vars = global_vars,
    .local_vars = local_vars
  };

  // NOTE we already inlined partial references

  Val context_name = nb_struct_get(node, 0);
  Val rules = nb_struct_get(node, 1);

  // NOTE we already have vars defined in build-symbols step

  // Lex[context, (RefContext[name] | SeqLexRules | BeginCallback | EndCallback | nil)*]
  // NOTE the rule_lines is in reverse order
  Val begin_rules = VAL_NIL;
  Val other_rules = VAL_NIL;
  for (Val list = rules; list; list = nb_cons_tail(list)) {
    Val rule_line = nb_cons_head(list);
    uint32_t klass = VAL_KLASS(rule_line);
    if (klass == kRefContext || klass == kSeqLexRules || klass == kEndCallback) {
      other_rules = nb_cons_new(rule_line, other_rules);
    } else if (klass == kBeginCallback) {
      begin_rules = nb_cons_new(rule_line, begin_rules);
    } else {
      assert(rule_line == VAL_NIL);
    }
  }

  for (Val list = begin_rules; list; list = nb_cons_tail(list)) {
    _encode_begin_rule(&compiler, nb_cons_head(list));
  }
  for (Val list = other_rules; list; list = nb_cons_tail(list)) {
    _encode_normal_rule(&compiler, nb_cons_head(list));
  }

  return VAL_NIL;
}
