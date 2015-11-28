#include "array.h"
#include "array-node.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// immutable array implemented as W_MAX-way tree

// for optimized queue-like operations, a copy-on-write ArraySlice is generated when shifting or slicing elements.
// nodes are allocated by reference counting GC, and transient in-place updates are performed on nodes of refcount == 1
// no tail-inlining, only root-inlining, optimization for appending is done by transient updates.

// todo add head and tail to slice, then we can reduce all of the re-allocation of data append actions to 1/32

// the `size` field is shared by both Array and Slice
// depth: start from 0
//   0:  0..W_MAX leaf nodes
//   W:  W_MAX+1..W_MAX**2 leaf nodes
//   2W: W_MAX**2+1..W_MAX**3 leaf nodes
//   ...

typedef struct {
  ValHeader h; // flags: depth
  uint64_t size;
  uint64_t root_size;
  Val slots[];
} Array;

typedef struct {
  ValHeader h;
  uint64_t size;
  uint64_t offset;
  Val ref;
} Slice;

#define ROOT_SIZE(a) ((Array*)(a))->root_size
#define ARR_SIZE(a) ((Array*)(a))->size
#define ARR_BYTES(a) (sizeof(Array) + sizeof(Val) * ROOT_SIZE(a))

#define ARR_IS_SLICE(a) ((ValHeader*)(a))->user1
#define ARR_DEPTH(a) ((ValHeader*)(a))->flags

inline static bool NODE_INDEX_OVERFLOW(Node* n, int level, uint64_t pos) {
  return ((pos >> level) & W_MASK) >= NODE_SIZE(n);
}

inline static Array* ARR_NEW(uint64_t root_size) {
  Array* a = val_alloc(sizeof(Array) + sizeof(Val) * root_size);
  a->h.klass = KLASS_ARRAY;
  // ARR_IS_SLICE(a) = 0;
  ROOT_SIZE(a) = root_size;
  return a;
}

inline static Array* ARR_DUP(Array* a) {
  size_t sz = ARR_BYTES(a);
  for (int i = 0; i < ROOT_SIZE(a); i++) {
    RETAIN(a->slots[i]);
  }

  Array* r = val_dup(a, sz, sz);
  return r;
}

