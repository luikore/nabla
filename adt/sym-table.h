#pragma once

// mutable bi-directional table of string <-> id, for internal use only
// id is incremental and start from 0

#include "val.h"

struct NbSymTableStruct;
typedef struct NbSymTableStruct NbSymTable;

NbSymTable* nb_sym_table_new();

void nb_sym_table_delete(NbSymTable* st);

// if k not in table, insert and return a new table
void nb_sym_table_get_set(NbSymTable* st, size_t ksize, const char* k, uint64_t* vid);

// true if found, false if not found
bool nb_sym_table_get(NbSymTable* st, size_t ksize, const char* k, uint64_t* vid);

// true if found, false if not found
bool nb_sym_table_reverse_get(NbSymTable* st, size_t* ksize, char** k, uint64_t vid);
