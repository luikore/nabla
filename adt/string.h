#pragma once

#include "val.h"

void nb_string_init_module();

Val nb_string_new(size_t size, const char* p);

// put into a permanent symbol table
Val nb_string_new_literal(size_t size, const char* p) __attribute__((const));

Val nb_string_new_c(const char* p);

Val nb_string_new_literal_c(const char* p) __attribute__((const));

#define nb_string_new_f(template, ...) ({\
  char __buf[snprintf(NULL, 0, template, __VA_ARGS__)];\
  snprintf(__buf, sizeof(__buf)/sizeof(char), template, __VA_ARGS__);\
  nb_string_new(sizeof(__buf)/sizeof(char), __buf);\
})

Val nb_string_new_transient(size_t size);

size_t nb_string_byte_size(Val s);

const char* nb_string_ptr(Val s);

Val nb_string_concat(Val s1, Val s2);

int nb_string_cmp(Val s1, Val s2);

// todo negative index
Val nb_string_slice(Val s, size_t from, size_t len);
