#pragma once

// genenational memory management

#include <stdint.h>
#include <stddef.h>

struct GensStruct;
typedef struct GensStruct Gens;

Gens* nb_gens_new_gens();

void nb_gens_delete_gens(Gens* gens);

// add new gen, and return the number (doesn't select it)
int32_t nb_gens_new_gen(Gens* g);

// return max gen number
int32_t nb_gens_max_gen(Gens* g);

// return current gen number
int32_t nb_gens_get_current(Gens* g);

// set current gen number
void nb_gens_set_current(Gens* g, int32_t i);

// drop generations after current
void nb_gens_drop(Gens* g);

void nb_gens_check_memory(Gens* g);

#pragma mark ## alloc functions

void* nb_gens_malloc(Gens* g, size_t size);

void nb_gens_free(Gens* g, void* p);

// for special mutable node. prereq: rc=1
void* nb_gens_realloc(Gens* g, void* p, size_t osize, size_t nsize);
