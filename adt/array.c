#include "array.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// immutable array implemented as W_MAX-way tree

// for optimized queue-like operations, a copy-on-write ArraySlice is generated when shifting or slicing elements.
// nodes are allocated by reference counting GC, and transient in-place updates are performed on nodes of refcount == 1
// no tail-inlining, only root-inlining, optimization for appending is done by transient updates.

// todo add head and tail to slice, then we can reduce all of the re-allocation of data append actions to 1/32

#define W 5
#define W_MAX (1ULL << W)
#define W_MASK (W_MAX - 1)
#define SHAPE_SLICE OBJ_SHAPE_A_SLICE
#define SHAPE_ARRAY OBJ_SHAPE_ARRAY

// the `size` field is shared by both Array and Slice
// depth: start from 0
//   0:  0..W_MAX leaf nodes
//   W:  W_MAX+1..W_MAX**2 leaf nodes
//   2W: W_MAX**2+1..W_MAX**3 leaf nodes
//   ...
#define HEADER_FIELDS \
  uint64_t ref_count;\
  uint32_t klass;\
  uint16_t is_slice;\
  uint16_t depth;\
  uint64_t size

typedef struct {
  HEADER_FIELDS;
} Header;

typedef struct {
  uint64_t ref_count;
  uint64_t size;
  Val slots[];
} Node;

typedef struct {
  HEADER_FIELDS;
  Node root;
} Array;

typedef struct {
  HEADER_FIELDS;
  uint64_t offset;
  Val ref;
} Slice;

#define ARR_BYTES(_a_) (sizeof(Array) + sizeof(Val) * (_a_)->root.size)

#define NODE_BYTES(_n_) (sizeof(Node) + sizeof(Val) * (_n_)->size)

inline static Node* NODE_NEW(uint64_t size) {
  Node* r = val_alloc(sizeof(Node) + sizeof(Val) * size);
  r->size = size;
  return r;
}

inline static Node* NODE_DUP(Node* n) {
  size_t size = NODE_BYTES(n);
  for (int i = 0; i < n->size; i++) {
    RETAIN(n->slots[i]);
  }

  return val_dup(n, size, size);
}

