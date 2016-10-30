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
  struct Iseq* iseq;
  void* structs_table;
  // NOTE an entry in map is much heavier than one in array
  struct RuleNumMap m;
  struct Labels l;
} PegCompileCtx;

static void _encode_rule_body_unit(PegCompileCtx* ctx, Val e);

#pragma mark ## impls

static void _ctx_init(PegCompileCtx* ctx, struct Iseq* iseq, void* structs_table) {
  ctx->iseq = iseq;
  ctx->structs_table = structs_table;
  RuleNumMap.init(&ctx->m);
  Labels.init(&ctx->l);
}

static void _ctx_cleanup(PegCompileCtx* ctx) {
  RuleNumMap.cleanup(&ctx->m);
  Labels.cleanup(&ctx->l);
}

static int _iseq_size(PegCompileCtx* ctx) {
  return Iseq.size(ctx->iseq);
}

static int _label_new_rule_num(PegCompileCtx* ctx, uint32_t rule_name_strlit) {
  int n;
  if (RuleNumMap.find(&ctx->m, rule_name_strlit, &n)) {
    return n;
  }

  // num not defined before? create one
  n = LABEL_NEW_NUM(&ctx->l);
  RuleNumMap.insert(&ctx->m, rule_name_strlit, n);
  return n;
}

static int _label_new_num(PegCompileCtx* ctx) {
  return LABEL_NEW_NUM(&ctx->l);
}

static void _label_def(PegCompileCtx* ctx, int num, int offset) {
  LABEL_DEF(&ctx->l, num, offset);
}

static void _label_ref(PegCompileCtx* ctx, int offset) {
  LABEL_REF(&ctx->l, offset);
}

// callback_maybe: [Callback]
static void _encode_callback_maybe(PegCompileCtx* ctx, Val callback_maybe, int terms_size) {
  if (callback_maybe != VAL_NIL) {
    Val callback = nb_cons_head(callback_maybe);

    Val stmts = nb_struct_get(callback, 0);
    if (stmts == VAL_NIL) {
      goto nil_callback;
    }

    sb_vm_callback_compile(ctx->iseq, stmts, terms_size, &ctx->l, true, ctx->structs_table);
    ENCODE(ctx->iseq, uint16_t, RULE_RET);
    return;
  }

nil_callback:
  ENCODE(ctx->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  ENCODE(ctx->iseq, uint16_t, RULE_RET);
}

static void _encode_term(PegCompileCtx* ctx, Val term_node) {
  if (VAL_KLASS(term_node) == kRefRule) {
    Val rule_name = nb_struct_get(term_node, 0);
    int num = _label_new_rule_num(ctx, VAL_TO_STR(rule_name));
    _label_ref(ctx, _iseq_size(ctx) + 1);
    ENCODE(ctx->iseq, ArgU32U32, ((ArgU32U32){RULE_CALL, num, VAL_TO_STR(term_node)}));
  } else { // token
    ENCODE(ctx->iseq, ArgU32, ((ArgU32){TERM, VAL_TO_STR(term_node)}));
  }
}

static void _encode_term_star(PegCompileCtx* ctx, Val term_star_node) {
  // e*
  //   push nil
  //   push_br L0
  //   L1: e
  //   list_maybe # [e, *res]
  //   loop_update L1 # L0
  //   L0:

  Val e = nb_struct_get(term_star_node, 0);
  int l0 = _label_new_num(ctx);
  int l1 = _label_new_num(ctx);

  ENCODE(ctx->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _label_def(ctx, l1, _iseq_size(ctx));
  _encode_term(ctx, e);

  ENCODE(ctx->iseq, uint16_t, LIST_MAYBE);
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){LOOP_UPDATE, l1}));
  _label_def(ctx, l0, _iseq_size(ctx));
}

static void _encode_term_plus(PegCompileCtx* ctx, Val term_plus_node) {
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
  int l0 = _label_new_num(ctx);
  int l1 = _label_new_num(ctx);

  ENCODE(ctx->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _encode_term(ctx, e);
  ENCODE(ctx->iseq, uint16_t, LIST_MAYBE);
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _label_def(ctx, l1, _iseq_size(ctx));
  _encode_term(ctx, e);
  ENCODE(ctx->iseq, uint16_t, LIST_MAYBE);
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){LOOP_UPDATE, l1}));
  _label_def(ctx, l0, _iseq_size(ctx));
}

