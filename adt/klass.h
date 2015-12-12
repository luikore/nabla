#pragma once

// klass and method data structure definition

#include "val.h"
#include "utils/mut-array.h"

typedef struct {
  ValHeader h; // user1: is_cfunc, user2: is_final
  uint32_t method_id;
  int32_t argc;
  union {
    void* code;
    ValMethodFunc func;
  } as;
} Method;

#define METHOD_ID(m) (m)->method_id
#define METHOD_ARGC(m) (m)->argc
#define METHOD_IS_CFUNC(m) (m)->h.user1
#define METHOD_IS_FINAL(m) (m)->h.user2

static bool METHOD_ARGC_MATCH(Method* m, int argc) {
  int m_argc = METHOD_ARGC(m);
  if (m_argc == argc) {
    return true;
  }
  if (m_argc < 0) {
    return argc >= (-m_argc);
  }
  return false;
}

static Val METHOD_INVOKE(Val obj, Method* m, int argc, Val* argv) {
  if (METHOD_IS_CFUNC(m)) {
    if (!METHOD_ARGC_MATCH(m, argc)) {
      // TODO raise error
      assert(false);
    }
    return val_c_call2(obj, m->as.func, argc, argv);
  } else {
    // TODO
    return VAL_UNDEF;
  }
}

typedef struct {
  Val name;
  void* matcher;
} Field;

typedef struct {
  Method* method;   // when NULL, use include_id
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
  Val parent_id; // parent namespace, 0 if no parent

  struct Fields fields; // struct only
  struct MethodSearches method_searches;

  ValCallbackFunc destruct_func;
  ValCallbackFunc delete_func;
  ValCallbackFunc debug_func;
  // cache hash and eq func?
} Klass;

#define KLASS_IS_STRUCT(k) (k)->h.user1
