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

// NOTE if we use iterator pattern, pros: good for bytecode optimization
//      cons: impl is complex, need allocation for each elem
//      for simplicity we just stick to the callback mode
typedef enum { PDLEX_MAP_INEXT, PDLEX_MAP_IBREAK, PDLEX_MAP_IFIN } NbMapIterRet;
// return ibreak/ibreak
typedef NbMapIterRet (*NbMapIterCb)(Val k, Val v, Val udata);
// return ibreak/ifin
NbMapIterRet nb_map_iter(Val m, Val udata, NbMapIterCb callback);

#pragma mark for test only

void nb_map_debug(Val m);

void nb_map_iter_debug(Val iter);

void nb_map_check_internal_structs();
