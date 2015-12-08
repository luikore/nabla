#include <stdlib.h>
#include <string.h>
#include "string.h"
#include "sym-table.h"

typedef struct {
  ValHeader h; // flags:encoding, user1:is_slice
  uint64_t byte_size;
  Val ref;
  uint64_t offset;
} SSlice;

typedef struct {
  ValHeader h; // flags:encoding, user1:is_slice
  uint64_t byte_size;
  char str[];
} String;

#define ENC(s) (s)->h.flags
#define IS_SLICE(s) (s)->h.user1
#define BYTE_SIZE(s) (s)->byte_size

static uint64_t _hash_func(Val str);
static void _destructor(void* p);

static NbSymTable* literal_table;

static String* _alloc_string(size_t size);
static SSlice* _alloc_s_slice();
static Val _slice_from_literal(Val v, size_t from, size_t len);

void nb_string_init_module() {
  literal_table = nb_sym_table_new();
  val_register_hash_func(KLASS_STRING, _hash_func);
  val_register_destructor_func(KLASS_STRING, _destructor);
}

Val nb_string_new(size_t size, const char* p) {
  String* s = _alloc_string(size);
  memcpy(s->str, p, size);
  return (Val)s;
}

Val nb_string_new_literal(size_t size, const char* p) {
  uint64_t sid;
  nb_sym_table_get_set(literal_table, size, p, &sid);
  return VAL_FROM_STR(sid);
}

Val nb_string_new_c(const char* p) {
  return nb_string_new(strlen(p), p);
}

Val nb_string_new_literal_c(const char* p) {
  return nb_string_new_literal(strlen(p), p);
}

Val nb_string_new_transient(size_t size) {
  String* s = _alloc_string(size);
  return (Val)s;
}

size_t nb_string_byte_size(Val s) {
  if (VAL_IS_STR(s)) {
    size_t size;
    char* ptr;
    nb_sym_table_reverse_get(literal_table, &size, &ptr, VAL_TO_STR(s));
    return size;
  }
  assert(!VAL_IS_IMM(s));
  return BYTE_SIZE((String*)s);
}

const char* nb_string_ptr(Val s) {
  if (VAL_IS_STR(s)) {
    size_t size;
    char* ptr;
    bool res = nb_sym_table_reverse_get(literal_table, &size, &ptr, VAL_TO_STR(s));
    assert(res);
    return ptr;
  }
  assert(!VAL_IS_IMM(s));
  Val ref;
  if (IS_SLICE((String*)s)) {
    ref = ((SSlice*)s)->ref;
  } else {
    ref = s;
  }
  return ((String*)ref)->str;
}

Val nb_string_concat(Val s1, Val s2) {
  size_t bytesize1 = nb_string_byte_size(s1);
  size_t bytesize2 = nb_string_byte_size(s2);
  String* r = _alloc_string(bytesize1 + bytesize2);
  memcpy(r->str, nb_string_ptr(s1), bytesize1);
  memcpy(r->str + bytesize1, nb_string_ptr(s2), bytesize2);
  return (Val)r;
}

bool nb_string_eql(Val s1, Val s2) {
  if (s1 == s2) { // fast path for same ref and literals
    return true;
  } else {
    return (nb_string_cmp(s1, s2) == 0);
  }
}

int nb_string_cmp(Val s1, Val s2) {
  const char* p1 = nb_string_ptr(s1);
  size_t l1 = nb_string_byte_size(s1);
  const char* p2 = nb_string_ptr(s2);
  size_t l2 = nb_string_byte_size(s2);
  int res = strncmp(p1, p2, l1 > l2 ? l2 : l1);
  if (res == 0) {
    return l1 < l2 ? 1 : l1 > l2 ? -1 : 0;
  } else {
    return res;
  }
}

Val nb_string_slice(Val v, size_t from, size_t len) {
  if (VAL_IS_STR(v)) {
    return _slice_from_literal(v, from, len);
  }

  String* h = (String*)v;
  SSlice* r = _alloc_s_slice();
  if (IS_SLICE(h)) {
    SSlice* s = (SSlice*)v;
    r->ref = s->ref;
    if (from > BYTE_SIZE(s)) {
      // todo use static empty string
      BYTE_SIZE(r) = 0;
      r->offset = 0;
    } else if (len > BYTE_SIZE(s) - from) {
      BYTE_SIZE(r) = BYTE_SIZE(s) - from;
      r->offset = s->offset + from;
    } else {
      BYTE_SIZE(r) = len;
      r->offset = s->offset + from;
    }
  } else {
    String* s = (String*)v;
    r->ref = v;
    if (from > BYTE_SIZE(s)) {
      // todo use static empty string
      BYTE_SIZE(r) = 0;
      r->offset = 0;
    } else if (len > BYTE_SIZE(s) - from) {
      BYTE_SIZE(r) = BYTE_SIZE(s) - from;
      r->offset = from;
    } else {
      BYTE_SIZE(r) = len;
      r->offset = from;
    }
  }
  RETAIN(r->ref);
  return (Val)r;
}

bool nb_string_literal_lookup(Val v, size_t* size, char** p) {
  uint64_t sid = VAL_TO_STR(v);
  return nb_sym_table_reverse_get(literal_table, size, p, sid);
}

static uint64_t _hash_func(Val v) {
  return val_hash_mem(nb_string_ptr(v), nb_string_byte_size(v));
}

static void _destructor(void* p) {
  String* h = p;
  if (IS_SLICE(h)) {
    SSlice* slice = p;
    RELEASE(slice->ref);
  }
}

static Val _slice_from_literal(Val v, size_t from, size_t len) {
  size_t lsize;
  char* lptr;
  if (nb_string_literal_lookup(v, &lsize, &lptr)) {
    if (from >= lsize) {
      return nb_string_new(0, NULL);
    }
    if (from + len > lsize) {
      len = lsize - from;
    }
    String* s = _alloc_string(len);
    memcpy(s, lptr + from, len);
    return (Val)s;
  } else {
    // todo error
    return VAL_NIL;
  }
}

static String* _alloc_string(size_t bytesize) {
  String* m = val_alloc(sizeof(String) + bytesize);
  m->h.klass = KLASS_STRING;
  // IS_SLICE(m) = 0;
  BYTE_SIZE(m) = bytesize;
  return m;
}

static SSlice* _alloc_s_slice() {
  SSlice* m = val_alloc(sizeof(SSlice));
  m->h.klass = KLASS_STRING;
  IS_SLICE(m) = 1;
  // m->h.is_char_size_computed = 0;
  // m->h.bytesize = 0;
  return m;
}
