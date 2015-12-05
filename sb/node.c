#include "node.h"
#include <stdarg.h>
#include <adt/val.h>
#include <adt/string.h>
#include <adt/box.h>
#include <adt/utils/dbg.h>
#include <adt/utils/arena.h>
#include <adt/utils/mut-map.h>

// TODO See also this for line/col management of tokens
// http://www.linuxdevcenter.com/pub/a/linux/excerpts/9780596155971/error-reporting-recovery.html

// TODO Reduce allocation by marking the arena save points

#pragma mark ## data structures

typedef struct {
  Val context;
  Val type;
} NodeClassSpecifier;

struct NodeClassStruct {
  NodeClassSpecifier spec;
  int32_t node_qword_size;
  int32_t attr_size; // only for syntax node
  char type_indicator;
};

static uint64_t hash_func(NodeClassSpecifier spec) {
  return val_hash(spec.context) ^ val_hash(spec.type);
}

static bool eq_func(NodeClassSpecifier v1, NodeClassSpecifier v2) {
  return v1.context == v2.context && v1.type == v2.type;
}

// NOTE we can't set value type to the non-pointer `NodeClass` instead, because:
//      node references the pointer, but in map rehash it changes slot address
// TODO arena allocate node (lower priority than optimizing mut-map)
MUT_MAP_DECL(PdlexNodeMetaStruct, NodeClassSpecifier, NodeClass*, hash_func, eq_func);

struct PdlexNodeArenaStruct {
  PdlexNodeMeta* node_meta;
  // TODO inline arena
  Arena* arena;
};

#pragma mark ## static classes and kinds of nodes

static NodeClass eof_class = {0};
static NodeClass cons_class = {0};
static NodeClass wrapper_class = {0};
static bool initialized = false;

#define GET_KLASS(node) ((node) == VAL_NIL ? &cons_class : ((NodeHeader*)(node))->klass)
#define QWORDS_CLASS ((sizeof(NodeClass) + 7) / 8)
#define QWORDS_WRAPPER ((sizeof(WrapperNode) + 7) / 8)
#define QWORDS_CONS ((sizeof(ConsNode) + 7) / 8)
#define QWORDS_TOKEN ((sizeof(TokenNode) + 7) / 8)

#pragma mark ## meta

PdlexNodeMeta* nb_node_meta_new() {
  if (!initialized) {
    initialized = true;
    eof_class.spec.context = VAL_NIL;
    eof_class.spec.type = nb_string_new_literal_c(".");
    eof_class.node_qword_size = QWORDS_TOKEN;
    eof_class.type_indicator = '.';

    cons_class.spec.context = VAL_NIL;
    cons_class.spec.type = nb_string_new_literal_c("*");
    cons_class.node_qword_size = QWORDS_CONS;
    cons_class.type_indicator = '*';

    wrapper_class.spec.context = VAL_NIL;
    wrapper_class.spec.type = nb_string_new_literal_c("@");
    wrapper_class.node_qword_size = QWORDS_WRAPPER;
    wrapper_class.type_indicator = '@';
  }

  PdlexNodeMeta* node_meta = malloc(sizeof(PdlexNodeMeta));
  PdlexNodeMetaStruct.init(node_meta);
  return node_meta;
}

void nb_node_meta_delete(PdlexNodeMeta* node_meta) {
  PdlexNodeMetaStructIter it;
  for (PdlexNodeMetaStruct.iter_init(&it, node_meta); !PdlexNodeMetaStruct.iter_is_end(&it); PdlexNodeMetaStruct.iter_next(&it)) {
    free(it.slot->v);
  }
  PdlexNodeMetaStruct.cleanup(node_meta);
  free(node_meta);
}

