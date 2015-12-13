#pragma once

// list processing

#include "val.h"

Val nb_cons_new(Val head, Val tail);

Val nb_cons_anew(void* arena, Val head, Val tail);

Val nb_cons_reverse(Val list);

Val nb_cons_arena_reverse(void* arena, Val list);

Val nb_cons_head(Val node);

Val nb_cons_tail(Val node);
