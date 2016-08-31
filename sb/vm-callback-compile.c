#include "compile.h"
#include "vm-callback-op-codes.h"

#pragma mark ## decls

static uint32_t kInfixLogic = 0;
static uint32_t kCall = 0;
static uint32_t kCapture = 0;
static uint32_t kCreateNode = 0;
static uint32_t kCreateList = 0;
static uint32_t kSplatEntry = 0;
static uint32_t kIf = 0;

typedef struct {
  int terms_size;
  bool peg_mode;
  struct Labels* labels;
  struct ClassRefs* class_refs;
} Ctx;

static void _encode_callback_lines(struct Iseq* iseq, Val stmts, Ctx* ctx);

#pragma mark ## impls

// returns change of stack
// terms_size is for checking of capture overflows
static void _encode_callback_expr(struct Iseq* iseq, Val expr, Ctx* ctx) {
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
    int l0 = LABEL_NEW_NUM(ctx->labels);
    uint16_t ins;

    _encode_callback_expr(iseq, lhs, ctx);
    LABEL_REF(ctx->labels, Iseq.size(iseq) + 1);
    if (nb_string_byte_size(op) == 2 && nb_string_ptr(op)[0] == '&' && nb_string_ptr(op)[1] == '&') {
      ins = JUNLESS;
    } else {
      ins = JIF;
    }
    ENCODE(iseq, Arg32, ((Arg32){ins, l0}));
    _encode_callback_expr(iseq, rhs, ctx);
    LABEL_DEF(ctx->labels, l0, Iseq.size(iseq));

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
      _encode_callback_expr(iseq, e, ctx);
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
    if (i > ctx->terms_size) {
      // raise error
      // TODO maybe we don't need to pass terms_size everywhere
      // just check it after the bytecode is compiled
    }
    ENCODE(iseq, uint16_t, CAPTURE);
    ENCODE(iseq, uint16_t, (uint16_t)i);

  } else if (klass == kCreateNode) {
    // CreateNode[ty, (Expr | SplatEntry)*]

    // node[a, *b]:
    //   node_beg klass_name # postprocess: replace name_lit with klasses, in LEX compile
    //   a
    //   node_set
    //   b
    //   node_setv
    //   node_end

    Val klass_name = nb_struct_get(expr, 0);
    Val elems = nb_struct_get(expr, 1);
    uint32_t klass_str = VAL_TO_STR(klass_name);
    uint32_t klass_ref_offset = Iseq.size(iseq) + 1;
    uint32_t elems_size = 0;
    bool has_splat = false;
    ENCODE(iseq, ArgU32, ((ArgU32){NODE_BEG, klass_str}));
    elems = nb_cons_reverse(elems);
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(tail);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(iseq, to_splat, ctx);
        ENCODE(iseq, uint16_t, NODE_SETV);
        has_splat = true;
      } else {
        _encode_callback_expr(iseq, e, ctx);
        ENCODE(iseq, uint16_t, NODE_SET);
        elems_size++;
      }
    }
    ENCODE(iseq, uint16_t, NODE_END);
    KLASS_REF(ctx->klass_refs, klass_ref_offset, klass_str, elems_size, has_splat);

  } else if (klass == kCreateList) {
    // CreateList[(Expr | SplatEntry)*]

    // [a, *b]: a, b, list
    // [*a, *b]: a, b, listv
    // [a]: a, nil, list
    // NOTE: no need to reverse list here
    // NOTE: expressions should be evaluated from left to right,
    //       but the list is built from right to left
    Val elems = nb_struct_get(expr, 0);
    int size = 0;
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      size++;
    }
    char a[size]; // stack to reverse list/listv operations
    int i = 0;
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(elems);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(iseq, to_splat, ctx);
        a[i++] = 1;
      } else {
        _encode_callback_expr(iseq, e, ctx);
        a[i++] = 0;
      }
    }

    ENCODE(iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
    for (i = size - 1; i >= 0; i--) {
      if (a[i]) {
        ENCODE(iseq, uint16_t, LISTV);
      } else {
        ENCODE(iseq, uint16_t, LIST);
      }
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
    int l0 = LABEL_NEW_NUM(ctx->labels);
    int l1 = LABEL_NEW_NUM(ctx->labels);

    _encode_callback_expr(iseq, cond, ctx);
    LABEL_REF(ctx->labels, Iseq.size(iseq) + 1);
    ENCODE(iseq, Arg32, ((Arg32){JUNLESS, l0}));
    _encode_callback_lines(iseq, true_clause, ctx);
    LABEL_REF(ctx->labels, Iseq.size(iseq) + 1);
    ENCODE(iseq, Arg32, ((Arg32){JMP, l1}));
    LABEL_DEF(ctx->labels, l0, Iseq.size(iseq));
    if (VAL_KLASS(false_clause) == kIf) {
      _encode_callback_expr(iseq, false_clause, ctx);
    } else {
      _encode_callback_lines(iseq, false_clause, ctx);
    }
    LABEL_DEF(ctx->labels, l1, Iseq.size(iseq));
  }
}

static void _encode_callback_lines(struct Iseq* iseq, Val stmts, Ctx* ctx) {
  // Expr* (NOTE: no VarDecl in PEG callback)

  // NOTE: should only push the last expr to stack so this code can be correct: `[a, (b, c)]`
  // TODO: to support debugging we need to allocate more slots in stack to hold results of each line
  stmts = nb_cons_reverse(stmts);
  for (Val tail = stmts; tail; tail = nb_cons_tail(tail)) {
    Val e = nb_cons_head(tail);
    _encode_callback_expr(iseq, e, ctx);
    if (nb_cons_tail(tail) != VAL_NIL) {
      ENCODE(iseq, uint16_t, POP);
    }
  }
}

Val sb_vm_callback_compile(struct Iseq* iseq, Val stmts, Ctx ctx) {
  if (!kInfixLogic) {
    uint32_t sb = sb_klass();
    kInfixLogic = klass_find_c("kInfixLogic", sb); assert(kInfixLogic);
    kCall       = klass_find_c("kCall", sb);
    kCapture    = klass_find_c("kCapture", sb);
    kCreateNode = klass_find_c("kCreateNode", sb);
    kCreateList = klass_find_c("kCreateList", sb);
    kSplatEntry = klass_find_c("kSplatEntry", sb);
    kIf         = klass_find_c("kIf", sb);
  }

  _encode_callback_lines(iseq, stmts, &ctx);
  return VAL_NIL;
}

void sb_vm_callback_decompile(uint16_t* pc_start) {
  uint16_t* pc = pc_start;
  uint32_t size = DECODE(ArgU32, pc).arg1;
  uint16_t* pc_end = pc_start + size;
  DECODE(void*, pc);

  while (pc < pc_end) {
    printf("%ld: %s", pc - pc_start, op_code_names[*pc]);
    switch (*pc) {
      case POP:
      case NODE_SET:
      case NODE_SETV:
      case NODE_END:
      case LIST:
      case LISTV: {
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

      case LOAD:
      case STORE:
      case NODE_BEG:
      case JIF:
      case JUNLESS:
      case JMP: {
        printf(" %u\n", DECODE(ArgU32, pc).arg1);
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

      case CALL: {
        ArgU32U32 args = DECODE(ArgU32U32, pc);
        printf(" %u %u\n", args.arg1, args.arg2);
        break;
      }

      default: {
        fatal_err("bad pc: %d", (int)*pc);
      }
    }
  }
}
