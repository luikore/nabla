#include "compile.h"
#include "vm-peg-op-codes.h"
#include <adt/cons.h>
#include <adt/utils/mut-map.h>
#include "labels.h"

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

static uint32_t kInfixLogic = 0;
static uint32_t kCall = 0;
static uint32_t kCapture = 0;
static uint32_t kCreateNode = 0;
static uint32_t kCreateList = 0;
static uint32_t kSplatEntry = 0;
static uint32_t kIf = 0;

MUT_ARRAY_DECL(Stack, Val);

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
} LabelTable;

static void _label_init(LabelTable* lt) {
  RuleNumMap.init(&lt->m);
  Labels.init(&lt->l);
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

// SeqRule or Branch
static void _encode_rule_body_unit(struct Iseq* iseq, Val e, LabelTable* lt);
static void _encode_callback_lines(struct Iseq* iseq, Val stmts, int terms_size, LabelTable* lt);

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

// returns change of stack
// terms_size is for checking of capture overflows
static void _encode_callback_expr(struct Iseq* iseq, Val expr, int terms_size, LabelTable* lt) {
  uint32_t klass = VAL_KLASS(expr);
  // Expr = InfixLogic | Call | Capture | CraeteNode | CreateList | Assign | If | Nul
  // NOTE: no VarRef for PEG
  if (klass == kInfixLogic) {
    // InfixLogic[Expr, op, Expr]

    // a && b:
    //   lhs
    //   junless L0
    //   rhs
    //   L0:

    // a || b:
    //   lhs
    //   jif L0
    //   rhs
    //   L0:

    Val lhs = nb_struct_get(expr, 0);
    Val op = nb_struct_get(expr, 1);
    Val rhs = nb_struct_get(expr, 2);
    int l0 = _label_new_num(lt);
    uint16_t ins;

    _encode_callback_expr(iseq, lhs, terms_size, lt);
    _label_ref(lt, Iseq.size(iseq) + 1);
    if (nb_string_byte_size(op) == 2 && nb_string_ptr(op)[0] == '&' && nb_string_ptr(op)[1] == '&') {
      ins = JUNLESS;
    } else {
      ins = JIF;
    }
    ENCODE(iseq, Arg32, ((Arg32){ins, l0}));
    _encode_callback_expr(iseq, rhs, terms_size, lt);
    _label_def(lt, l0, Iseq.size(iseq));

  } else if (klass == kCall) {
    // Call[func_name, Expr*] # args reversed
    // NOTE
    //   methods in PEG is shared with methods in LEX.
    //   but only operators are supported,
    //   this makes sure the method call doesn't generate any side effects.
    //   methods are all defined under sb_klass and the receiver is context,
    //   when executing methods in PEG, we use a nil receiver since all pure operator methods don't need receiver.
    //   (context receiver is not flexible for being compatible with custom methods, TODO use Val receiver?)
    uint32_t func_name = VAL_TO_STR(nb_struct_get(expr, 0));
    Val exprs = nb_struct_get(expr, 1);
    uint32_t argc = 0;
    for (Val tail = exprs; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(tail);
      _encode_callback_expr(iseq, e, terms_size, lt);
      argc += 1;
    }
    ENCODE(iseq, ArgU32U32, ((ArgU32U32){CALL, argc, func_name}));

  } else if (klass == kCapture) {
    // Capture[var_name]

    // TODO $-\d+
    Val tok = nb_struct_get(expr, 0);
    int size = nb_string_byte_size(tok);
    // TODO raise error if size > 2
    char s[size];
    strncpy(s, nb_string_ptr(tok) + 1, size - 1);
    s[size - 1] = '\0';
    int i = atoi(s);
    if (i > terms_size) {
      // raise error
      // TODO maybe we don't need to pass terms_size everywhere
      // just check it after the bytecode is compiled
    }
    ENCODE(iseq, uint16_t, CAPTURE);
    ENCODE(iseq, uint16_t, (uint16_t)i);

  } else if (klass == kCreateNode) {
    // CreateNode[ty, (Expr | SplatEntry)*]

    // node[a, *b]:
    //   node_beg klass_name
    //   a
    //   node_set
    //   b
    //   node_setv
    //   node_end

    Val klass_name = nb_struct_get(expr, 0);
    Val elems = nb_struct_get(expr, 1);
    ENCODE(iseq, ArgU32, ((ArgU32){NODE_BEG, VAL_TO_STR(klass_name)}));
    elems = nb_cons_reverse(elems);
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(tail);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(iseq, to_splat, terms_size, lt);
        ENCODE(iseq, uint16_t, NODE_SETV);
      } else {
        _encode_callback_expr(iseq, e, terms_size, lt);
        ENCODE(iseq, uint16_t, NODE_SET);
      }
    }
    ENCODE(iseq, uint16_t, NODE_END);

  } else if (klass == kCreateList) {
    // CreateList[(Expr | SplatEntry)*]

    // [a, *b]: b, a, list
    // [*a, b]: b, a, r_list
    // NOTE: no need to reverse list here
    Val elems = nb_struct_get(expr, 0);
    if (elems) {
      Val e = nb_cons_head(elems);
      Val tail = nb_cons_tail(elems);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(iseq, to_splat, terms_size, lt);
        // we can continue with this list
      } else {
        ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
        _encode_callback_expr(iseq, e, terms_size, lt);
        ENCODE(iseq, uint16_t, LIST);
      }
      for (; tail; tail = nb_cons_tail(tail)) {
        e = nb_cons_head(tail);
        if (VAL_KLASS(e) == kSplatEntry) {
          Val to_splat = nb_struct_get(e, 0);
          _encode_callback_expr(iseq, to_splat, terms_size, lt);
          ENCODE(iseq, uint16_t, R_LIST);
        } else {
          _encode_callback_expr(iseq, e, terms_size, lt);
          ENCODE(iseq, uint16_t, LIST);
        }
      }
    } else {
      ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
    }

  } else {
    assert(klass == kIf);
    // If[Expr, Expr*, (Expr* | If)]

    // if cond, true_clause, else, false_clause:
    //   cond
    //   junless L0
    //   true_clause
    //   jmp L1
    //   L0: false_clause
    //   L1:

    Val cond = nb_struct_get(expr, 0);
    Val true_clause = nb_struct_get(expr, 1);
    Val false_clause = nb_struct_get(expr, 2);
    int l0 = _label_new_num(lt);
    int l1 = _label_new_num(lt);

    _encode_callback_expr(iseq, cond, terms_size, lt);
    _label_ref(lt, Iseq.size(iseq) + 1);
    ENCODE(iseq, Arg32, ((Arg32){JUNLESS, l0}));
    _encode_callback_lines(iseq, true_clause, terms_size, lt);
    _label_ref(lt, Iseq.size(iseq) + 1);
    ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
    _label_def(lt, l0, Iseq.size(iseq));
    if (VAL_KLASS(false_clause) == kIf) {
      _encode_callback_expr(iseq, false_clause, terms_size, lt);
    } else {
      _encode_callback_lines(iseq, false_clause, terms_size, lt);
    }
    _label_def(lt, l1, Iseq.size(iseq));
  }
}

