#include "compile.h"
#include <stdlib.h>

enum OpCodes {
  PUSH_CAPTURE=0, // n:(0..9)
  PUSH,           // val:(in next opcode)
  PUSH_VAR,       // name:(val_str)
  POP_VAR,        // name:(val_str)
  CALL,           // argsize:(uint32)     # pop stack for func name
  CREATE_NODE,    // argsize:(uint32)     # pop stack for class name
  JIF,            // offset:(int32)       # pop stack for value test
  MATCH,          // reg:(in next opcode) # match reg, and push match result on to stack
  END,            //                      # end vm
  OP_CODES_SIZE   //
};

typedef struct {
  uint32_t op;
  uint32_t data;
} Inst;

typedef union {
  Inst inst;
  Val v;
} Cast;

struct VmCallbackStruct {
  Inst* insts;
  uint32_t max_stack_size;
  uint32_t size;
  uint32_t cap;
};

#define OP(pc) (pc)->op
#define DATA(pc) (pc)->data

static void _traverse(Val node, VmCallback* cb, Spellbreak* sb);

static void _ensure_size(VmCallback* cb, int inc) {
  if (cb->size + inc > cb->cap) {
    cb->insts = realloc(cb->insts, cb->cap *= 2);
  }
}

static void _append(VmCallback* cb, uint32_t op, uint32_t data) {
  _ensure_size(cb, 1);
  cb->insts[cb->size].op = op;
  cb->insts[cb->size].data = data;
  cb->size++;
}

static void _append_val(VmCallback* cb, Val v) {
  _ensure_size(cb, 2);
  cb->insts[cb->size].op = PUSH;
  cb->insts[cb->size].data = 0;
  cb->size++;
  Cast cast = {.v = v};
  cb->insts[cb->size] = cast.inst;
  cb->size++;
}

static int64_t _to_i(Val tok) {
  // todo
}

// NOTE the list is right-to-left
static int32_t _push_list(Val list, VmCallback* cb, Spellbreak* sb) {
  int32_t size = 0;
  list = nb_cons_node_reverse(sb->arena, list);
  for (; list != VAL_NIL; list = TAIL(list)) {
    Val node = HEAD(list);
    if (node != VAL_UNDEF) {
      _traverse(ast, cb, sb);
      size++;
    }
  }
  return size;
}

static void _traverse(Val node, VmCallback* cb, Spellbreak* sb) {
  if (VAL_IS_IMM(node)) {
    if (node == VAL_UNDEF) {
      // ignore
    } else {
      _append(cb, PUSH, node);
    }

  } else if (IS_A(node, "InfixLogic")) { // shortcuts
    // todo

  } else if (IS_A(node, "Call")) {
    int arity = _push_list(AT(node, 1), cb, sb);
    TokenNode* fname_tok = (TokenNode*)AT(node, 0);
    void* func = nb_spellbreak_find_action(sb, fname_tok->loc.s, fname_tok->loc.size, arity);
    _append(cb, PUSH, 0);
    _append_val(cb, (Val)func);
    _append(cb, CALL, arity);

  } else if (IS_A(node, "Capture")) {
    _append(cb, PUSH_CAPTURE, _to_i(AT(node, 0)));

  } else if (IS_A(node, "CreateNode")) {
    int attr_size = _push_list(AT(node, 1), cb, sb);
    _append(cb, PUSH, 0);
    _append_val(cb, nb_token_node_to_s_literal(AT(node, 0)));
    _append(cb, CREATE_NODE);

  } else if (IS_A(node, "CreateList")) {
    // todo translate to a series of cons calls
    
  } else if (IS_A(node, "VarRef")) {
    // todo

  } else if (IS_A(node, "Assign")) {
    // todo

  } else if (IS_A(node, "If")) {
    // todo

  } else {
    _append(cb, PUSH, node);
  }
}

