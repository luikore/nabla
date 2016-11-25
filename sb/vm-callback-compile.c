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
  struct Iseq* iseq;
  int terms_size;
  bool peg_mode;
  struct Labels* labels;
  struct StructsTable* structs_table;
} CallbackCompileCtx;

static void _encode_callback_lines(CallbackCompileCtx* ctx, Val stmts);

#pragma mark ## impls

static int _iseq_size(CallbackCompileCtx* ctx) {
  return Iseq.size(ctx->iseq);
}

// returns change of stack
// terms_size is for checking of capture overflows
static void _encode_callback_expr(CallbackCompileCtx* ctx, Val expr) {
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

    _encode_callback_expr(ctx, lhs);
    LABEL_REF(ctx->labels, _iseq_size(ctx) + 1);
    if (nb_string_byte_size(op) == 2 && nb_string_ptr(op)[0] == '&' && nb_string_ptr(op)[1] == '&') {
      ins = JUNLESS;
    } else {
      ins = JIF;
    }
    ENCODE(ctx->iseq, Arg32, ((Arg32){ins, l0}));
    _encode_callback_expr(ctx, rhs);
    LABEL_DEF(ctx->labels, l0, _iseq_size(ctx));

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
      _encode_callback_expr(ctx, e);
      argc += 1;
    }
    ENCODE(ctx->iseq, ArgU32U32, ((ArgU32U32){CALL, argc, func_name}));

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
    ENCODE(ctx->iseq, uint16_t, CAPTURE);
    ENCODE(ctx->iseq, uint16_t, (uint16_t)i);

  } else if (klass == kCreateNode) {
    // CreateNode[ty, (Expr | SplatEntry)*]

    // node[a, *b]:
    //   node_beg klass_id
    //   a
    //   node_set
    //   b
    //   node_setv
    //   node_end

    // search for klass
    Val klass_name = AT(expr, 0);
    Val elems = AT(expr, 1);

    StructsTableValue structs_table_value;
    bool found = StructsTable.find(ctx->structs_table, klass_name, &structs_table_value);
    if (!found) {
      // TODO resumable and report syntax error at position
      fatal_err("struct not found: %.*s", (int)nb_string_byte_size(klass_name), nb_string_ptr(klass_name));
    }

    // validate arity
    int elems_size = 0;
    bool has_more_elems = false;
    for (Val elems_list = elems; elems_list != VAL_NIL; elems_list = TAIL(elems_list)) {
      if (VAL_KLASS(HEAD(elems_list)) == kSplatEntry) {
        has_more_elems = true;
      } else {
        elems_size++;
      }
    }
    if (has_more_elems && elems_size > structs_table_value.max_elems) {
      fatal_err("struct %.*s requies no more than %d members", (int)nb_string_byte_size(klass_name), nb_string_ptr(klass_name), structs_table_value.max_elems);
    }
    if (!has_more_elems && elems_size < structs_table_value.min_elems) {
      fatal_err("struct %.*s requies at least %d members", (int)nb_string_byte_size(klass_name), nb_string_ptr(klass_name), structs_table_value.min_elems);
    }

    // encode
    uint32_t klass_id = structs_table_value.klass_id;
    ENCODE(ctx->iseq, ArgU32, ((ArgU32){NODE_BEG, klass_id}));
    elems = nb_cons_reverse(elems);
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(tail);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(ctx, to_splat);
        ENCODE(ctx->iseq, uint16_t, NODE_SETV);
      } else {
        _encode_callback_expr(ctx, e);
        ENCODE(ctx->iseq, uint16_t, NODE_SET);
      }
    }
    ENCODE(ctx->iseq, uint16_t, NODE_END);

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
        _encode_callback_expr(ctx, to_splat);
        a[i++] = 1;
      } else {
        _encode_callback_expr(ctx, e);
        a[i++] = 0;
      }
    }

    ENCODE(ctx->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
    for (i = size - 1; i >= 0; i--) {
      if (a[i]) {
        ENCODE(ctx->iseq, uint16_t, LISTV);
      } else {
        ENCODE(ctx->iseq, uint16_t, LIST);
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

    _encode_callback_expr(ctx, cond);
    LABEL_REF(ctx->labels, _iseq_size(ctx) + 1);
    ENCODE(ctx->iseq, Arg32, ((Arg32){JUNLESS, l0}));
    _encode_callback_lines(ctx, true_clause);
    LABEL_REF(ctx->labels, _iseq_size(ctx) + 1);
    ENCODE(ctx->iseq, Arg32, ((Arg32){JMP, l1}));
    LABEL_DEF(ctx->labels, l0, _iseq_size(ctx));
    if (VAL_KLASS(false_clause) == kIf) {
      _encode_callback_expr(ctx, false_clause);
    } else {
      _encode_callback_lines(ctx, false_clause);
    }
    LABEL_DEF(ctx->labels, l1, _iseq_size(ctx));
  }
}

static void _encode_callback_lines(CallbackCompileCtx* ctx, Val stmts) {
  // Expr* (NOTE: no VarDecl in PEG callback)

  // NOTE: should only push the last expr to stack so this code can be correct: `[a, (b, c)]`
  // TODO: to support debugging we need to allocate more slots in stack to hold results of each line
  stmts = nb_cons_reverse(stmts);
  for (Val tail = stmts; tail; tail = nb_cons_tail(tail)) {
    Val e = nb_cons_head(tail);
    _encode_callback_expr(ctx, e);
    if (nb_cons_tail(tail) != VAL_NIL) {
      ENCODE(ctx->iseq, uint16_t, POP);
    }
  }
}

Val sb_vm_callback_compile(struct Iseq* iseq, Val stmts, int32_t terms_size, void* labels, bool peg_mode, void* structs_table) {
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

  CallbackCompileCtx ctx;
  ctx.iseq = iseq;
  ctx.terms_size = terms_size;
  ctx.labels = labels;
  ctx.peg_mode = peg_mode;
  ctx.structs_table = structs_table;
  _encode_callback_lines(&ctx, stmts);
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