static void _encode_callback_lines(struct Iseq* iseq, Val stmts, int terms_size, LabelTable* lt) {
  // Expr* (NOTE: no VarDecl in PEG callback)

  // NOTE: should only push the last expr to stack so this code can be correct: `[a, (b, c)]`
  // TODO: to support debugging we need to allocate more slots in stack to hold results of each line
  stmts = nb_cons_reverse(stmts);
  for (Val tail = stmts; tail; tail = nb_cons_tail(tail)) {
    Val e = nb_cons_head(tail);
    _encode_callback_expr(iseq, e, terms_size, lt);
    if (nb_cons_tail(tail) != VAL_NIL) {
      ENCODE(iseq, uint16_t, POP);
    }
  }
}

// callback_maybe: [Callback]
static void _encode_callback_maybe(struct Iseq* iseq, Val callback_maybe, int terms_size, LabelTable* lt) {
  if (callback_maybe != VAL_NIL) {
    Val callback = nb_cons_head(callback_maybe);

    Val stmts = nb_struct_get(callback, 0);
    if (stmts == VAL_NIL) {
      goto nil_callback;
    }

    _encode_callback_lines(iseq, stmts, terms_size, lt);
    ENCODE(iseq, uint16_t, RULE_RET);
    return;
  }

nil_callback:
  ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  ENCODE(iseq, uint16_t, RULE_RET);
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
Val sb_vm_peg_compile(struct Iseq* iseq, Val patterns_dict, Val node) {
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

    // callback klasses (without kAssign)
    kInfixLogic = klass_find_c("InfixLogic", sb);
    kCall       = klass_find_c("Call", sb);
    kCapture    = klass_find_c("Capture", sb);
    kCreateNode = klass_find_c("CreateNode", sb);
    kCreateList = klass_find_c("CreateList", sb);
    kSplatEntry = klass_find_c("SplatEntry", sb);
    kIf         = klass_find_c("If", sb);
  }

  // peg = [(PegRule | nil)*]
  // PegRule[name.rule, (Branch | SeqRule)]
  // Branch[op.branch, SeqRule, [Term], Callback]

  struct Stack stack; // TODO use stack to deal with recursive constructs so we can trace more info
  LabelTable lt;
  // Stack.init(&stack, 25);
  _label_init(&lt);

  uint32_t rule_size = 0;
  for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    Val e = nb_cons_head(curr);
    if (e != VAL_NIL) {
      assert(VAL_KLASS(e) == kPegRule);
      rule_size++;
    }
  }
  ENCODE(iseq, ArgU32, ((ArgU32){RULE_SIZE, rule_size}));

  for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    Val e = nb_cons_head(curr);
    if (e != VAL_NIL) {
      Val rule_name_tok = nb_struct_get(e, 0); // TODO use PegRule.name.rule
      _encode_rule_body_unit(iseq, nb_struct_get(e, 1), &lt);
    }
  }

