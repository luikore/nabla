#pragma once

// all keys should be Val, but value type should be uint64_t or Val

#include "val.h"

Val nb_map_new();

// values are integer, and won't be retained/released
// NOTE we need to be careful that values do not conflict with VAL_UNDEF
Val nb_map_new_i();

size_t nb_map_size(Val m);

Val nb_map_insert(Val m, Val k, Val v);

// return VAL_UNDEF if elem not exist
Val nb_map_find(Val m, Val k);

// *v = VAL_UNDEF if elem not exist
Val nb_map_remove(Val m, Val k, Val* v);

// NOTE not use iterator pattern
//      iterator pattern
//        pros: good for bytecode optimization
//        cons: impl is complex, need allocation for each elem
//      for simplicity we just stick to the callback style
typedef enum { NB_MAP_NEXT, NB_MAP_BREAK, NB_MAP_FIN } NbMapEachRet;
// return ibreak/ibreak
typedef NbMapEachRet (*NbMapEachCb)(Val k, Val v, Val udata);
// return ibreak/ifin
NbMapEachRet nb_map_each(Val m, Val udata, NbMapEachCb callback);

#pragma mark for test only

void nb_map_debug(Val m);
