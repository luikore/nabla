#pragma once

// (map internal node)
// popcnt powered node

#include "val.h"

// key and value
typedef struct {
  Val k;
  Val v;
} Kv;

struct NodeStruct;
typedef struct NodeStruct Node;

// node and bitmap
typedef struct {
  Node* n;
  uint64_t b;
} NodeBm;

typedef union {
  Kv kv;
  NodeBm nb;
  Val h;
} Slot;

struct NodeStruct {
  ValHeader header; // klass = KLASS_MAP_NODE, flags = level, user1 = int valued
  int64_t size;
  Slot slots[];     // 0...2**W (W=6 for 64bit; can be 0 or 1 elem due to remove actions)
};

#define W 6
#define W_MASK ((1ULL << W) - 1)
#define MAX_NODE_LEVEL ((1ULL << W) / W * W - W)
#define IS_COLA_LEVEL(_level_) ((_level_) == MAX_NODE_LEVEL)

// for both node and cola
#define SIZE(n) (n)->size
#define LEVEL(n) (n)->header.flags

// which bit is set in the mask
static int GET_POS(uint64_t hash, int level) {
  return ((hash >> level) & W_MASK);
}

// mask on the bitmap
static uint64_t GET_FLAG(int pos) {
  return (1ULL << (pos));
}

static bool COLLIDE(uint64_t h1, uint64_t h2, int level) {
  return GET_POS(h1, level) == GET_POS(h2, level);
}

#define BM_TEST_POS(_bm_, _pos_) ((_bm_ >> _pos_) & 1)
static void BM_SET_FLAG(uint64_t* bm, uint64_t flag, bool set) {
  if (set) {
    *bm |= flag;
  } else {
    *bm &= ~flag;
  }
}
#define BM_INDEX(_bm_, _pos_) NB_POPCNT(_bm_ & ((1ULL << (_pos_)) - 1))

#define IS_INT_VALUED(node_or_cola) ((node_or_cola)->header.user1)

#define SLOT_IS_NODE(s) (VAL_KLASS((s)->h) == KLASS_MAP_NODE)

#define SLOT_IS_COLA(s) (VAL_KLASS((s)->h) == KLASS_MAP_COLA)

#define SLOT_IS_KV(s) (!SLOT_IS_NODE(s) && !SLOT_IS_COLA(s))

static void KV_RETAIN(Kv kv, bool is_int_valued) {
  RETAIN(kv.k);
  if (!is_int_valued) {
    RETAIN(kv.v);
  }
}

static void KV_RELEASE(Kv kv, bool is_int_valued) {
  RELEASE(kv.k);
  if (!is_int_valued) {
    RELEASE(kv.v);
  }
}

static void SLOT_RETAIN(Slot* slot, bool is_int_valued) {
  if (SLOT_IS_KV(slot)) {
    KV_RETAIN(slot->kv, is_int_valued);
  } else {
    RETAIN(slot->h);
  }
}

static void SLOT_RELEASE(Slot* slot, bool is_int_valued) {
  if (SLOT_IS_KV(slot)) {
    KV_RELEASE(slot->kv, is_int_valued);
  } else {
    RELEASE(slot->h);
  }
}

#pragma mark ### node

static Node* NODE_ALLOC(size_t size, int level, bool is_int_valued) {
  Node* node = val_alloc(sizeof(Node) + sizeof(Slot) * size);
  node->header.klass = KLASS_MAP_NODE;
  IS_INT_VALUED(node) = is_int_valued;
  LEVEL(node) = level;
  SIZE(node) = size;
  return node;
}

static NodeBm NODE_WRAP_SLOT(Slot slot, uint64_t hash, int level, bool is_int_valued) {
  Node* node = NODE_ALLOC(1, level, is_int_valued);
  node->slots[0] = slot;

  NodeBm nb = {.n = node, .b = 0};
  int pos = GET_POS(hash, level);
  uint64_t flag = GET_FLAG(pos);
  BM_SET_FLAG(&nb.b, flag, true);

  return nb;
}