Val nb_node_meta_def_class(PdlexNodeMeta* node_meta, const char* context_name, const char* type_name, int32_t attr_size) {
  assert(strlen(type_name));
  assert(type_name[0] == '<');

  Val context = nb_string_new_literal_c(context_name);
  Val type = nb_string_new_literal_c(type_name);
  NodeClassSpecifier spec = {.context = context, .type = type};

  NodeClass* c;
  if (PdlexNodeMetaStruct.find(node_meta, spec, &c)) {
    assert(c->attr_size == attr_size);
    return type;
  }

  int32_t node_qword_size = attr_size + 1;
  c = malloc(sizeof(NodeClass));
  c->spec.context = context;
  c->spec.type = type;
  c->node_qword_size = node_qword_size;
  c->attr_size = attr_size;
  c->type_indicator = type_name[0];
  PdlexNodeMetaStruct.insert(node_meta, spec, c);
  return type;
}

Val nb_node_meta_def_token(PdlexNodeMeta* node_meta, const char* type_name) {
  assert(strlen(type_name));
  assert(type_name[0] == '.');

  Val type = nb_string_new_literal_c(type_name);
  NodeClassSpecifier spec = {.context = VAL_NIL, .type = type};

  if (PdlexNodeMetaStruct.find(node_meta, spec, NULL)) {
    return type;
  }

  NodeClass* c = malloc(sizeof(NodeClass));
  c->spec.context = VAL_NIL;
  c->spec.type = type;
  c->node_qword_size = QWORDS_TOKEN;
  c->attr_size = 0;
  c->type_indicator = type_name[0];
  PdlexNodeMetaStruct.insert(node_meta, spec, c);
  return type;
}

#pragma mark ## arena

PdlexNodeArena* nb_node_arena_new(PdlexNodeMeta* meta) {
  PdlexNodeArena* arena = malloc(sizeof(PdlexNodeArena));
  arena->node_meta = meta;
  arena->arena = arena_new();
  return arena;
}

void nb_node_arena_push(PdlexNodeArena* arena) {
  arena_push(arena->arena);
}

void nb_node_arena_pop(PdlexNodeArena* arena) {
  arena_pop(arena->arena);
}

void nb_node_arena_delete(PdlexNodeArena* arena) {
  arena_delete(arena->arena);
  free(arena);
}

#pragma mark ## node

Val nb_node_context_name(Val node) {
  return GET_KLASS(node)->spec.context;
}

Val nb_node_type_name(Val node) {
  return GET_KLASS(node)->spec.type;
}

Val nb_node_dup(PdlexNodeArena* arena, Val node) {
  if (GET_KLASS(node) == &cons_class) {
    // TODO
    log_err("not implemented");
    return VAL_NIL;
  } else {
    void* node_ptr = (void*)node;
    void* new_node = arena_slot_alloc(arena->arena, GET_KLASS(node)->node_qword_size);
    memcpy(new_node, node_ptr, GET_KLASS(node)->node_qword_size * 8);
    return (Val)new_node;
  }
}

void nb_node_debug(Val node) {
  // todo
}

bool nb_node_is_wrapper(Val node) {
  return GET_KLASS(node) == &wrapper_class;
}

#pragma mark ## conversion

Val nb_node_to_val(Val node) {
  if (VAL_IS_IMM(node)) {
    return node;
  } else if (nb_node_is_wrapper(node)) {
    return ((WrapperNode*)node)->val;
  } else {
    return nb_box_new(node);
  }
}

Val nb_val_to_node(PdlexNodeArena* arena, Val val) {
  if (VAL_IS_IMM(val)) {
    return val;
  } else if (nb_val_is_box(val)) {
    return nb_box_get(val);
  } else {
    return nb_wrapper_node_new(arena, val);
  }
}

#pragma mark ## token node, locatable

Val nb_token_node_new(PdlexNodeArena* arena, const char* node_name) {
  char type_name[strlen(node_name) + 2];
  type_name[0] = '.';
  strcpy(type_name + 1, node_name);

  TokenNode* node = arena_slot_alloc(arena->arena, QWORDS_TOKEN);
  if (strlen(type_name) == 1) {
    node->klass = &eof_class;
    return (Val)node;
  }

  Val type = nb_string_new_literal_c(type_name);
  NodeClassSpecifier spec = {.context = VAL_NIL, .type = type};
  NodeClass* c;
  bool found = PdlexNodeMetaStruct.find(arena->node_meta, spec, &c);
  assert(found);
  node->klass = c;
  return (Val)node;
}