terminate:

  _label_cleanup(&lt);
  // Stack.cleanup(&stack);

  return VAL_NIL;
}

void sb_vm_peg_decompile(struct Iseq* iseq, int32_t start, int32_t size) {
  uint16_t* pc_start = Iseq.at(iseq, start);
  uint16_t* pc_end = pc_start + size;
  uint16_t* pc = pc_start;
  while (pc < pc_end) {
    printf("%ld: %s", pc - pc_start, op_code_names[*pc]);
    switch (*pc) {

      case RULE_RET:
      case POP_BR:
      case UNPARSE:
      case POP:
      case NODE_SET:
      case NODE_SETV:
      case NODE_END:
      case LIST:
      case LIST_MAYBE:
      case R_LIST:
      case MATCH:
      case END: {
        printf("\n");
        pc++;
        break;
      }

      case RULE_SIZE:
      case TERM:
      case NODE_BEG:
      case JIF:
      case JUNLESS:
      case FAIL: {
        printf(" %u\n", DECODE(ArgU32, pc).arg1);
        break;
      }

      case PUSH_BR:
      case LOOP_UPDATE:
      case JMP: {
        printf(" %d\n", DECODE(Arg32, pc).arg1);
        break;
      }

      case RULE_CALL:
      case CALL: {
        ArgU32U32 args = DECODE(ArgU32U32, pc);
        printf(" %u %u\n", args.arg1, args.arg2);
        break;
      }

      case CAPTURE: {
        printf(" %u\n", DECODE(Arg16, pc).arg1);
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
