#include "compile.h"
#include <adt/utils/mut-map.h>
#include <adt/utils/arena.h>

struct DepNodeStruct;
typedef struct DepNodeStruct DepNode;
struct DepNodeStruct {
  Val ctx_name; // string
  DepNode* next;
};

MUT_MAP_DECL(ContextMap, Val, Val, val_hash, val_eq);

// true: removed, false: not found
static bool _pop_zero_deg_node(DepNode** list, int size, Val* res) {
  for (int i = size - 1; i >= 0; i--) {
    if (list[i]->next == NULL) {
      *res = list[i]->ctx_name;
      if (i + 1 < size) {
        memmove(list + i, list + i + 1, sizeof(DepNode*) * (size - 1 - i));
      }
      return true;
    }
  }
  return false;
}

// expand partial ref (it is ensured to be leaf by toposort), and eliminate nil rules
static void _expand_lex_def(struct ContextMap* context_map, Val name) {
  Val curr;
  bool found = ContextMap.find(context_map, name, &curr);
  assert(found);
  Val res = VAL_NIL;

  for (; curr != VAL_NIL; curr = TAIL(curr)) {
    if (IS_A(HEAD(curr), "RefPartialContext")) {
      Val ref_name = AT(HEAD(curr), 0);
      Val child_nodes;
      bool found = ContextMap.find(context_map, ref_name, &child_nodes);
      if (!found) {
        COMPILE_ERROR("partial context not found: %.*s", (int)nb_string_byte_size(ref_name), nb_string_ptr(ref_name));
      }
      for (Val child_curr = child_nodes; child_curr != VAL_NIL; child_curr = TAIL(child_curr)) {
        if (child_curr != VAL_NIL) {
          res = nb_cons_new(child_curr, res);
        }
      }
    }
  }

  // yes it allocs more, but necessary to keep the order consistent
  ContextMap.insert(context_map, name, nb_cons_areverse(arena, res));
}

static void _remove_edges(DepNode** dep_network, int size, Val name) {
  for (int i = 0; i < size; i++) {
    DepNode* node = dep_network[i];
    DepNode* prev = node;
    DepNode* tail = node->next;
    while (tail) {
      if (val_eq(name, tail->ctx_name)) {
        prev->next = tail->next;
        tail = prev->next;
      } else {
        prev = tail;
        tail = tail->next;
      }
    }
  }
}

static void _print_network(DepNode** dep_network, int size) {
  printf("\n-----\n");
  for (int i = 0; i < size; i++) {
    DepNode* head = dep_network[i];
    while(head) {
      printf("%.*s ", (int)nb_string_byte_size(head->ctx_name), nb_string_ptr(head->ctx_name));
      head = head->next;
    }
    printf("\n");
  }
}

static DepNode* _dep_node_new(Arena* node_arena) {
  DepNode* node = arena_slot_alloc(node_arena, (sizeof(DepNode) + 7) / 8);
  node->next = NULL;
  return node;
}

// topological sort lex contexts, and inline all refs
// TODO integrate with external language definition
void sb_inline_partial_references(CompileCtx* ctx) {
  // build partial node map and dep graph
  Val ins_list = AT(ctx->ast, 0);

  // node map for query
  struct ContextMap context_map;
  ContextMap.init(&context_map);
  size_t size = 0;
  for (Val curr_ins = ins_list; curr_ins != VAL_NIL; curr_ins = TAIL(curr_ins)) {
    Val child = HEAD(curr_ins);
    if (IS_A(child, "Lex")) { // Lex[context, rules]
      Val name = AT(child, 0);
      // todo error for duplicated lex name
      ContextMap.insert(&context_map, name, AT(child, 1));
      size++;
    }
  }

  // build toposort data structure
  // dep_network: array of dep_node links
  DepNode* dep_network[size];
  memset(dep_network, 0, sizeof(DepNode*) * size);
  Arena* node_arena = arena_new();
  int dep_network_size = 0;
  ContextMapIter it;
  for (ContextMap.iter_init(&it, &context_map); !ContextMap.iter_is_end(&it); ContextMap.iter_next(&it)) {
    Val name = it.slot->k;
    DepNode* tail = _dep_node_new(node_arena);
    tail->ctx_name = name;
    dep_network[dep_network_size] = tail;
    Val rule_body = it.slot->v;
    for (Val rule_cons = rule_body; rule_cons != VAL_NIL; rule_cons = TAIL(rule_cons)) {
      Val rule = HEAD(rule_cons);
      if (IS_A(rule, "RefPartialContext")) { // RefPartialContext[context]
        tail->next = _dep_node_new(node_arena);
        tail = tail->next;
        tail->ctx_name = AT(rule, 0);
      }
    }
    dep_network_size++;
  }

  // toposort and expand
  for (; dep_network_size > 0; dep_network_size--) {
    Val name;
    if (_pop_zero_deg_node(dep_network, dep_network_size, &name)) {
      dep_network_size--;
      // _print_network(dep_network, dep_network_size);
      _remove_edges(dep_network, dep_network_size, name);
      _expand_lex_def(ctx->arena, &context_map, name);
    } else {
      log_err("loop dependency found:");
      _print_network(dep_network, dep_network_size);
      ContextMap.cleanup(&context_map);
      arena_delete(node_arena);
      COMPILE_ERROR("fatal");
    }
  }

  // update lex nodes
  Val res = VAL_NIL;
  for (Val curr_ins = ins_list; curr_ins != VAL_NIL; curr_ins = TAIL(curr_ins)) {
    Val child = HEAD(curr_ins);
    if (IS_A(child, "Lex")) { // Lex[context, rules]
      Val name = AT(child, 0);
      if (nb_string_ptr(name)[0] != '*') {
        Val converted;
        ContextMap.find(&context_map, name, &converted);
        res = nb_cons_anew(ctx->arena, child, res);
      }
    } else {
      res = nb_cons_anew(ctx->arena, child, res);
    }
  }

  REPLACE(ctx->ast, nb_struct_set(ctx->ast, 0, res));

  ContextMap.cleanup(&context_map);
  arena_delete(node_arena);
}
