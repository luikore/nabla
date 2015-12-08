#pragma once

// klass and method data structure definition

#include "val.h"
#include "utils/mut-array.h"

typedef struct {
  ValHeader h;
  void* code;
  ValMethodFunc func;
} ValMethod;

typedef struct {
  Val name;
  void* matcher;
} Field;

typedef struct {
  ValMethod* method;   // when NULL, use include_id
  uint32_t include_id;
} MethodSearch;

MUT_ARRAY_DECL(Fields, Field);
MUT_ARRAY_DECL(MethodSearches, MethodSearch);

// class metadata
typedef struct {
  ValHeader h; // user1: is_struct
  uint32_t id;
  uint32_t pad;
  Val name;
  Val parent_id; // parent namespace

  struct Fields fields; // struct only
  struct MethodSearches method_searches;

  ValCallbackFunc destruct_func;
  ValCallbackFunc delete_func;
  ValCallbackFunc debug_func;
  // cache hash and eq func?
} Klass;
