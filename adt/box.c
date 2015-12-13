#include "box.h"
#include <assert.h>

typedef struct {
  ValHeader header;
  uint64_t data;
} Box;

static Val _box_eq(Val l, Val r) {
  if (VAL_KLASS(r) == KLASS_BOX) {
    return (nb_box_get(l) == nb_box_get(r)) ? VAL_TRUE : VAL_FALSE;
  } else {
    return VAL_FALSE;
  }
}

static Val _box_hash(Val b) {
  uint64_t v = nb_box_get(b);
  v ^= KLASS_BOX_SALT;
  uint64_t h = val_hash_mem(&v, sizeof(uint64_t));
  // TODO val from uint64
  return VAL_FROM_INT(h);
}

void nb_box_init_module() {
  klass_def_internal(KLASS_BOX, val_strlit_new_c("Box"));
  klass_def_method(KLASS_BOX, val_strlit_new_c("=="), 1, _box_eq, true);
  klass_def_method(KLASS_BOX, val_strlit_new_c("hash"), 0, _box_hash, true);
}

Val nb_box_new(uint64_t data) {
  Box* box = val_alloc(KLASS_BOX, sizeof(Box));
  box->data = data;
  return (Val)box;
}

void nb_box_set(Val v, uint64_t data) {
  Box* box = (Box*)v;
  assert(box->header.klass == KLASS_BOX);
  box->data = data;
}

uint64_t nb_box_get(Val v) {
  Box* box = (Box*)v;
  assert(box->header.klass == KLASS_BOX);
  return box->data;
}

void nb_box_delete(Val v) {
  Box* box = (Box*)v;
  assert(box->header.klass == KLASS_BOX);
  val_free(box);
}

bool nb_val_is_box(Val v) {
  return !VAL_IS_IMM(v) && ((Box*)v)->header.klass == KLASS_BOX;
}
