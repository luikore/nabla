#include "compile.h"
#include "vm-callback-op-codes.h"

#pragma mark ## decls

// the top 10 slots are for captures
#define LOCAL_VAR_OFFSET 10

static uint32_t kInfixLogic = 0;
static uint32_t kCall = 0;
static uint32_t kCapture = 0;
static uint32_t kCreateNode = 0;
static uint32_t kCreateList = 0;
static uint32_t kSplatEntry = 0;
static uint32_t kIf = 0;
static uint32_t kAssign = 0;
static uint32_t kGlobalAssign = 0;
static uint32_t kVarRef = 0;
static uint32_t kGlobalVarRef = 0;
static uint32_t kVarDecl = 0;

typedef struct {
  struct Iseq* iseq;
  int terms_size;
  uint16_t capture_mask;
  struct Labels* labels;
  struct StructsTable* structs_table;  // for Peg
  struct VarsTable* global_vars;       // for Lex
  struct VarsTable* local_vars;        // for Lex (TODO maybe also for Peg?)
} CallbackCompiler;

static void _encode_callback_lines(CallbackCompiler* compiler, Val stmts);

#pragma mark ## impls

static int _iseq_size(CallbackCompiler* compiler) {
  return Iseq.size(compiler->iseq);
}

// returns change of stack
// terms_size is for checking of capture overflows
static void _encode_callback_expr(CallbackCompiler* compiler, Val expr) {
  uint32_t klass = VAL_KLASS(expr);
  // Expr = InfixLogic | Call | Capture | CraeteNode | CreateList | Assign | If | Nul
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
    int l0 = LABEL_NEW_NUM(compiler->labels);
    uint16_t ins;

    _encode_callback_expr(compiler, lhs);
    LABEL_REF(compiler->labels, _iseq_size(compiler) + 1);
    if (nb_string_byte_size(op) == 2 && nb_string_ptr(op)[0] == '&' && nb_string_ptr(op)[1] == '&') {
      ins = JUNLESS;
    } else {
      ins = JIF;
    }
    ENCODE(compiler->iseq, Arg32, ((Arg32){ins, l0}));
    _encode_callback_expr(compiler, rhs);
    LABEL_DEF(compiler->labels, l0, _iseq_size(compiler));

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
      _encode_callback_expr(compiler, e);
      argc += 1;
    }
    ENCODE(compiler->iseq, ArgU32U32, ((ArgU32U32){CALL, argc, func_name}));

  } else if (klass == kCapture) {
    // Capture[var_name]

    // TODO $-\d+
    Val tok = nb_struct_get(expr, 0);
    int size = nb_string_byte_size(tok);
    // TODO handle the cases of more than 2 digits
    char s[size];
    strncpy(s, nb_string_ptr(tok) + 1, size - 1);
    s[size - 1] = '\0';
    int i = atoi(s);
    if (i > compiler->terms_size) {
      // raise error
      // TODO maybe we don't need to pass terms_size everywhere
      // just check it after the bytecode is compiled
    }

    // capture allocates resources, the mask for the save.
    compiler->capture_mask |= (1 << i);
    ENCODE(compiler->iseq, uint16_t, LOAD);
    ENCODE(compiler->iseq, uint32_t, (uint32_t)i);

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
    bool found = StructsTable.find(compiler->structs_table, klass_name, &structs_table_value);
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
    ENCODE(compiler->iseq, ArgU32, ((ArgU32){NODE_BEG, klass_id}));
    elems = nb_cons_reverse(elems);
    for (Val tail = elems; tail; tail = nb_cons_tail(tail)) {
      Val e = nb_cons_head(tail);
      if (VAL_KLASS(e) == kSplatEntry) {
        Val to_splat = nb_struct_get(e, 0);
        _encode_callback_expr(compiler, to_splat);
        ENCODE(compiler->iseq, uint16_t, NODE_SETV);
      } else {
        _encode_callback_expr(compiler, e);
        ENCODE(compiler->iseq, uint16_t, NODE_SET);
      }
    }
    ENCODE(compiler->iseq, uint16_t, NODE_END);

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
        _encode_callback_expr(compiler, to_splat);
        a[i++] = 1;
      } else {
        _encode_callback_expr(compiler, e);
        a[i++] = 0;
      }
    }

    ENCODE(compiler->iseq, ArgVal, ((ArgVal){PUSH, VAL_NIL}));
    for (i = size - 1; i >= 0; i--) {
      if (a[i]) {
        ENCODE(compiler->iseq, uint16_t, LISTV);
      } else {
        ENCODE(compiler->iseq, uint16_t, LIST);
      }
    }

  } else if (klass == kAssign) {
    // Assign[var_name, expr]
    Val var_name = nb_struct_get(expr, 0);
    Val value = nb_struct_get(expr, 1);

    int var_id = SYMBOLS_LOOKUP_VAR_ID(compiler->local_vars, var_name);
    if (var_id < 0) {
      fatal_err("assigning local var %.*s not declared", (int)nb_string_byte_size(var_name), nb_string_ptr(var_name));
    }
    _encode_callback_expr(compiler, value);
    ENCODE(compiler->iseq, uint16_t, STORE);
    ENCODE(compiler->iseq, uint32_t, var_id + LOCAL_VAR_OFFSET);

  } else if (klass == kGlobalAssign) {
    // GlobalAssign[var_name, expr]
    Val var_name = nb_struct_get(expr, 0);
    Val value = nb_struct_get(expr, 1);

    int var_id = SYMBOLS_LOOKUP_VAR_ID(compiler->global_vars, var_name);
    if (var_id < 0) {
      fatal_err("assigning global var %.*s not declared", (int)nb_string_byte_size(var_name), nb_string_ptr(var_name));
    }
    _encode_callback_expr(compiler, value);
    ENCODE(compiler->iseq, uint16_t, STORE_GLOB);
    ENCODE(compiler->iseq, uint32_t, var_id);

  } else if (klass == kVarRef) {
    // VarRef[var_name]
    Val var_name = nb_struct_get(expr, 0);
    
    int var_id = SYMBOLS_LOOKUP_VAR_ID(compiler->local_vars, var_name);
    if (var_id < 0) {
      fatal_err("referencing local var %.*s not declared", (int)nb_string_byte_size(var_name), nb_string_ptr(var_name));
    }
    ENCODE(compiler->iseq, uint16_t, LOAD);
    ENCODE(compiler->iseq, uint32_t, var_id + LOCAL_VAR_OFFSET);

  } else if (klass == kGlobalVarRef) {
    // GlobalVarRef[var_name]
    Val var_name = nb_struct_get(expr, 0);

    int var_id = SYMBOLS_LOOKUP_VAR_ID(compiler->global_vars, var_name);
    if (var_id < 0) {
      fatal_err("referencing global var %.*s not declared", (int)nb_string_byte_size(var_name), nb_string_ptr(var_name));
    }
    ENCODE(compiler->iseq, uint16_t, LOAD_GLOB);
    ENCODE(compiler->iseq, uint32_t, var_id);

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
    int l0 = LABEL_NEW_NUM(compiler->labels);
    int l1 = LABEL_NEW_NUM(compiler->labels);

    _encode_callback_expr(compiler, cond);
    LABEL_REF(compiler->labels, _iseq_size(compiler) + 1);
    ENCODE(compiler->iseq, Arg32, ((Arg32){JUNLESS, l0}));
    _encode_callback_lines(compiler, true_clause);
    LABEL_REF(compiler->labels, _iseq_size(compiler) + 1);
    ENCODE(compiler->iseq, Arg32, ((Arg32){JMP, l1}));
    LABEL_DEF(compiler->labels, l0, _iseq_size(compiler));
    if (VAL_KLASS(false_clause) == kIf) {
      _encode_callback_expr(compiler, false_clause);
    } else {
      _encode_callback_lines(compiler, false_clause);
    }
    LABEL_DEF(compiler->labels, l1, _iseq_size(compiler));
  }
}

