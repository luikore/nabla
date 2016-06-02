#pragma once

// list processing

#include "val.h"

void nb_cons_init_module();

Val nb_cons_new(Val head, Val tail);

Val nb_cons_new_rev(Val init, Val last);

Val nb_cons_reverse(Val list);

Val nb_cons_head(Val node);

Val nb_cons_tail(Val node);

// [arg1, arg2, arg3] -> (arg1 : arg2 : arg3 : nil)
Val nb_cons_list(int32_t argc, Val* argv);
