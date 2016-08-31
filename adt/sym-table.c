#include "sym-table.h"
#include "utils/mut-array.h"
#include "utils/mut-map.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t size;
  char* data;
} Str;

static uint64_t str_hash(Str s) {
  return val_hash_mem(s.data, s.size);
}

static bool str_eq(Str s1, Str s2) {
  return (s1.size == s2.size) && (0 == memcmp(s1.data, s2.data, s1.size));
}

MUT_MAP_DECL(MM, Str, uint64_t, str_hash, str_eq);

MUT_ARRAY_DECL(Strs, Str);

// todo pool alloc MM slot to improve performance

// TODO raise error when too big
#define INC(_int_v_) ((_int_v_) + 1)

struct NbSymTableStruct {
  // map of Str* -> int, in which keys are standalone on heap
  struct MM mm;

  // map of int -> Str*
  // NOTE that we can't use that values for mm's key, because when strs are resized, the address may change
  struct Strs strs;
};

// for insertion
static Str STR_NEW(size_t size, const char* p) {
  Str s = {
    .size = size,
    .data = malloc(size)
  };
  memcpy(s.data, p, size);
  return s;
}

static void STR_DELETE(Str s) {
  free(s.data);
}

// for searching
inline static Str STR_OF(size_t size, const char* p) {
  Str s = {
    .size = size,
    .data = (char*)p
  };
  return s;
}

NbSymTable* nb_sym_table_new() {
  NbSymTable* t = malloc(sizeof(NbSymTable));
  MM.init(&t->mm);
  Strs.init(&t->strs, 8);
  return t;
}

void nb_sym_table_delete(NbSymTable* t) {
  MM.cleanup(&t->mm);
  for (size_t i = 0; i < Strs.size(&t->strs); i++) {
    STR_DELETE(*Strs.at(&t->strs, i));
  }
  Strs.cleanup(&t->strs);
  free(t);
}

void nb_sym_table_get_set(NbSymTable* t, size_t ksize, const char* k, uint64_t* vid) {
  if (nb_sym_table_get(t, ksize, k, vid)) {
    return;
  }

  if (vid) {
    *vid = Strs.size(&t->strs);
  }
  Str s = STR_NEW(ksize, k);
  MM.insert(&t->mm, s, Strs.size(&t->strs));
  Strs.push(&t->strs, s);
}

bool nb_sym_table_get(NbSymTable* t, size_t ksize, const char* k, uint64_t* vid) {
  uint64_t v;
  bool res = MM.find(&t->mm, STR_OF(ksize, k), &v);
  if (vid && res) {
    *vid = v;
  }
  return res;
}

bool nb_sym_table_reverse_get(NbSymTable* t, size_t* ksize, char** k, uint64_t i) {
  if (i < Strs.size(&t->strs)) {
    Str* str = Strs.at(&t->strs, i);
    *k = str->data;
    *ksize = str->size;
    return true;
  } else {
    return false;
  }
}
