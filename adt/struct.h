#pragma once

// struct define and use

// a struct can also have a proc (stored in metadata)
// so a method can be represented as a struct

#include "val.h"

void* nb_struct_def(uint32_t klass_id);

// create a new struct with the given klass id
void nb_struct_def_fields(void* st, uint32_t field_size, NbStructField* fields);

// set default proc to the struct, making it a lambda
typedef Val (*NbStructProc)(Val instance);
void nb_struct_def_proc(void* st, NbStructProc proc);

void* nb_struct_find(uint32_t klass_id);

// example: nb_struct_new(klass_find(nb_string_new_literal_c("Foo"), 0), n, attrs);
Val nb_struct_new(uint32_t klass_id, uint32_t argc, Val* argv);

// new empty {struct, field_size} for bytecode building, field_size is unboxed int
ValPair nb_struct_new_empty(uint32_t klass_id);

// invoke the lambda / method
// returns {res, error}
ValPair nb_struct_call(Val instance);

// get field index by name, -1 if not found
int32_t nb_struct_field_i(uint32_t klass_id, uint32_t field_id);

// returns field_value
Val nb_struct_get(Val instance, uint32_t i);

// returns new st
Val nb_struct_set(Val instance, uint32_t i, Val field_value);

// mutable set field
void nb_struct_mset(Val instance, uint32_t i, Val field_value);
