#include "compile.h"
#include "vm-peg-op-codes.h"
#include <adt/cons.h>
#include <adt/utils/mut-map.h>

#pragma mark ## decls

static uint32_t kPegRule = 0;
static uint32_t kBranch = 0;
static uint32_t kSeqRule = 0;
static uint32_t kTerm = 0;
static uint32_t kTermStar = 0;
static uint32_t kTermPlus = 0;
static uint32_t kTermMaybe = 0;
static uint32_t kLookahead = 0;
static uint32_t kNegLookahead = 0;
static uint32_t kRefRule = 0;
static uint32_t kCallback = 0;

static uint64_t _rule_num_key_hash(uint32_t k) {
  return val_hash_mem(&k, sizeof(uint32_t));
}

static bool _rule_num_key_eq(uint32_t k1, uint32_t k2) {
  return k1 == k2;
}

// map rule name to label num, so we can complete the offset in one pass
MUT_MAP_DECL(RuleNumMap, uint32_t, int, _rule_num_key_hash, _rule_num_key_eq);

typedef struct {
  // NOTE an entry in map is much heavier than one in array
  struct RuleNumMap m;
  struct Labels l;
  struct KlassRefs klass_refs;
} LabelTable;

static void _encode_rule_body_unit(struct Iseq* iseq, Val e, LabelTable* lt);

#pragma mark ## impls

static void _label_init(LabelTable* lt, KlassRefs* klass_refs) {
  RuleNumMap.init(&lt->m);
  Labels.init(&lt->l);
  lt->klass_refs = klass_refs;
}

static void _label_cleanup(LabelTable* lt) {
  RuleNumMap.cleanup(&lt->m);
  Labels.cleanup(&lt->l);
}

static int _label_new_rule_num(LabelTable* lt, uint32_t rule_name_strlit) {
  int n;
  if (RuleNumMap.find(&lt->m, rule_name_strlit, &n)) {
    return n;
  }

  // num not defined before? create one
  n = LABEL_NEW_NUM(&lt->l);
  RuleNumMap.insert(&lt->m, rule_name_strlit, n);
  return n;
}

static int _label_new_num(LabelTable* lt) {
  return LABEL_NEW_NUM(&lt->l);
}

static void _label_def(LabelTable* lt, int num, int offset) {
  LABEL_DEF(&lt->l, num, offset);
}

static void _label_ref(LabelTable* lt, int offset) {
  LABEL_REF(&lt->l, offset);
}

// callback_maybe: [Callback]
static void _encode_callback_maybe(struct Iseq* iseq, Val callback_maybe, int terms_size, LabelTable* lt) {
  if (callback_maybe != VAL_NIL) {
    Val callback = nb_cons_head(callback_maybe);

    Val stmts = nb_struct_get(callback, 0);
    if (stmts == VAL_NIL) {
      goto nil_callback;
    }

    sb_vm_callback_compile(iseq, stmts, terms_size, &lt->l, true);
    ENCODE(iseq, uint16_t, RULE_RET);
    return;
  }

nil_callback:
  ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  ENCODE(iseq, uint16_t, RULE_RET);
}

static void _encode_term(struct Iseq* iseq, Val term_node, LabelTable* lt) {
  if (VAL_KLASS(term_node) == kRefRule) {
    Val rule_name = nb_struct_get(term_node, 0);
    int num = _label_new_rule_num(lt, VAL_TO_STR(rule_name));
    _label_ref(lt, Iseq.size(iseq) + 1);
    ENCODE(iseq, ArgU32U32, ((ArgU32U32){RULE_CALL, num, VAL_TO_STR(term_node)}));
  } else { // token
    ENCODE(iseq, ArgU32, ((ArgU32){TERM, VAL_TO_STR(term_node)}));
  }
}

static void _encode_term_star(struct Iseq* iseq, Val term_star_node, LabelTable* lt) {
  // e*
  //   push nil
  //   push_br L0
  //   L1: e
  //   list_maybe # [e, *res]
  //   loop_update L1 # L0
  //   L0:

  Val e = nb_struct_get(term_star_node, 0);
  int l0 = _label_new_num(lt);
  int l1 = _label_new_num(lt);

  ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _label_def(lt, l1, Iseq.size(iseq));
  _encode_term(iseq, e, lt);

  ENCODE(iseq, uint16_t, LIST_MAYBE);
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){LOOP_UPDATE, l1}));
  _label_def(lt, l0, Iseq.size(iseq));
}