// prereq: kv.k != k
static NodeBm NODE_NEW2(Kv kv, uint64_t hash, Val k, Val v, int level, bool is_int_valued) {
  Node* node = NODE_ALLOC(2, level, is_int_valued);

  uint64_t hash1 = val_hash(kv.k);
  int pos1 = GET_POS(hash1, level);
  int pos2 = GET_POS(hash, level);
  if (pos1 < pos2) {
    node->slots[0].kv = kv;
    node->slots[1].kv.k = k;
    node->slots[1].kv.v = v;
  } else {
    assert(pos1 > pos2);
    node->slots[1].kv = kv;
    node->slots[0].kv.k = k;
    node->slots[0].kv.v = v;
  }

  NodeBm nb = {.n = node, .b = 0};
  BM_SET_FLAG(&nb.b, GET_FLAG(pos1), true);
  BM_SET_FLAG(&nb.b, GET_FLAG(pos2), true);

  RETAIN(kv.k);
  RETAIN(k);
  if (!is_int_valued) {
    RETAIN(kv.v);
    RETAIN(v);
  }
  return nb;
}

// return NULL if not found
static Slot* NODE_FIND_SLOT(Node* n, uint64_t bitmap, uint64_t hash) {
  int pos = GET_POS(hash, LEVEL(n));
  size_t index = BM_INDEX(bitmap, pos);
  if (!BM_TEST_POS(bitmap, pos)) {
    return NULL;
  }
  return n->slots + index;
}

// purely dup
static Node* NODE_DUP(Node* n) {
  int bytes = sizeof(Node) + sizeof(Slot) * SIZE(n);
  Node* new_n = val_dup(n, bytes, bytes);
  for (int i = 0; i < SIZE(n); i++) {
    SLOT_RETAIN(new_n->slots + i, IS_INT_VALUED(new_n));
  }
  return new_n;
}

// dup and replace with slot
// prereq: slot is non-empty
static Node* NODE_DUP_REPLACE(Node* n, uint64_t bitmap, uint64_t hash, Slot replacement) {
  Node* new_n = NODE_ALLOC(SIZE(n), LEVEL(n), IS_INT_VALUED(n));

  int pos = GET_POS(hash, LEVEL(n));
  size_t index = BM_INDEX(bitmap, pos);

  for (int i = 0; i < SIZE(n); i++) {
    if (i == index) {
      new_n->slots[i] = replacement;
    } else {
      new_n->slots[i] = n->slots[i];
      SLOT_RETAIN(new_n->slots + i, IS_INT_VALUED(n));
    }
  }
  return new_n;
}

// prereq: hash not hit
// the added slot is left in VAL_UNDEF
static NodeBm NODE_BM_INSERT(NodeBm nb, uint64_t hash) {
  Node* old_node = nb.n;

  int level = LEVEL(old_node);
  int64_t new_size = SIZE(old_node) + 1;
  Node* new_node = NODE_ALLOC(new_size, level, IS_INT_VALUED(old_node));
  NodeBm res = {.n = new_node, .b = nb.b};

  int pos = GET_POS(hash, level);
  size_t index = BM_INDEX(nb.b, pos);
  uint64_t flag = GET_FLAG(pos);
  BM_SET_FLAG(&res.b, flag, true);

  for (int i = 0, j = 0; i < SIZE(old_node); i++) {
    if (i == index) {
      new_node->slots[j].kv.k = VAL_UNDEF;
      new_node->slots[j].kv.v = VAL_UNDEF;
      j++;
    }
    SLOT_RETAIN(old_node->slots + i, IS_INT_VALUED(old_node));
    new_node->slots[j++] = old_node->slots[i];
  }
  if (index == SIZE(old_node)) {
    new_node->slots[index].kv.k = VAL_UNDEF;
    new_node->slots[index].kv.v = VAL_UNDEF;
  }

  return res;
}

// prereq: hash hit
static NodeBm NODE_BM_REMOVE(NodeBm nb, uint64_t hash) {
  Node* old_node = nb.n;
  int level = LEVEL(old_node);
  Node* new_node = NODE_ALLOC(SIZE(old_node) - 1, level, IS_INT_VALUED(old_node));
  NodeBm res = {.n = new_node, .b = nb.b};

  int pos = GET_POS(hash, LEVEL(old_node));
  size_t index = BM_INDEX(nb.b, pos);
  uint64_t flag = GET_FLAG(pos);
  BM_SET_FLAG(&res.b, flag, false);

  for (int i = 0, j = 0; i < SIZE(old_node); i++) {
    if (i == index) {
      continue;
    }
    SLOT_RETAIN(old_node->slots + i, IS_INT_VALUED(old_node));
    new_node->slots[j++] = old_node->slots[i];
  }

  return res;
}

static void NODE_DESTROY(void* ptr) {
  Node* node = ptr;

  for (int i = 0; i < SIZE(node); i++) {
    Slot* slot = node->slots + i;
    SLOT_RELEASE(slot, IS_INT_VALUED(node));
  }
}
