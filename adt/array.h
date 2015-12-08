#pragma once

// immutable array

#include "val.h"
#include <stdarg.h>

void nb_array_init_module();

Val nb_array_new_empty();

Val nb_array_new(size_t size, ...);

Val nb_array_new_v(size_t size, va_list vl);

Val nb_array_new_a(size_t size, Val* p);

size_t nb_array_size(Val v);

// pos can be negative
// return VAL_UNDEF when out of index
Val nb_array_get(Val v, int64_t pos);

// pos can be negative, if pos exceeds the size, intermediate slots are filled with nil
Val nb_array_set(Val v, int64_t pos, Val e);

Val nb_array_append(Val v, Val e);

Val nb_array_slice(Val v, uint64_t from, uint64_t len);

// pos can be negative, when pos out of range, just returns the last array
// *v = VAL_UNDEF when out of index
Val nb_array_remove(Val v, int64_t pos);

// NOTE array_each iteration impl is rather error-prone and is recursive, doesn't provide much speed-ups.
// so we stick to array_get and provide a little bit faster routine to array_map.

// for test (the suffix is size)
Val nb_array_build_test_10();
Val nb_array_build_test_37();
Val nb_array_build_test_546();
int nb_array_struct_size();
void nb_array_debug(Val v);