static void _encode_term_plus(struct Iseq* iseq, Val term_plus_node, LabelTable* lt) {
  // e+ # NOTE encode e twice for simplicity,
  //           this will not cause much code duplication, since there is no nesting
  //   push nil
  //   e
  //   list_maybe # [e]
  //   push_br L0
  //   L1: e
  //   list_maybe # [e, *res]
  //   loop_update L1 # L0
  //   L0:

  Val e = nb_struct_get(term_plus_node, 0);
  int l0 = _label_new_num(lt);
  int l1 = _label_new_num(lt);

  ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _encode_term(iseq, e, lt);
  ENCODE(iseq, uint16_t, LIST_MAYBE);
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _label_def(lt, l1, Iseq.size(iseq));
  _encode_term(iseq, e, lt);
  ENCODE(iseq, uint16_t, LIST_MAYBE);
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){LOOP_UPDATE, l1}));
  _label_def(lt, l0, Iseq.size(iseq));
}

static void _encode_term_maybe(struct Iseq* iseq, Val term_maybe_node, LabelTable* lt) {
  // e?
  //   push nil
  //   push_br L0
  //   e
  //   list_maybe # [e]
  //   pop_br
  //   L0:

  Val e = nb_struct_get(term_maybe_node, 0);
  int l0 = _label_new_num(lt);

  ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(iseq, e, lt);

  ENCODE(iseq, uint16_t, LIST_MAYBE);
  ENCODE(iseq, uint16_t, POP_BR);
  _label_def(lt, l0, Iseq.size(iseq) + 1);
}

static void _encode_lookahead(struct Iseq* iseq, Val node, LabelTable* lt) {
  // &e
  //   push_br L0
  //   e
  //   unparse
  //   jmp L1
  //   L0: term 0 # always fail
  //   L1:

  Val e = nb_struct_get(node, 0);
  int l0 = _label_new_num(lt);
  int l1 = _label_new_num(lt);

  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(iseq, e, lt);
  ENCODE(iseq, uint16_t, UNPARSE);
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
  _label_def(lt, l0, Iseq.size(iseq));
  ENCODE(iseq, ArgU32, ((ArgU32){TERM, 0}));
  _label_def(lt, l1, Iseq.size(iseq));
}

static void _encode_neg_lookahead(struct Iseq* iseq, Val node, LabelTable* lt) {
  // ^e
  //   push_br L0
  //   e
  //   unparse
  //   term 0 # always fail
  //   L0:

  Val e = nb_struct_get(node, 0);
  int l0 = _label_new_num(lt);

  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(iseq, e, lt);
  ENCODE(iseq, uint16_t, UNPARSE);
  ENCODE(iseq, ArgU32, ((ArgU32){TERM, 0}));
  _label_def(lt, l0, Iseq.size(iseq));
}

// terms: (Term | TermStar | TermPlus | TermMaybe | Lookahead | NegLookahead)*
// returns size of terms
static int _encode_terms(struct Iseq* iseq, Val terms, LabelTable* lt) {
  terms = nb_cons_reverse(terms);
  int terms_size = 0;
  for (Val node = terms; node != VAL_NIL; node = nb_cons_tail(node)) {
    Val e = nb_cons_head(node);
    uint32_t klass = VAL_KLASS(e);
    if (klass == kTerm) {
      _encode_term(iseq, e, lt);
    } else if (klass == kTermStar) {
      _encode_term_star(iseq, e, lt);
    } else if (klass == kTermPlus) {
      _encode_term_plus(iseq, e, lt);
    } else if (klass == kTermMaybe) {
      _encode_term_maybe(iseq, e, lt);
    } else if (klass == kLookahead) {
      _encode_lookahead(iseq, e, lt);
    } else if (klass == kNegLookahead) {
      _encode_neg_lookahead(iseq, e, lt);
    }
    terms_size++;
  }
  return terms_size;
}

