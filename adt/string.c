#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "string.h"
#include "sym-table.h"
#include "utils/str.h"

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
static bool _eq_func(Val l, Val r);
static void _destructor(void* p);

static String* _alloc_string(size_t size);
static SSlice* _alloc_s_slice();
static Val _slice_from_literal(Val v, size_t from, size_t len);

void nb_string_init_module() {
  klass_def_internal(KLASS_STRING, val_strlit_new_c("String"));
  klass_set_hash_func(KLASS_STRING, _hash_func);
  klass_set_eq_func(KLASS_STRING, _eq_func);
  klass_set_destruct_func(KLASS_STRING, _destructor);
}

Val nb_string_new(size_t size, const char* p) {
  String* s = _alloc_string(size);
  memcpy(s->str, p, size);
  return (Val)s;
}

Val nb_string_new_literal(size_t size, const char* p) {
  return VAL_FROM_STR((uint64_t)val_strlit_new(size, p));
}

Val nb_string_new_c(const char* p) {
  return nb_string_new(strlen(p), p);
}

Val nb_string_new_literal_c(const char* p) {
  return VAL_FROM_STR((uint64_t)val_strlit_new_c(p));
}

Val nb_string_new_f(const char* template, ...) {
  va_list ap;
  va_start(ap, template);
  int sz = vsnprintf(NULL, 0, template, ap);
  char buf[sz + 1];
  vsnprintf(buf, sz + 1, template, ap);
  va_end(ap);
  return nb_string_new(sz, buf);
}

Val nb_string_new_transient(size_t size) {
  String* s = _alloc_string(size);
  return (Val)s;
}

size_t nb_string_byte_size(Val s) {
  if (VAL_IS_STR(s)) {
    return val_strlit_byte_size(VAL_TO_STR(s));
  }
  assert(!VAL_IS_IMM(s));
  return BYTE_SIZE((String*)s);
}

const char* nb_string_ptr(Val s) {
  if (VAL_IS_STR(s)) {
    return val_strlit_ptr(VAL_TO_STR(s));
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

static uint64_t _hash_func(Val v) {
  return val_hash_mem(nb_string_ptr(v), nb_string_byte_size(v));
}

static bool _eq_func(Val l, Val r) {
  if (VAL_KLASS(r) == KLASS_STRING) {
    const char* lptr = nb_string_ptr(l);
    size_t lsize = nb_string_byte_size(l);
    const char* rptr = nb_string_ptr(r);
    size_t rsize = nb_string_byte_size(r);
    return !str_compare(lsize, lptr, rsize, rptr);
  }
  return false;
}

static void _destructor(void* p) {
  String* h = p;
  if (IS_SLICE(h)) {
    SSlice* slice = p;
    RELEASE(slice->ref);
  }
}

static Val _slice_from_literal(Val v, size_t from, size_t len) {
  uint32_t sid = VAL_TO_STR(v);
  size_t lsize = val_strlit_byte_size(sid);
  if (from >= lsize) {
    return nb_string_new(0, NULL);
  }

  const char* lptr = val_strlit_ptr(sid);
  if (from + len > lsize) {
    len = lsize - from;
  }
  String* s = _alloc_string(len);
  memcpy(s, lptr + from, len);
  return (Val)s;
}

static String* _alloc_string(size_t bytesize) {
  String* m = val_alloc(KLASS_STRING, sizeof(String) + bytesize);
  // IS_SLICE(m) = 0;
  BYTE_SIZE(m) = bytesize;
  return m;
}

static SSlice* _alloc_s_slice() {
  SSlice* m = val_alloc(KLASS_STRING, sizeof(SSlice));
  IS_SLICE(m) = 1;
  // m->h.is_char_size_computed = 0;
  // m->h.bytesize = 0;
  return m;
}
