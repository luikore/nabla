#include "struct.h"
#include "klass.h"
#include "string.h"

typedef struct {
  ValHeader h;
  Val fields[];
} Struct;

#define STRUCT_BYTE_SIZE(argc) (sizeof(ValHeader) + sizeof(Val) * (argc))

static void _struct_destruct(void* ptr) {
  Struct* st = ptr;
  Klass* k = (Klass*)klass_val(st->h.klass);
  int attr_size = Fields.size(&k->fields);
  for (int i = 0; i < attr_size; i++) {
    RELEASE(st->fields[i]);
  }
}

uint32_t nb_struct_def(Val name, uint32_t parent_id, uint32_t field_size, NbStructField* fields) {
  uint32_t klass_id = klass_ensure(name, parent_id);
  Klass* k = (Klass*)klass_val(klass_id);
  for (int i = 0; i < field_size; i++) {
    Fields.push(&k->fields, fields[i]);
  }
  klass_set_destruct_func(klass_id, _struct_destruct);
  return klass_id;
}

Val nb_struct_new(uint32_t klass_id, uint32_t argc, Val* argv) {
  // todo splat matcher logic
  Klass* k = (Klass*)klass_val(klass_id);
  assert(k);
  int field_size = Fields.size(&k->fields);
  if (field_size != argc) {
    // todo var-length match
    val_throw(nb_string_new_literal_c("field size mismatch"));
  }

  Struct* s = val_alloc(klass_id, STRUCT_BYTE_SIZE(argc));
  memcpy(s->fields, argv, argc * sizeof(Val));
  return (Val)s;
}

Val nb_struct_anew(void* arena, uint32_t klass_id, uint32_t argc, Val* argv) {
  // todo splat matcher logic
  Klass* k = (Klass*)klass_val(klass_id);
  assert(k);
  int field_size = Fields.size(&k->fields);
  if (field_size != argc) {
    // todo var-length match
    val_throw(nb_string_new_literal_c("field size mismatch"));
  }

  Struct* s = val_arena_alloc(arena, klass_id, field_size);
  memcpy(s->fields, argv, argc * sizeof(Val));
  return (Val)s;
}

int32_t nb_struct_field_i(uint32_t klass_id, uint32_t field_id) {
  Klass* k = (Klass*)klass_val(klass_id);
  uint32_t index;
  if (IdFieldIndexes.find(&k->id_field_indexes, field_id, &index)) {
    return index;
  } else {
    return -1;
  }
}

Val nb_struct_get(Val st, uint32_t i) {
  return ((Struct*)st)->fields[i];
}

Val nb_struct_set(Val st, uint32_t i, Val field_value) {
  // todo
  return st;
}

// mutable set field
void nb_struct_mset(Val st, uint32_t i, Val field_value) {
  ((Struct*)st)->fields[i] = field_value;
}