// a can be 0-sized
// dup and append 1 slot of v
inline static Array* ARR_DUP_APPEND(Array* a, Val v) {
  assert(ROOT_SIZE(a) < W_MAX);
  size_t sz = ARR_BYTES(a);
  Array* r = val_dup(a, sz, sz + sizeof(Val));
  for (int i = 0; i < ROOT_SIZE(a); i++) {
    RETAIN(a->slots[i]);
  }

  RETAIN(v); // retain before `while` loop changes it
  for (int level = ARR_DEPTH(a); level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->slots[ROOT_SIZE(r)++] = v;
  r->size++;
  return r;
}

// NOTE 0-sized array is surely not full
inline static bool ARR_IS_FULL(Array* a) {
  return a->size == (W_MAX << ARR_DEPTH(a));
}

// every elem of root is full
// NOTE 0-sized array is surely partial full
inline static bool ARR_IS_PARTIAL_FULL(Array* a) {
  return ((a->size >> ARR_DEPTH(a)) << ARR_DEPTH(a)) == a->size;
}

inline static Slice* SLICE_NEW() {
  Slice* s = val_alloc(sizeof(Slice));
  s->h.klass = KLASS_ARRAY;
  ARR_IS_SLICE(s) = true;
  return s;
}

// raised array will ensure there are 2 slots in root, and append v
inline static Array* ARR_RAISE_DEPTH(Array* a, Val v) {
  Array* r = ARR_NEW(2);
  r->size = a->size + 1;
  ARR_DEPTH(r) = ARR_DEPTH(a) + W;
  ROOT_SIZE(r) = 2;

  Node* root_dup = NODE_NEW(ROOT_SIZE(a));
  for (int i = 0; i < ROOT_SIZE(a); i++) {
    root_dup->slots[i] = a->slots[i];
    RETAIN(root_dup->slots[i]);
  }
  r->slots[0] = (Val)root_dup;

  RETAIN(v);
  for (int level = ARR_DEPTH(r); level; level -= W) {
    Node* wrapper = NODE_NEW(1);
    wrapper->slots[0] = v;
    v = (Val)wrapper;
  }
  r->slots[1] = v;

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

static void ARR_DESTROY(void* p) {
  if (ARR_IS_SLICE(p)) {
    Slice* s = p;
    val_release(s->ref);
  } else {
    Array* a = p;
    for (int i = 0; i < ROOT_SIZE(a); i++) {
      val_release(a->slots[i]);
    }
  }
}

#pragma mark --- helpers decl

static Val _array_get(Array* a, int64_t pos);
static void _array_transient_set(Array* a, int64_t pos, Val e);
static Array* _array_set(Array* a, int64_t pos, Val e);
static Array* _slice_set(Slice* s, int64_t pos, Val e);
static Array* _array_append(Array* a, Val e);
void _node_debug(Node* node, int depth);

#pragma mark --- interface

static Val empty_arr;
static void _init() __attribute__((constructor(100)));
static void _init() {
  Array* a = ARR_NEW(0);
  val_perm(a);
  empty_arr = (Val)a;

  val_register_destructor_func(KLASS_ARRAY_NODE, NODE_DESTROY);
  val_register_destructor_func(KLASS_ARRAY, ARR_DESTROY);
}

Val nb_array_new_empty() {
  return empty_arr;
}

Val nb_array_new(size_t size, ...) {
  int depth = DEPTH_FOR(size);
  int root_size = ROOT_SIZE_FOR(size, depth);
  Array* r = ARR_NEW(root_size);
  r->size = size;
  ARR_DEPTH(r) = depth;

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
  ARR_DEPTH(r) = depth;

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
  ARR_DEPTH(r) = depth;

  for (size_t i = 0; i < size; i++) {
    _array_transient_set(r, i, p[i]);
  }
  return (Val)r;
}

size_t nb_array_size(Val v) {
  return ARR_SIZE(v);
}

Val nb_array_get(Val v, int64_t pos) {
  // unify pos
  if (pos < 0) {
    pos += ARR_SIZE(v);
    if (pos < 0) {
      // todo out of bound error
      return VAL_UNDEF;
    }
  }
  if (pos >= ARR_SIZE(v)) {
    return VAL_UNDEF;
  }

  // get real array
  Array* a;
  if (ARR_IS_SLICE(v)) {
    Slice* s = (Slice*)v;
    a = (Array*)s->ref;
    pos += s->offset;
  } else {
    a = (Array*)v;
  }

  return _array_get(a, pos);
}

Val nb_array_set(Val v, int64_t pos, Val e) {
  if (pos < 0) {
    pos += ARR_SIZE(v);
    if (pos < 0) {
      // todo out of bound error
      pos = 0;
    }
  }

  if (ARR_IS_SLICE(v)) {
    return (Val)_slice_set((Slice*)v, pos, e);
  } else {
    return (Val)_array_set((Array*)v, pos, e);
  }
}

Val nb_array_append(Val v, Val e) {
  if (ARR_IS_SLICE(v)) {
    return (Val)_slice_set((Slice*)v, ARR_SIZE(v), e);
  } else {
    return (Val)_array_append((Array*)v, e);
  }
}

Val nb_array_slice(Val v, uint64_t from, uint64_t len) {
  if (from >= ARR_SIZE(v)) {
    return empty_arr;
  }

  Slice* r = SLICE_NEW();

  if (ARR_IS_SLICE(v)) {
    Slice* s = (Slice*)v;
    r->offset = s->offset + from;
    r->size = (from + len < s->size) ? len : s->size - from;
    r->ref = s->ref;
  } else {
    r->offset = from;
    r->size = (from + len < ARR_SIZE(v)) ? len : ARR_SIZE(v) - from;
    r->ref = v;
  }
  RETAIN(r->ref);

  return (Val)r;
}

Val nb_array_remove(Val v, int64_t pos) {
  if (pos < 0) {
    pos += ARR_SIZE(v);
    if (pos < 0) {
      RETAIN(v); // NOTE the "new array" has ref_count=1
      return v;
    }
  } else if (pos >= ARR_SIZE(v)) { // covers ARR_SIZE(v) == 0
    RETAIN(v);
    return v;
  }

  assert(ARR_SIZE(v) > 0);
  if (pos == 0) {
    return nb_array_slice(v, 1, ARR_SIZE(v) - 1);
  } else if (pos == ARR_SIZE(v) - 1) {
    return nb_array_slice(v, 0, ARR_SIZE(v) - 1);
  } else {
    int64_t offset;
    Array* src_arr;
    if (ARR_IS_SLICE(v)) {
      Slice* s = (Slice*)v;
      offset = s->offset;
      src_arr = (Array*)s->ref;
    } else {
      offset = 0;
      src_arr = (Array*)v;
    }

    // dirty copy

    int64_t size = ARR_SIZE(v) - 1;
    int depth = DEPTH_FOR(size);
    int root_size = ROOT_SIZE_FOR(size, depth);
    Array* r = ARR_NEW(root_size);
    r->size = size;
    ARR_DEPTH(r) = depth;

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
  ARR_DEPTH(a) = 0;
  for (long i = 0; i < 10; i++) {
    a->slots[i] = VAL_FROM_INT(i);
  }
  return (Val)a;
}

// 17 * 32 + 1 * 2
Val nb_array_build_test_546() {
  Array* a = ARR_NEW(18);
  a->size = 546;
  ARR_DEPTH(a) = W;

  long e = 0;
  for (long i = 0; i < 17; i++) {
    Node* n = NODE_NEW(32);
    a->slots[i] = (Val)n;
    for (long j = 0; j < 32; j++) {
      n->slots[j] = VAL_FROM_INT(e++);
    }
  }
  Node* n = NODE_NEW(2);
  for (long i = 0; i < 2; i++) {
    n->slots[i] = VAL_FROM_INT(e++);
  }
  a->slots[17] = (Val)n;
  return (Val)a;
}

void nb_array_debug(Val v) {
  if (VAL_KLASS(v) == KLASS_ARRAY) {
    if (!ARR_IS_SLICE(v)) {
      Array* a = (Array*)v;
      printf("<array addr=%p size=%llu extra_rc=%hu>\n",
      a, a->size, ((ValHeader*)a)->extra_rc);

      printf("<root depth=%d size=%llu slots=[", ARR_DEPTH(a), ROOT_SIZE(a));
      for (long i = 0; i < ROOT_SIZE(a); i++) {
        printf("%p, ", (void*)a->slots[i]);
      }
      printf("]>\n");

      if (ARR_DEPTH(a) > 0) {
        for (int i = 0; i < ROOT_SIZE(a); i++) {
          _node_debug((Node*)a->slots[i], ARR_DEPTH(a) - W);
        }
      }
    } else {
      Slice* s = (Slice*)v;
      printf("<array_slice size=%llu offset=%llu ref=%p extra_rc=%hu>\n",
      s->size, s->offset, (void*)s->ref, ((ValHeader*)s)->extra_rc);
    }
  } else {
    printf("not array\n");
  }
}

#pragma mark --- helpers impl

static Val _array_get(Array* a, int64_t pos) {
  assert(pos < a->size);
  Val* slots = a->slots;
  for (int i = ARR_DEPTH(a); i; i -= W) {
    size_t index = ((pos >> i) & W_MASK);
    Node* node = (Node*)slots[index];
    assert(node);
    slots = node->slots;
  }
  Val v = slots[pos & W_MASK];
  RETAIN(v);
  return v;
}

// assume we are building an array, the slot to be filled is empty in the beginning
static void _array_transient_set(Array* a, int64_t pos, Val e) {
  Val* slots = a->slots;
  size_t slots_size = ROOT_SIZE(a);
  for (int i = ARR_DEPTH(a); i ; i -= W) {
    size_t index = ((pos >> i) & W_MASK);
    if (!slots[index]) {
      bool is_last_node = (index == slots_size - 1);
      size_t node_size = is_last_node ? (a->size >> i) : W_MAX;
      slots[index] = (Val)NODE_NEW(node_size);
    }
    slots = ((Node*)slots[index])->slots;
    slots_size = NODE_SIZE(slots);
  }
  size_t index = (pos & W_MASK);
  slots[index] = e;
  RETAIN(e);
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

    Val* slots = r->slots;
    // Node* node = &r->root;
    size_t index;
    for (int i = ARR_DEPTH(a); i; i -= W) {
      index = ((pos >> i) & W_MASK);
      RELEASE(slots[index]);
      slots[index] = (Val)NODE_DUP((Node*)slots[index]);
      slots = ((Node*)slots[index])->slots;
    }
    index = (pos & W_MASK);
    RELEASE(slots[index]);
    slots[index] = e;
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
  } else if (ARR_IS_PARTIAL_FULL(a)) { // covers ARR_DEPTH(a) == 0
    return ARR_DUP_APPEND(a, e);
  }
  assert(ARR_DEPTH(a));

  r = ARR_DUP(a);

  size_t pos = r->size;
  r->size++;
  Val* parent_slots = r->slots;

  // hierarchical dup slot
  int i;
  size_t index;
  size_t next_index;
  for (i = ARR_DEPTH(r); i; i -= W) {
    index = ((pos >> i) & W_MASK);
    Node* child = (Node*)parent_slots[index];
    if (NODE_INDEX_OVERFLOW(child, i - W, pos)) { // covers i - W == 0
      Val new_child = (Val)NODE_DUP_APPEND(child, i - W, e);
      RELEASE(child);
      parent_slots[index] = new_child;
      return r;
    }

    Val new_child = (Val)NODE_DUP(child);
    RELEASE(child);
    parent_slots[index] = new_child;
    parent_slots = child->slots;
  }
  NB_UNREACHABLE();
}

void _node_debug(Node* node, int depth) {
  if (depth > 0) {
    printf("<node depth=%d size=%hu extra_rc=%hu slots=[", depth, NODE_SIZE(node), ((ValHeader*)node)->extra_rc);
    for (long i = 0; i < NODE_SIZE(node); i++) {
      printf("%p, ", (void*)node->slots[i]);
    }
    printf("]\n");
    for (long i = 0; i < NODE_SIZE(node); i++) {
      _node_debug((Node*)node->slots[i], depth - W);
    }
    if (depth == W * 2) {
      printf("\n");
    }
  } else {
    printf("<leaf size=%hu extra_rc=%hu slots=[", NODE_SIZE(node), ((ValHeader*)node)->extra_rc);
    for (long i = 0; i < NODE_SIZE(node); i++) {
      printf("%lu, ", node->slots[i]);
    }
    printf("]>\n");
  }
}
