#pragma once

#include "val.h"

void nb_token_init_module();

typedef struct {
  const char* s;
  int32_t pos, size, line, col;
  Val v;
} NbTokenLoc;

Val nb_token_new(Val name, NbTokenLoc loc);

Val nb_token_new_c(Val name, const char* content, Val v);

NbTokenLoc* nb_token_loc(Val tok);

Val nb_token_to_s(Val tok);
