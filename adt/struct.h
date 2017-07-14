#pragma once

// struct class define and use

#include "val.h"

// create a new struct klass, return the klass id
// set parent_id=0 for top level struct
uint32_t nb_struct_def(Val name, uint32_t parent_id, uint32_t field_size, NbStructField* fields);

// example: nb_struct_new(klass_find(nb_string_new_literal_c("Foo"), 0), n, attrs);
Val nb_struct_new(uint32_t klass_id, uint32_t argc, Val* argv);

// new empty {struct, field_size} for bytecode building, field_size is unboxed int
ValPair nb_struct_new_empty(uint32_t klass_id);

// -1 if not found
int32_t nb_struct_field_i(uint32_t klass_id, uint32_t field_id);

// returns field_value
Val nb_struct_get(Val st, uint32_t i);

// returns new st
Val nb_struct_set(Val st, uint32_t i, Val field_value);

// mutable set field
void nb_struct_mset(Val st, uint32_t i, Val field_value);