VmCallback* nb_vm_callback_compile(void* arena, Val node, Spellbreak* sb, Val lex_name) {
  VmCallback* code = malloc(sizeof(VmCallback));
  code->cap = 10;
  code->insts = malloc(sizeof(Inst) * 10);
  code->size = 0;
  Val cons_node = nb_cons_node_reverse(arena, AT(node, 0));

  bool is_begin = true;
  for (; cons_node != VAL_NIL; cons_node = TAIL(cons_node)) {
    Val stmt = HEAD(cons_node);
    if (stmt == VAL_UNDEF) {
    } else if (IS_A(stmt, "VarDecl")) {
      if (lex_name == VAL_NIL) {
        COMPILE_ERROR("var statement only allowed in lex");
      } else if (is_begin) {
        int lex_name_size = nb_string_byte_size(lex_name);
        char* lex_name_s = nb_string_ptr(lex_name);
        TokenNode* name_tok = (TokenNode*)AT(stmt, 0);
        int name_size = name_tok->loc.size + lex_name_size + 1;
        char name[name_size + 1];
        sprintf(name, "%.*s:%.*s", lex_name_size, lex_name_s, name_size, name_tok->loc.s);
        REPLACE(sb->vars_dict, nb_dict_insert(sb->vars_dict, name, name_size, VAL_TRUE));
      } else {
        // todo allow more flexible var decl?
        COMPILE_ERROR("declaring var in non-begin block");
      }
    } else { // expr
      is_begin = false;
      _traverse(node, code);
    }
  }

  return code;
}

#define GET_VAR(ctx, i) ctx->vars[i]
#define SET_VAR(ctx, i, v) ctx->vars[i] = v

int64_t nb_vm_callback_exec(VmCallback* code, Ctx* ctx) {
  static const void* labels[] = {
    [PUSH_CAPTURE] = &&label_PUSH_CAPTURE,
    [PUSH] = &&label_PUSH,
    [PUSH_VAR] = &&label_PUSH_VAR,
    [POP_VAR] = &&label_POP_VAR,
    [CALL] = &&label_CALL,
    [CREATE_NODE] = &&label_CREATE_NODE,
    [JIF] = &&label_JIF,
    [END] = &&label_END
  };
  Val stack[code->max_stack_size];
  size_t sp = 0;
  Inst* pc = code->insts;

# define INIT_DISPATCH goto *labels[OP(pc)]
# define DISPATCH goto *labels[OP(++pc)]
# define CASE(l) label_##l: case l

  INIT_DISPATCH;
  switch (OP(pc)) {
    CASE(PUSH_CAPTURE): {
      uint32_t n = DATA(pc);
      stack[sp++] = ctx->captures[n];
      DISPATCH;
    }
    CASE(PUSH): {
      pc++;
      stack[sp++] = *((Val*)pc);
      DISPATCH;
    }
    CASE(PUSH_VAR): {
      uint32_t var_id = DATA(pc);
      stack[sp++] = GET_VAR(ctx, var_id);
      DISPATCH;
    }
    CASE(POP_VAR): {
      uint32_t var_id = DATA(pc);
      Val v = stack[--sp];
      SET_VAR(ctx, var_id, v);
      DISPATCH;
    }
    CASE(CALL): {
      void* func = (void*)stack[--sp];
      uint32_t argc = DATA(pc);
      sp -= argc;
      Val res = VAL_C_CALL(func, argc, stack + sp);
      stack[sp++] = res;
      DISPATCH;
    }
    CASE(CREATE_NODE): {
      Val node_name = (Val)stack[--sp];
      Val context_name;
      // TODO
      DISPATCH;
    }
    CASE(JIF): {
      int32_t offset = (int32_t)DATA(pc);
      pc += offset;
      goto *labels[OP(pc)];
    }
    CASE(END): {
      // todo do some check?
      return 1;
    }
  }

# undef CASE
# undef INIT_DISPATCH
# undef DISPATCH

}