// dup and append 1 empty slot chain, and append v
inline static Node* NODE_DUP_APPEND(Node* n, int level, Val v) {
  assert(n->size < W_MAX);
  size_t size = NODE_BYTES(n);
  for (int i = 0; i < n->size; i++) {
    RETAIN(n->slots[i]);
  }

  RETAIN(v);
  Node* r = val_dup(n, size, size + sizeof(Val));
  for (; level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->slots[r->size++] = v;
  return r;
}

inline static bool NODE_INDEX_OVERFLOW(Node* n, int level, uint64_t pos) {
  return ((pos >> level) & W_MASK) >= n->size;
}

inline static Array* ARR_NEW(uint64_t root_size) {
  Array* a = val_alloc(sizeof(Array) + sizeof(Val) * root_size);
  a->klass = KLASS_ARRAY;
  // a->is_slice = 0;
  a->root.size = root_size;
  return a;
}

inline static Array* ARR_DUP(Array* a) {
  size_t sz = ARR_BYTES(a);
  for (int i = 0; i < a->root.size; i++) {
    RETAIN(a->root.slots[i]);
  }

  Array* r = val_dup(a, sz, sz);
  return r;
}

// a can be 0-sized
// dup and append 1 slot of v
inline static Array* ARR_DUP_APPEND(Array* a, Val v) {
  assert(a->root.size < W_MAX);
  size_t sz = ARR_BYTES(a);
  Array* r = val_dup(a, sz, sz + sizeof(Val));
  for (int i = 0; i < a->root.size; i++) {
    RETAIN(a->root.slots[i]);
  }

  RETAIN(v); // retain before `while` loop changes it
  for (int level = a->depth; level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->root.slots[r->root.size++] = v;
  r->size++;
  return r;
}

// NOTE 0-sized array is surely not full
inline static bool ARR_IS_FULL(Array* a) {
  return a->size == (W_MAX << a->depth);
}

// every elem of root is full
// NOTE 0-sized array is surely partial full
inline static bool ARR_IS_PARTIAL_FULL(Array* a) {
  return ((a->size >> a->depth) << a->depth) == a->size;
}

inline static Slice* SLICE_NEW() {
  Slice* s = val_alloc(sizeof(Slice));
  s->klass = KLASS_ARRAY;
  s->is_slice = 1;
  return s;
}

// raised array will ensure there are 2 slots in root, and append v
inline static Array* ARR_RAISE_DEPTH(Array* a, Val v) {
  Array* r = ARR_NEW(2);
  r->size = a->size + 1;
  r->depth = a->depth + W;
  r->root.size = 2;
  r->root.slots[0] = (Val)NODE_DUP(&a->root); // NOTE in dup, elems in a->root are RETAIN()ed

  RETAIN(v);
  for (int level = r->depth; level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->root.slots[1] = v;

  return r;
}

inline static int DEPTH_FOR(uint64_t sz) {
  int d = 0;
  while (sz > (W_MAX << d)) {
    d += W;
  }
  return d;
}

inline static int ROOT_SIZE_FOR(uint64_t size, int depth) {
  // root_size must satisfy:
  //   root_size * (1ULL << depth) >= size
  int r = size >> depth;
  if ((r << depth) < size) {
    r++;
  }
  return r;
}

#pragma mark --- helpers decl

static Val _array_get(Array* a, int64_t pos);
static void _array_transient_set(Array* a, int64_t pos, Val e);
static Array* _array_set(Array* a, int64_t pos, Val e);
static Array* _slice_set(Slice* s, int64_t pos, Val e);
static Array* _array_append(Array* a, Val e);
void _node_debug(Node* node, int depth);

void _destroy(void* p);
void _node_release(Node* node, int depth);

#pragma mark --- interface

static Val empty_arr;
static void _init() __attribute__((constructor(100)));
static void _init() {
  Array* a = ARR_NEW(0);
  val_perm(a);
  empty_arr = (Val)a;

  val_register_destructor_func(KLASS_ARRAY, _destroy);
}

Val nb_array_new_empty() {
  return empty_arr;
}

Val nb_array_new(size_t size, ...) {
  int depth = DEPTH_FOR(size);
  int root_size = ROOT_SIZE_FOR(size, depth);
  Array* r = ARR_NEW(root_size);
  r->size = size;
  r->depth = depth;

  va_list vl;
  va_start(vl, size);
  for (size_t i = 0; i < size; i++) {
    Val e = va_arg(vl, Val);
    _array_transient_set(r, i, e);
  }
  va_end(vl);

  return (Val)r;
}

Val nb_array_new_v(size_t size, va_list vl) {
  int depth = DEPTH_FOR(size);
  int root_size = ROOT_SIZE_FOR(size, depth);
  Array* r = ARR_NEW(root_size);
  r->size = size;
  r->depth = depth;

  for (size_t i = 0; i < size; i++) {
    _array_transient_set(r, i, va_arg(vl, Val));
  }
  return (Val)r;
}

Val nb_array_new_a(size_t size, Val* p) {
  int depth = DEPTH_FOR(size);
  int root_size = ROOT_SIZE_FOR(size, depth);
  Array* r = ARR_NEW(root_size);
  r->size = size;
  r->depth = depth;

  for (size_t i = 0; i < size; i++) {
    _array_transient_set(r, i, p[i]);
  }
  return (Val)r;
}

size_t nb_array_size(Val v) {
  Header* h = (Header*)v;
  return h->size;
}

Val nb_array_get(Val v, int64_t pos) {
  // unify pos
  Header* h = (Header*)v;
  if (pos < 0) {
    pos += h->size;
    if (pos < 0) {
      // todo out of bound error
      return VAL_UNDEF;
    }
  }
  if (pos >= h->size) {
    return VAL_UNDEF;
  }

  // get real array
  Array* a;
  if (h->is_slice) {
    Slice* s = (Slice*)v;
    a = (Array*)s->ref;
    pos += s->offset;
  } else {
    a = (Array*)v;
  }

  return _array_get(a, pos);
}

Val nb_array_set(Val v, int64_t pos, Val e) {
  Header* h = (Header*)v;
  if (pos < 0) {
    pos += h->size;
    if (pos < 0) {
      // todo out of bound error
      pos = 0;
    }
  }

  if (h->is_slice) {
    return (Val)_slice_set((Slice*)v, pos, e);
  } else {
    return (Val)_array_set((Array*)v, pos, e);
  }
}

Val nb_array_append(Val v, Val e) {
  Header* h = (Header*)v;
  if (h->is_slice) {
    return (Val)_slice_set((Slice*)v, h->size, e);
  } else {
    return (Val)_array_append((Array*)v, e);
  }
}

Val nb_array_slice(Val v, uint64_t from, uint64_t len) {
  Header* h = (Header*)v;
  if (from >= h->size) {
    return empty_arr;
  }

  Slice* r = SLICE_NEW();

  if (h->is_slice) {
    Slice* s = (Slice*)v;
    r->offset = s->offset + from;
    r->size = (from + len < s->size) ? len : s->size - from;
    r->ref = s->ref;
  } else {
    r->offset = from;
    r->size = (from + len < h->size) ? len : h->size - from;
    r->ref = v;
  }
  RETAIN(r->ref);

  return (Val)r;
}

Val nb_array_remove(Val v, int64_t pos) {
  Header* h = (Header*)v;
  if (pos < 0) {
    pos += h->size;
    if (pos < 0) {
      RETAIN(v); // NOTE the "new array" has ref_count=1
      return v;
    }
  } else if (pos >= h->size) { // covers h->size == 0
    RETAIN(v);
    return v;
  }

  assert(h->size > 0);
  if (pos == 0) {
    return nb_array_slice(v, 1, h->size - 1);
  } else if (pos == h->size - 1) {
    return nb_array_slice(v, 0, h->size - 1);
  } else {
    int64_t offset;
    Array* src_arr;
    if (h->is_slice) {
      Slice* s = (Slice*)v;
      offset = s->offset;
      src_arr = (Array*)s->ref;
    } else {
      offset = 0;
      src_arr = (Array*)v;
    }

    // dirty copy

    int64_t size = h->size - 1;
    int depth = DEPTH_FOR(size);
    int root_size = ROOT_SIZE_FOR(size, depth);
    Array* r = ARR_NEW(root_size);
    r->size = size;
    r->depth = depth;

    int64_t j = offset;
    for (int64_t i = 0; i < pos; i++) {
      _array_transient_set(r, i, _array_get(src_arr, j++));
    }
    j++;
    for (int64_t i = pos; i < size ; i++) {
      _array_transient_set(r, i, _array_get(src_arr, j++));
    }
    return (Val)r;
  }
}

Val nb_array_build_test_10() {
  Array* a = ARR_NEW(10);
  a->size = 10;
  a->depth = 0;
  for (long i = 0; i < 10; i++) {
    a->root.slots[i] = VAL_FROM_INT(i);
  }
  return (Val)a;
}

// 17 * 32 + 1 * 2
Val nb_array_build_test_546() {
  Array* a = ARR_NEW(18);
  a->size = 546;
  a->depth = W;

  long e = 0;
  for (long i = 0; i < 17; i++) {
    Node* n = NODE_NEW(32);
    a->root.slots[i] = (Val)n;
    for (long j = 0; j < 32; j++) {
      n->slots[j] = VAL_FROM_INT(e++);
    }
  }
  Node* n = NODE_NEW(2);
  for (long i = 0; i < 2; i++) {
    n->slots[i] = VAL_FROM_INT(e++);
  }
  a->root.slots[17] = (Val)n;
  return (Val)a;
}

void nb_array_debug(Val v) {
  Header* h = (Header*)v;
  if (!h->is_slice) {
    Array* a = (Array*)v;
    printf("<array addr=%p size=%llu depth=%u root_size=%llu>\n",
    a, a->size, a->depth, a->root.size);
    _node_debug(&a->root, a->depth);
  } else if (h->klass == KLASS_ARRAY) {
    Slice* s = (Slice*)v;
    printf("<array_slice size=%llu offset=%llu ref=%p>\n", s->size, s->offset, (void*)s->ref);
  } else {
    printf("not array\n");
  }
}

#pragma mark --- helpers impl

static Val _array_get(Array* a, int64_t pos) {
  assert(pos < a->size);
  Node* node = &(a->root);
  for (int i = a->depth; i; i -= W) {
    size_t index = ((pos >> i) & W_MASK);
    node = (Node*)node->slots[index];
    assert(node);
  }
  Val v = node->slots[pos & W_MASK];
  RETAIN(v);
  return v;
}

// assume we are building an array, the slot to be filled is empty in the beginning
static void _array_transient_set(Array* a, int64_t pos, Val e) {
  Node* root = &a->root;
  Node** node = &root;
  bool is_last_node = true; // whether is the last node on parent
  for (int i = a->depth; i >= 0; i -= W) {
    if (*node == NULL) {
      size_t node_size = is_last_node ? (a->size >> i) : W_MAX;
      *node = NODE_NEW(node_size);
    }
    size_t index = ((pos >> i) & W_MASK);
    if (i) {
      is_last_node = (index == (*node)->size - 1);
      node = (Node**)((*node)->slots + index);
    } else {
      (*node)->slots[index] = e;
      RETAIN(e);
    }
  }
}

static Array* _array_set(Array* a, int64_t pos, Val e) {
  if (pos >= a->size) {
    for (int64_t i = a->size; i < pos; i++) {
      a = _array_append(a, VAL_NIL);
    }
    return _array_append(a, e);
  } else {
    // dup path and set
    Array* r = ARR_DUP(a);

    Node* node = &r->root;
    size_t index;
    for (int i = a->depth; i; i -= W) {
      index = ((pos >> i) & W_MASK);
      RELEASE(node->slots[index]);
      node->slots[index] = (Val)NODE_DUP((Node*)node->slots[index]);
      node = (Node*)node->slots[index];
    }
    index = (pos & W_MASK);
    RELEASE(node->slots[index]);
    node->slots[index] = e;
    RETAIN(e);
    return r;
  }
}

static Array* _slice_set(Slice* s, int64_t pos, Val e) {
  Array* a = (Array*)s->ref;
  Val r = empty_arr;
  int64_t j = s->offset;

  if (pos < s->size) {
    for (int64_t i = 0; i < s->size; i++) {
      REPLACE(r, (Val)_array_append((Array*)r, i == pos ? e : _array_get(a, j++)));
    }
    return (Array*)r;
  } else {
    for (int64_t i = 0; i < s->size; i++) {
      REPLACE(r, (Val)_array_append((Array*)r, _array_get(a, j++)));
    }
    for (int64_t i = s->size; i < pos; i++) {
      REPLACE(r, (Val)_array_append((Array*)r, VAL_NIL));
    }
    REPLACE(r, (Val)_array_append((Array*)r, e));
    return (Array*)r;
  }
}

static Array* _array_append(Array* a, Val e) {
  Array* r;
  if (ARR_IS_FULL(a)) {
    return ARR_RAISE_DEPTH(a, e);
  } else if (ARR_IS_PARTIAL_FULL(a)) { // covers a->depth == 0
    return ARR_DUP_APPEND(a, e);
  }
  assert(a->depth);

  r = ARR_DUP(a);

  size_t pos = r->size;
  r->size++;
  Node* parent = &r->root;

  // hierarchical dup slot
  int i;
  size_t index;
  size_t next_index;
  for (i = r->depth; i; i -= W) {
    index = ((pos >> i) & W_MASK);
    Node* child = (Node*)parent->slots[index];
    if (NODE_INDEX_OVERFLOW(child, i - W, pos)) { // covers i - W == 0
      Val new_child = (Val)NODE_DUP_APPEND(child, i - W, e);
      RELEASE(child);
      parent->slots[index] = new_child;
      return r;
    }

    Val new_child = (Val)NODE_DUP(child);
    RELEASE(child);
    parent->slots[index] = new_child;
    parent = child;
  }
  PDLEX_UNREACHABLE();
}

void _node_debug(Node* node, int depth) {
  if (depth > 0) {
    printf("<node depth=%d size=%llu slots=[", depth, node->size);
    for (long i = 0; i < node->size; i++) {
      printf("%p, ", (void*)node->slots[i]);
    }
    printf("]\n");
    for (long i = 0; i < node->size; i++) {
      _node_debug((Node*)node->slots[i], depth - W);
    }
    if (depth == W * 2) {
      printf("\n");
    }
  } else {
    printf("<leaf size=%llu slots=[", node->size);
    for (long i = 0; i < node->size; i++) {
      printf("%lu, ", node->slots[i]);
    }
    printf("]>\n");
  }
}

void _destroy(void* p) {
  Header* h = p;
  if (h->is_slice) {
    Slice* s = p;
    val_release(s->ref);
  } else {
    Array* a = p;
    if (a->depth) {
      for (int i = 0; i < a->root.size; i++) {
        _node_release((Node*)a->root.slots[i], a->depth - W);
      }
    } else {
      for (int i = 0; i < a->root.size; i++) {
        val_release(a->root.slots[i]);
      }
    }
  }
}

void _node_release(Node* node, int depth) {
  if (VAL_IS_PERM(node)) {
    return;
  }

  if (node->ref_count > 1) {
    node->ref_count--;
    return;
  }

  if (depth) {
    for (int i = 0; i < node->size; i++) {
      _node_release((Node*)node->slots[i], depth - W);
    }
  } else {
    for (int i = 0; i < node->size; i++) {
      val_release(node->slots[i]);
    }
  }
  val_free(node);
}
