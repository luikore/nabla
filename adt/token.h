#pragma once

#include "val.h"

void nb_token_init_module();

typedef struct {
  const char* s;
  int32_t pos, size, line, col;
} NbTokenLoc;

Val nb_token_new(Val name, NbTokenLoc loc);

Val nb_token_new_c(Val name, const char* content);

Val nb_token_anew(void* arena, Val name, NbTokenLoc loc);

Val nb_token_anew_c(void* arena, Val name, const char* content);

NbTokenLoc* nb_token_loc(Val tok);