static void _encode_term_maybe(PegCompileCtx* ctx, Val term_maybe_node) {
  // e?
  //   push nil
  //   push_br L0
  //   e
  //   list_maybe # [e]
  //   pop_br
  //   L0:

  Val e = nb_struct_get(term_maybe_node, 0);
  int l0 = _label_new_num(ctx);

  ENCODE(ctx->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(ctx, e);

  ENCODE(ctx->iseq, uint16_t, LIST_MAYBE);
  ENCODE(ctx->iseq, uint16_t, POP_BR);
  _label_def(ctx, l0, _iseq_size(ctx) + 1);
}

static void _encode_lookahead(PegCompileCtx* ctx, Val node) {
  // &e
  //   push_br L0
  //   e
  //   unparse
  //   jmp L1
  //   L0: term 0 # always fail
  //   L1:

  Val e = nb_struct_get(node, 0);
  int l0 = _label_new_num(ctx);
  int l1 = _label_new_num(ctx);

  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(ctx, e);
  ENCODE(ctx->iseq, uint16_t, UNPARSE);
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){JMP, l1}));
  _label_def(ctx, l0, _iseq_size(ctx));
  ENCODE(ctx->iseq, ArgU32, ((ArgU32){TERM, 0}));
  _label_def(ctx, l1, _iseq_size(ctx));
}

static void _encode_neg_lookahead(PegCompileCtx* ctx, Val node) {
  // ^e
  //   push_br L0
  //   e
  //   unparse
  //   term 0 # always fail
  //   L0:

  Val e = nb_struct_get(node, 0);
  int l0 = _label_new_num(ctx);

  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_term(ctx, e);
  ENCODE(ctx->iseq, uint16_t, UNPARSE);
  ENCODE(ctx->iseq, ArgU32, ((ArgU32){TERM, 0}));
  _label_def(ctx, l0, _iseq_size(ctx));
}

// terms: (Term | TermStar | TermPlus | TermMaybe | Lookahead | NegLookahead)*
// returns size of terms
static int _encode_terms(PegCompileCtx* ctx, Val terms) {
  terms = nb_cons_reverse(terms);
  int terms_size = 0;
  for (Val node = terms; node != VAL_NIL; node = nb_cons_tail(node)) {
    Val e = nb_cons_head(node);
    uint32_t klass = VAL_KLASS(e);
    if (klass == kTerm) {
      _encode_term(ctx, e);
    } else if (klass == kTermStar) {
      _encode_term_star(ctx, e);
    } else if (klass == kTermPlus) {
      _encode_term_plus(ctx, e);
    } else if (klass == kTermMaybe) {
      _encode_term_maybe(ctx, e);
    } else if (klass == kLookahead) {
      _encode_lookahead(ctx, e);
    } else if (klass == kNegLookahead) {
      _encode_neg_lookahead(ctx, e);
    }
    terms_size++;
  }
  return terms_size;
}

static void _encode_seq_rule(PegCompileCtx* ctx, Val seq_rule) {
  Val terms = nb_struct_get(seq_rule, 0);
  Val callback_maybe = nb_struct_get(seq_rule, 1);
  int terms_size = _encode_terms(ctx, terms);
  _encode_callback_maybe(ctx, callback_maybe, terms_size);
}

static void _encode_branch_or(PegCompileCtx* ctx, Val a, Val terms, Val callback_maybe) {
  // a / terms callback_maybe
  //   push_br L0
  //   a
  //   pop_br
  //   jmp L1
  //   L0: terms
  //   callback_maybe(captures = terms.size)
  //   L1:

  assert(terms != VAL_NIL);

  int l0 = _label_new_num(ctx);
  int l1 = _label_new_num(ctx);

  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
  _encode_rule_body_unit(ctx, a);
  ENCODE(ctx->iseq, uint16_t, POP_BR);
  _label_ref(ctx, _iseq_size(ctx) + 1);
  ENCODE(ctx->iseq, Arg32, ((Arg32){JMP, l1}));
  _label_def(ctx, l0, _iseq_size(ctx));
  int terms_size = _encode_terms(ctx, terms);
  _encode_callback_maybe(ctx, callback_maybe, terms_size);
  _label_def(ctx, l1, _iseq_size(ctx));
}

