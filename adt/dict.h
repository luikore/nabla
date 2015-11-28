#pragma once

// memory-efficient data structure for string keys
// keys are ordered by binary lexcical ascendence (todo iter)
// inserted values must be valid Val

#include "val.h"

Val nb_dict_new();

Val nb_dict_new_with_root(Val root, int64_t size);

size_t nb_dict_size(Val dict);

// true: found, false: not found
bool nb_dict_find(Val dict, const char* k, size_t ksize, Val* v);

// NOTE: must manual retain v before insert
Val nb_dict_insert(Val dict, const char* k, size_t ksize, Val v);

// true: removed and value stored in v, false: not removed
bool nb_dict_remove(Val dict, const char* k, size_t ksize, Val* v);

void nb_dict_debug(Val h, bool bucket_as_binary);

// TODO dict each