Val nb_token_node_new_c(PdlexNodeArena* arena, const char* type_name, const char* s) {
  TokenNode* tok = (TokenNode*)nb_token_node_new(arena, type_name);
  tok->loc.s = s;
  tok->loc.size = strlen(s);
  return (Val)tok;
}

Val nb_token_node_to_s(Val node) {
  TokenNode* n = (TokenNode*)node;
  return nb_string_new(n->loc.size, n->loc.s);
}

Val nb_token_node_to_s_literal(Val node) {
  TokenNode* n = (TokenNode*)node;
  return nb_string_new_literal(n->loc.size, n->loc.s);
}

#pragma mark ## cons node

Val nb_cons_node_new(PdlexNodeArena* arena, Val e, Val list) {
  assert(GET_KLASS(list) == &cons_class);

  ConsNode* node = arena_slot_alloc(arena->arena, QWORDS_CONS);
  node->klass = &cons_class;
  node->e = e;
  node->list = list;
  return (Val)node;
}

Val nb_cons_node_reverse(PdlexNodeArena* arena, Val list) {
  Val res = VAL_NIL;
  for (Val curr = list; curr != VAL_NIL; curr = ((ConsNode*)curr)->list) {
    res = nb_cons_node_new(arena, ((ConsNode*)curr)->e, res);
  }
  return res;
}

#pragma mark ## syntax node

Val nb_syntax_node_new(PdlexNodeArena* arena, const char* context_name, const char* node_name) {
  assert(strlen(node_name));
  char type_name[strlen(node_name) + 2];
  type_name[0] = '<';
  strcpy(type_name + 1, node_name);

  SyntaxNode* node;
  NodeClass* klass;
  NodeClassSpecifier spec = {
    .context = nb_string_new_literal_c(context_name),
    .type = nb_string_new_literal_c(type_name)
  };
  bool found = PdlexNodeMetaStruct.find(arena->node_meta, spec, &klass);
  assert(found);
  node = arena_slot_alloc(arena->arena, klass->node_qword_size);
  node->klass = klass;
  return (Val)node;
}

Val nb_syntax_node_new_v(PdlexNodeArena* arena, const char* context_name, const char* node_name, int32_t size, ...) {
  assert(strlen(node_name));
  char type_name[strlen(node_name) + 2];
  type_name[0] = '<';
  strcpy(type_name + 1, node_name);

  NodeClass* klass;
  NodeClassSpecifier spec = {
    .context = nb_string_new_literal_c(context_name),
    .type = nb_string_new_literal_c(type_name)
  };
  bool found = PdlexNodeMetaStruct.find(arena->node_meta, spec, &klass);
  if (!found) {
    log_err("not found: %s:%s", context_name, type_name);
    assert(false);
    _Exit(-1);
  }
  if (klass->attr_size != size) {
    log_err("attr size not match in %s:%s: registered %d, but calling %d", context_name, type_name, klass->attr_size, size);
    assert(false);
    _Exit(-1);
  }
  assert(klass->attr_size == size);

  SyntaxNode* node = (SyntaxNode*)nb_syntax_node_new(arena, context_name, node_name);
  va_list vl;
  va_start(vl, size);
  for (int i = 0; i < size; i++) {
    Val arg = va_arg(vl, Val);
    node->attrs[i] = arg;
  }
  va_end(vl);
  return (Val)node;
}

int32_t nb_syntax_node_size(Val node) {
  assert(GET_KLASS(node)->type_indicator == '<');
  return GET_KLASS(node)->attr_size;
}

bool nb_syntax_node_is(Val node, const char* type) {
  Val node_type = GET_KLASS(node)->spec.type;
  const char* from = nb_string_ptr(node_type) + 1;
  int64_t size = nb_string_byte_size(node_type) - 1;
  if (size != strlen(type)) {
    return false;
  }
  return strncmp(from, type, size) == 0;
}

#pragma mark ## res node

Val nb_wrapper_node_new(PdlexNodeArena* arena, Val val) {
  WrapperNode* node = arena_slot_alloc(arena->arena, QWORDS_WRAPPER);

  node->klass = &wrapper_class;
  node->val = val;
  return (Val)node;
}