static void _encode_ljoin(PegCompileCtx* ctx, char kind, Val a, Val terms, Val callback_maybe) {
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
      int l0 = _label_new_num(ctx);
      int l1 = _label_new_num(ctx);
      _encode_rule_body_unit(ctx, a);
      _label_def(ctx, l1, _iseq_size(ctx));
      _label_ref(ctx, _iseq_size(ctx) + 1);
      ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      int terms_size = _encode_terms(ctx, terms);
      _encode_callback_maybe(ctx, callback_maybe, terms_size + 1);
      _label_ref(ctx, _iseq_size(ctx) + 1);
      ENCODE(ctx->iseq, Arg32, ((Arg32){JMP, l1}));
      _label_def(ctx, l0, _iseq_size(ctx));
      break;
    }

    case '?': {
      // a /? terms callback
      //   a
      //   push_br L0
      //   terms
      //   callback(captures = terms.size + 1)
      //   L0:
      int l0 = _label_new_num(ctx);
      _encode_rule_body_unit(ctx, a);
      _label_ref(ctx, _iseq_size(ctx) + 1);
      ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      int terms_size = _encode_terms(ctx, terms);
      _encode_callback_maybe(ctx, callback_maybe, terms_size + 1);
      _label_def(ctx, l0, _iseq_size(ctx));
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
      int l0 = _label_new_num(ctx);
      int l1 = _label_new_num(ctx);
      _encode_rule_body_unit(ctx, a);
      _label_def(ctx, l1, _iseq_size(ctx));
      int terms_size = _encode_terms(ctx, terms);
      _encode_callback_maybe(ctx, callback_maybe, terms_size + 1);
      _label_ref(ctx, _iseq_size(ctx) + 1);
      ENCODE(ctx->iseq, Arg32, ((Arg32){PUSH_BR, l0}));
      _label_ref(ctx, _iseq_size(ctx) + 1);
      ENCODE(ctx->iseq, Arg32, ((Arg32){JMP, l1}));
      _label_def(ctx, l0, _iseq_size(ctx));

      break;
    }
  }
}

static void _encode_rule_body_unit(PegCompileCtx* ctx, Val e) {
  uint32_t klass = VAL_KLASS(e);
  if (klass == kBranch) {
    Val op = nb_struct_get(e, 0);
    Val lhs = nb_struct_get(e, 1);
    Val terms = nb_struct_get(e, 2);
    Val callback = nb_struct_get(e, 3);
    int op_size = nb_string_byte_size(op);
    const char* op_ptr = nb_string_ptr(op);

    if (op_size == 1 && op_ptr[0] == '/') {
      _encode_branch_or(ctx, lhs, terms, callback);
    } else if (op_size == 2 && op_ptr[0] == '/') {
      assert(op_ptr[1] == '+' || op_ptr[1] == '*' || op_ptr[1] == '?');
      // TODO report parse error
      _encode_ljoin(ctx, op_ptr[1], lhs, terms, callback);
    } else {
      // TODO encode op table
    }

  } else {
    assert(klass == kSeqRule);
    _encode_seq_rule(ctx, e);
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
Val sb_vm_peg_compile(struct Iseq* iseq, Val patterns_dict, void* structs_table, Val node) {
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
  PegCompileCtx peg_compile_ctx;
  // Vals.init(&stack, 25);
  _ctx_init(&peg_compile_ctx, iseq, structs_table);

  uint32_t rule_size = 0;
  int iseq_original_size = _iseq_size(&peg_compile_ctx);
  ENCODE_META(iseq);

  for (Val curr = node; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    Val e = nb_cons_head(curr);
    if (e != VAL_NIL) {
      Val rule_name_tok = nb_struct_get(e, 0); // TODO use PegRule.name.rule
      _encode_rule_body_unit(&peg_compile_ctx, nb_struct_get(e, 1));
      rule_size++;
    }
  }

  _ctx_cleanup(&peg_compile_ctx);
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
