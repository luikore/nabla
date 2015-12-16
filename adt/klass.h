#pragma once

// klass and method data structure definition

#include "val.h"
#include "utils/mut-array.h"
#include "utils/mut-map.h"

// def foo a b c      # min_argc = max_argc = 3
// def foo [a, *b]    # min_argc = 1, max_argc = -1
// def foo a b c=3    # min_argc = 2, max_argc = 3
typedef struct {
  ValHeader h; // user1: is_cfunc, user2: is_final
  uint32_t method_id;
  int32_t min_argc;
  int32_t max_argc; // -1 if not limited
  int32_t func_takes_argv;
  union {
    void* code;
    ValMethodFunc func;
    ValMethodFunc2 func2;
  } as;
} Method;

#define METHOD_ID(m) (m)->method_id
#define METHOD_MIN_ARGC(m) (m)->min_argc
#define METHOD_MAX_ARGC(m) (m)->max_argc
#define METHOD_FUNC_TAKES_ARGV(m) m->func_takes_argv
#define METHOD_IS_CFUNC(m) (m)->h.user1
#define METHOD_IS_FINAL(m) (m)->h.user2

static bool METHOD_ARGC_MATCH(Method* m, int argc) {
  if (METHOD_MAX_ARGC(m) == -1) {
    return METHOD_MIN_ARGC(m) <= argc;
  } else {
    return METHOD_MIN_ARGC(m) <= argc && METHOD_MAX_ARGC(m) >= argc;
  }
}

static Val METHOD_INVOKE(Val obj, Method* m, int argc, Val* argv) {
  if (METHOD_IS_CFUNC(m)) {
    if (!METHOD_ARGC_MATCH(m, argc)) {
      // TODO raise error
      assert(false);
    }
    if (m->func_takes_argv) {
      return m->as.func2(obj, argc, argv);
    } else {
      return val_c_call2(obj, m->as.func, argc, argv);
    }
  } else {
    // TODO bytecode method
    return VAL_UNDEF;
  }
}

static uint64_t ID_HASH(uint32_t id) {
  return val_hash_mem(&id, sizeof(uint32_t));
}

static uint64_t ID_EQ(uint32_t idl, uint32_t idr) {
  return idl == idr;
}

MUT_MAP_DECL(IdMethods, uint32_t, Method*, ID_HASH, ID_EQ);
MUT_ARRAY_DECL(Includes, uint32_t);
MUT_MAP_DECL(IdFieldIndexes, uint32_t, uint32_t, ID_HASH, ID_EQ);
MUT_ARRAY_DECL(Fields, NbStructField);

// class metadata
typedef struct {
  ValHeader h; // user1: is_struct
  uint32_t id; // index in runtime.klasses
  uint32_t parent_id; // parent namespace, 0 if no parent
  Val name;

  struct IdMethods id_methods; // {id => Method*}
  struct Includes includes; // [uint32_t]

  // struct only:
  struct IdFieldIndexes id_field_indexes; // {id => field_index}
  struct Fields fields; // [NbStructField]

  ValCallbackFunc destruct_func;
  ValCallbackFunc delete_func;
  ValCallbackFunc debug_func;
  void* data; // some klasses require custom data
  // todo cache hash and eq func?
} Klass;

#define KLASS_IS_STRUCT(k) (k)->h.user1
