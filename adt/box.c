#include "box.h"
#include <assert.h>

typedef struct {
  ValHeader header;
  uint64_t data;
} Box;

Val nb_box_new(uint64_t data) {
  Box* box = val_alloc(sizeof(Box));
  box->header.klass = KLASS_BOX;
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
