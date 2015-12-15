#pragma once

// klass and method data structure definition

#include "val.h"
#include "utils/mut-array.h"
#include "utils/mut-map.h"

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
  // todo cache hash and eq func?
} Klass;

#define KLASS_IS_STRUCT(k) (k)->h.user1
