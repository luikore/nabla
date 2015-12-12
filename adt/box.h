#pragma once

// for programming wrapping custom pointers

#include "val.h"

void nb_box_init_module();

Val nb_box_new(uint64_t data);
// todo box pointers with destruct function

void nb_box_set(Val v, uint64_t data);

uint64_t nb_box_get(Val v);

void nb_box_delete(Val v);

bool nb_val_is_box(Val v);