static void _encode_seq_rule(struct Iseq* iseq, Val seq_rule, LabelTable* lt) {
  Val terms = nb_struct_get(seq_rule, 0);
  Val callback_maybe = nb_struct_get(seq_rule, 1);
  int terms_size = _encode_terms(iseq, terms, lt);
  _encode_callback_maybe(iseq, callback_maybe, terms_size, lt);
}

static void _encode_branch_or(struct Iseq* iseq, Val a, Val terms, Val callback_maybe, LabelTable* lt) {
  // a / terms callback_maybe
  //   push_br L0
  //   a
  //   pop_br
  //   jmp L1
  //   L0: terms
  //   callback_maybe(captures = terms.size)
  //   L1:

  assert(terms != VAL_NIL);

  int l0 = _label_new_num(lt);
  int l1 = _label_new_num(lt);

  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_rule_body_unit(iseq, a, lt);
  ENCODE(iseq, uint16_t, POP_BR);
  _label_ref(lt, Iseq.size(iseq) + 1);
  ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
  _label_def(lt, l0, Iseq.size(iseq));
  int terms_size = _encode_terms(iseq, terms, lt);
  _encode_callback_maybe(iseq, callback_maybe, terms_size, lt);
  _label_def(lt, l1, Iseq.size(iseq));
}