static void _encode_callback_lines(CallbackCompiler* compiler, Val stmts) {
  // NOTE: should only push the last expr to stack so this code can be correct: `[a, (b, c)]`
  // TODO: to support debugging we need to allocate more slots in stack to hold results of each line
  stmts = nb_cons_reverse(stmts);
  for (Val tail = stmts; tail; tail = nb_cons_tail(tail)) {
    Val e = nb_cons_head(tail);
    if (VAL_KLASS(e) == kVarDecl) {
      // we already have vars declared in compile-build-symbols
    } else {
      _encode_callback_expr(compiler, e);
    }
    if (nb_cons_tail(tail) != VAL_NIL) {
      ENCODE(compiler->iseq, uint16_t, POP);
      // TODO later we can optimize redundant pops with peepholes
    }
  }
}

// when given global_vars, it is in lex mode
Val sb_vm_callback_compile(struct Iseq* iseq, Val stmts, int32_t terms_size, void* labels,
                           void* structs_table, struct VarsTable* global_vars, struct VarsTable* local_vars, uint16_t* capture_mask) {
  if (!kInfixLogic) {
    uint32_t sb   = sb_klass();
    kInfixLogic   = klass_find_c("kInfixLogic", sb); assert(kInfixLogic);
    kCall         = klass_find_c("kCall", sb);
    kCapture      = klass_find_c("kCapture", sb);
    kCreateNode   = klass_find_c("kCreateNode", sb);
    kCreateList   = klass_find_c("kCreateList", sb);
    kSplatEntry   = klass_find_c("kSplatEntry", sb);
    kIf           = klass_find_c("kIf", sb);
    kAssign       = klass_find_c("kAssign", sb);
    kGlobalAssign = klass_find_c("kGlobalAssign", sb);
    kVarRef       = klass_find_c("kVarRef", sb);
    kGlobalVarRef = klass_find_c("kGlobalVarRef", sb);
    kVarDecl      = klass_find_c("kVarDecl", sb);
  }

  CallbackCompiler compiler;
  compiler.iseq = iseq;
  compiler.terms_size = terms_size;
  compiler.labels = labels;
  compiler.structs_table = structs_table;
  compiler.global_vars = global_vars;
  compiler.local_vars = local_vars;
  compiler.capture_mask = 0;

  _encode_callback_lines(&compiler, stmts);
  *capture_mask = compiler.capture_mask;
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
      case LOAD_GLOB:
      case STORE_GLOB:
      case NODE_BEG:
      case JIF:
      case JUNLESS:
      case JMP: {
        printf(" %u\n", DECODE(ArgU32, pc).arg1);
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
