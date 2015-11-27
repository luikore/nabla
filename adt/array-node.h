#pragma once

// array node

#include "val.h"

#define W 5
#define W_MAX (1ULL << W)
#define W_MASK (W_MAX - 1)

typedef struct {
  ValHeader h; // flags:size
  Val slots[];
} Node;

#define NODE_SIZE(node) ((ValHeader*)(node))->flags
#define NODE_BYTES(n) (sizeof(Node) + sizeof(Val) * NODE_SIZE(n))

static Node* NODE_NEW(uint64_t size) {
  Node* r = val_alloc(sizeof(Node) + sizeof(Val) * size);
  r->h.klass = KLASS_ARRAY_NODE;
  NODE_SIZE(r) = size;
  return r;
}

static Node* NODE_DUP(Node* n) {
  assert(VAL_KLASS((Val)n) == KLASS_ARRAY_NODE);
  size_t size = NODE_BYTES(n);
  for (int i = 0; i < NODE_SIZE(n); i++) {
    RETAIN(n->slots[i]);
  }

  void* r = val_dup(n, size, size);
  assert(VAL_KLASS((Val)r) == KLASS_ARRAY_NODE);
  return r;
}

// dup and append 1 empty slot chain, and append v
static Node* NODE_DUP_APPEND(Node* n, int level, Val v) {
  assert(NODE_SIZE(n) < W_MAX);
  assert(VAL_KLASS((Val)n) == KLASS_ARRAY_NODE);
  size_t bytes = NODE_BYTES(n);
  for (int i = 0; i < NODE_SIZE(n); i++) {
    RETAIN(n->slots[i]);
  }

  RETAIN(v);
  Node* r = val_dup(n, bytes, bytes + sizeof(Val));
  for (; level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->slots[NODE_SIZE(r)++] = v;
  assert(VAL_KLASS((Val)r) == KLASS_ARRAY_NODE);
  return r;
}

static void NODE_DESTROY(void* vn) {
  Node* n = vn;
  if (val_is_tracing()) {
    val_end_trace();
  }
  for (int i = 0; i < NODE_SIZE(n); i++) {
    val_release(n->slots[i]);
  }
}