static void _encode_ljoin(struct Iseq* iseq, char kind, Val a, Val terms, Val callback_maybe, LabelTable* lt) {
  assert(terms != VAL_NIL);

  switch (kind) {
    case '*': {
      // a /* terms callback
      //   a
      //   L1: push_br L0
      //   terms
      //   callback(captures = terms.size + 1)
      //   jmp L1
      //   L0:
      int l0 = _label_new_num(lt);
      int l1 = _label_new_num(lt);
      _encode_rule_body_unit(iseq, a, lt);
      _label_def(lt, l1, Iseq.size(iseq));
      _label_ref(lt, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      int terms_size = _encode_terms(iseq, terms, lt);
      _encode_callback_maybe(iseq, callback_maybe, terms_size + 1, lt);
      _label_ref(lt, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
      _label_def(lt, l0, Iseq.size(iseq));
      break;
    }

    case '?': {
      // a /? terms callback
      //   a
      //   push_br L0
      //   terms
      //   callback(captures = terms.size + 1)
      //   L0:
      int l0 = _label_new_num(lt);
      _encode_rule_body_unit(iseq, a, lt);
      _label_ref(lt, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      int terms_size = _encode_terms(iseq, terms, lt);
      _encode_callback_maybe(iseq, callback_maybe, terms_size + 1, lt);
      _label_def(lt, l0, Iseq.size(iseq));
      break;
    }

    case '+': {
      // a /+ terms callback
      //   a
      //   L1: terms
      //   callback(captures = terms.size + 1)
      //   push_br L0
      //   jmp L1
      //   L0:
      int l0 = _label_new_num(lt);
      int l1 = _label_new_num(lt);
      _encode_rule_body_unit(iseq, a, lt);
      _label_def(lt, l1, Iseq.size(iseq));
      int terms_size = _encode_terms(iseq, terms, lt);
      _encode_callback_maybe(iseq, callback_maybe, terms_size + 1, lt);
      _label_ref(lt, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      _label_ref(lt, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
      _label_def(lt, l0, Iseq.size(iseq));

      break;
    }
  }
}

static void _encode_rule_body_unit(struct Iseq* iseq, Val e, LabelTable* lt) {
  uint32_t klass = VAL_KLASS(e);
  if (klass == kBranch) {
    Val op = nb_struct_get(e, 0);
    Val lhs = nb_struct_get(e, 1);
    Val terms = nb_struct_get(e, 2);
    Val callback = nb_struct_get(e, 3);
    int op_size = nb_string_byte_size(op);
    const char* op_ptr = nb_string_ptr(op);

    if (op_size == 1 && op_ptr[0] == '/') {
      _encode_branch_or(iseq, lhs, terms, callback, lt);
    } else if (op_size == 2 && op_ptr[0] == '/') {
      assert(op_ptr[1] == '+' || op_ptr[1] == '*' || op_ptr[1] == '?');
      // TODO report parse error
      _encode_ljoin(iseq, op_ptr[1], lhs, terms, callback, lt);
    } else {
      // TODO encode op table
    }

  } else {
    assert(klass == kSeqRule);
    _encode_seq_rule(iseq, e, lt);
  }
}

// Bytecode layout:
//
//   rule1(main): ...
//   rule2: ...
//   rule3: ...
//
// call_rule instruction jumps bytecode into the offset of target rule,
// but since we don't know the rule offset yet when compiling,
// we need build a {rule_name => num} mapping.
// (TODO this mapping should be put in init metadata)
Val sb_vm_peg_compile(struct Iseq* iseq, Val patterns_dict, Val node, void* klass_refs) {
  if (!kPegRule) {
    uint32_t sb   = sb_klass();
    kPegRule      = klass_find_c("PegRule", sb); assert(kPegRule);
    kBranch       = klass_find_c("Branch", sb);
    kSeqRule      = klass_find_c("SeqRule", sb);
    kTerm         = klass_find_c("Term", sb);
    kTermStar     = klass_find_c("TermStar", sb);
    kTermPlus     = klass_find_c("TermPlus", sb);
    kTermMaybe    = klass_find_c("TermMaybe", sb);
    kLookahead    = klass_find_c("Lookahead", sb);
    kNegLookahead = klass_find_c("NegLookahead", sb);
    kRefRule      = klass_find_c("RefRule", sb);
    kCallback     = klass_find_c("Callback", sb);
  }

  // peg = [(PegRule | nil)*]
  // PegRule[name.rule, (Branch | SeqRule)]
  // Branch[op.branch, SeqRule, [Term], Callback]

  struct Vals stack; // TODO use stack to deal with recursive constructs so we can trace more info
  LabelTable lt;
  // Vals.init(&stack, 25);
  _label_init(&lt, klass_refs);

  uint32_t rule_size = 0;
  int iseq_original_size = Iseq.size(iseq);
  ENCODE_META(iseq);

  for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    Val e = nb_cons_head(curr);
    if (e != VAL_NIL) {
      Val rule_name_tok = nb_struct_get(e, 0); // TODO use PegRule.name.rule
      _encode_rule_body_unit(iseq, nb_struct_get(e, 1), &lt);
      rule_size++;
    }
  }

  _label_cleanup(&lt);
  // Vals.cleanup(&stack);

  union { void* as_void; uint32_t as_u32; } cast = {.as_u32 = rule_size};
  ENCODE_FILL_META(iseq, iseq_original_size, cast.as_void);
  return VAL_NIL;
}

void sb_vm_peg_decompile(uint16_t* pc_start) {
  uint16_t* pc = pc_start;
  uint32_t size = DECODE(ArgU32, pc).arg1;
  uint16_t* pc_end = pc_start + size;
  DECODE(void*, pc);

  while (pc < pc_end) {
    printf("%ld: %s", pc - pc_start, op_code_names[*pc]);
    switch (*pc) {

      case RULE_RET:
      case POP_BR:
      case UNPARSE:
      case LIST_MAYBE:
      case POP:
      case MATCH: {
        printf("\n");
        pc++;
        break;
      }

      case END: {
        printf("\n");
        pc++;
        if (pc != pc_end) {
          fatal_err("end ins %d and pc_end %d not match", (int)pc, (int)pc_end);
        }
        break;
      }

      case TERM:
      case FAIL: {
        printf(" %u\n", DECODE(ArgU32, pc).arg1);
        break;
      }

      case CALLBACK: {
        uint32_t next_offset = DECODE(ArgU32, pc).arg1;
        printf(" %u\n", next_offset);
        printf(" --- begin callback ---");
        sb_vm_callback_decompile(pc);
        printf(" --- end callback ---");
        pc = pc_start + next_offset;
        break;
      }

      case PUSH_BR:
      case LOOP_UPDATE:
      case JMP: {
        printf(" %d\n", DECODE(Arg32, pc).arg1);
        break;
      }

      case RULE_CALL: {
        ArgU32U32 args = DECODE(ArgU32U32, pc);
        printf(" %u %u\n", args.arg1, args.arg2);
        break;
      }

      case PUSH: {
        printf(" %lu\n", DECODE(ArgVal, pc).arg1);
        break;
      }

      default: {
        fatal_err("bad pc: %d", (int)*pc);
      }
    }
  }
}
