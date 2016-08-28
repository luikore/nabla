#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include "utils/intrinsics.h"
#include "utils/dbg.h"

#pragma mark ### tagged pointer for pdlex and nabla VM

typedef uintptr_t Val;
typedef union {
  uint64_t u;
  double d;
  int64_t i;
} ValCast;

/*

lowest 8 bits in the tagged pointer

    ...xxxx xxx1 int
    ...xxxx xx10 dbl
    ...0000 1100 str (also entries in sym table, limited to 32 bits)
    ...0000 0000 nil   0x00 = 0
    ...0000 1000 false 0x08 = 8
    ...0001 0100 true  0x14 = 20
    ...xxxx x000 pointer (satisfies truth test and 000 mask)

special values for internal use only

    ...0011 0100 undef 0x34 = 52
    ...0111 0100 user1 0x74 = 116
    ...1111 0100 user2 0xF4 = 244

*/

#define VAL_NIL   ((Val)0)
#define VAL_FALSE ((Val)8)
#define VAL_TRUE  ((Val)0x14)
#define VAL_UNDEF ((Val)0x34)
#define VAL_USER1 ((Val)0x74)
#define VAL_USER2 ((Val)0xF4)

#define VAL_IS_TRUE(_v_) ((_v_) & ~VAL_FALSE)
#define VAL_IS_FALSE(_v_) (!VAL_IS_TRUE(_v_))

// immediate value: int, dbl, true, str, nil, false
static inline bool VAL_IS_IMM(Val v) {
  return (v & 7) || VAL_IS_FALSE(v);
}

static inline uint64_t VAL_DBL_AS_UINT(double d) {
  ValCast c = {.d = d};
  return c.u;
}
static inline double VAL_UINT_AS_DBL(uint64_t u) {
  ValCast c = {.u = u};
  return c.d;
}
#define VAL_DBL_CAN_IMM(_dbl_) (VAL_DBL_AS_UINT(_dbl_) & (1ULL << 62))
#define VAL_IS_DBL(_v_) (((_v_) & 3) == 2)
#define VAL_FROM_DBL(_dbl_) NB_ROTL(VAL_DBL_AS_UINT(_dbl_), 3)
#define VAL_TO_DBL(_v_) VAL_UINT_AS_DBL(NB_ROTR((_v_), 3))

static inline uint64_t VAL_INT_AS_UINT(int64_t i) {
  ValCast c = {.i = i};
  return c.u;
}
static inline int64_t VAL_UINT_AS_INT(uint64_t u) {
  ValCast c = {.u = u};
  return c.i;
}
#define VAL_INT_MAX ((1LL << 62) - 1)
#define VAL_INT_MIN (-(1LL << 62))
#define VAL_INT_CAN_IMM(_i_) ((_i_) < VAL_INT_MAX && (_i_) > VAL_INT_MIN)
#define VAL_IS_INT(_v_) ((_v_) & 1)
#define VAL_FROM_INT(_int_) ((VAL_INT_AS_UINT(_int_) << 1) | 1)
// NOTE we need arithmetic shift to restore negative values
#define VAL_TO_INT(_v_) (VAL_UINT_AS_INT(_v_) >> 1)

// TODO rename STR -> SLIT
#define VAL_IS_STR(_v_) (((_v_) & 0x0f) == 0x0c)
// NOTE we use the whole 64 bit value as hash keys... etc.
//      so when generating, the id is added by (1 << 32) for each new value
#define VAL_FROM_STR(_sid_) (((uint64_t)(_sid_) << 32) | 0x0c)
#define VAL_TO_STR(_v_) ((Val)(_v_) >> 32)

#pragma mark ### object

// klass can be combined with 32bit method id for the method search? (think about the HAMT method table)
typedef struct {
  // little endian
  uint16_t extra_rc: 12; // rc - 1 (if highest bit is set)
  bool rc_overflow: 1;   // we inc extra_rc until this bit is set
  bool perm: 1;          // is permanent (or managed by custom allocator)
  bool has_dtor: 1;
  bool deallocating: 1;  // we are inside the object's deallocation procedure

  bool has_assoc: 1;     // extended with asociated memory, stored in global table
  bool user1: 1;
  bool user2: 1;
  bool user3: 1;
  uint16_t flags: 12;    // 12 bits available, can be used as counters, etc...

  uint32_t klass;
} ValHeader;

#define VAL_MAX_EMBED_RC (1<<12)
#define VAL_IS_PERM(_p_) (((ValHeader*)(_p_))->perm)

enum {
  KLASS_NIL=1,
  KLASS_BOOLEAN,
  KLASS_INTEGER,
  KLASS_DOUBLE,
  KLASS_RANGE,
  KLASS_STRING,

  KLASS_METHOD,
  KLASS_LAMBDA,
  KLASS_SUBROUTINE,
  KLASS_KLASS,

  KLASS_ARRAY_NODE,
  KLASS_ARRAY,

  KLASS_MAP_NODE,
  KLASS_MAP_COLA,
  KLASS_MAP,

  KLASS_DICT_MAP,
  KLASS_DICT_BUCKET,
  KLASS_DICT,

  KLASS_RATIONAL,
  KLASS_COMPLEX,
  KLASS_VECTOR,

  KLASS_CONS,
  KLASS_TOKEN,
  KLASS_BOX,

  KLASS_USER // start of dynamic allocated classes
};

// some salts for hashing
#define KLASS_BOX_SALT 0x01030507090A0CULL
#define KLASS_TOKEN_SALT 0x9370ULL
#define KLASS_CONS_SALT 0x29450A0CULL

inline static uint32_t VAL_KLASS(Val v) {
  if (VAL_IS_IMM(v)) {
    switch (v) {
      case VAL_NIL: return KLASS_NIL;
      case VAL_TRUE:
      case VAL_FALSE: return KLASS_BOOLEAN;
    }
    if (VAL_IS_INT(v)) {
      return KLASS_INTEGER;
    }
    if (VAL_IS_DBL(v)) {
      return KLASS_DOUBLE;
    }
    if (VAL_IS_STR(v)) {
      return KLASS_STRING;
    }
    debug("IMM v of unkown klass: %lu", v);
    assert(false);
    return VAL_NIL;
  } else {
    return ((ValHeader*)(v))->klass;
  }
}

int64_t val_global_ref_count(Val v);

inline static int64_t VAL_REF_COUNT(Val v) {
  if (VAL_IS_IMM(v)) {
    return -1;
  }
  ValHeader* h = (ValHeader*)v;
  if (h->rc_overflow) {
    return val_global_ref_count(v);
  } else if (h->perm) {
    return -1;
  } else {
    return h->extra_rc + 1;
  }
}

#pragma mark ### misc

typedef struct {
  Val fst;
  Val snd;
} ValPair;

// returns res, res.fst is result, res.snd is error thrown
ValPair val_c_call(void* cfunc, uint64_t argc, Val* argv);

// call with obj as the first arg
ValPair val_c_call2(Val obj, void* cfunc, uint64_t argc, Val* argv);

uint64_t val_hash_mem(const void* memory, size_t size);

uint64_t val_hash(Val v);

bool val_eq(Val l, Val r);

void val_debug(Val v);

ValPair val_send(Val obj, uint32_t id, int32_t argc, Val* args);

uint32_t val_strlit_new(size_t size, const char* s);

uint32_t val_strlit_new_c(const char* s);

size_t val_strlit_byte_size(uint32_t l);

const char* val_strlit_ptr(uint32_t l);

noreturn void val_throw(Val obj);

void val_def_const(uint32_t namespace, uint32_t name_str, Val v);

#define FATAL(msg, ...) ({ fprintf(stderr, (msg), ##__VA_ARGS__); _Exit(-1); })
#define ASSERT(expr, msg, ...) if (!(expr)) { VAL_FATAL((msg), ##__VA_ARGS__) }

#pragma mark ### method and klass

typedef void (*ValCallbackFunc)(void*);

#ifndef ANYARGS
# ifdef __cplusplus
#   define ANYARGS ...
# else
#   define ANYARGS
# endif
#endif
typedef ValPair (*ValMethodFunc)(ANYARGS);
typedef ValPair (*ValMethodFuncV)(Val, int32_t, Val*);

typedef uint64_t (*ValHashFunc)(Val);
typedef bool (*ValEqFunc)(Val, Val);

// define klass_id < KLASS_USER
// todo do not expose this func
void klass_def_internal(uint32_t klass_id, uint32_t name_id);

// return 0 if not exist
uint32_t klass_find(Val name, uint32_t parent_id);

uint32_t klass_find_c(const char* name, uint32_t parent_id);

// create if not exist
uint32_t klass_ensure(Val name, uint32_t parent_id);

Val klass_val(uint32_t klass_id);

Val klass_name(uint32_t klass_id);

// to avoid extension methods segfault
// klass with unsafe methods should be tagged to avoid being included
void klass_set_unsafe(uint32_t klass_id);

// assoc data to klass
void klass_set_data(uint32_t klass_id, void* data);
void* klass_get_data(uint32_t klass_id);

// register destructor before val_free() is called on the object of the klass.
// to avoid memory leak, a standard implementaion is to call val_release on all Val members.
void klass_set_destruct_func(uint32_t klass_id, ValCallbackFunc func);

// replace the standard destructor() -> val_free() for objects of the klass.
void klass_set_delete_func(uint32_t klass_id, ValCallbackFunc func);

void klass_set_debug_func(uint32_t klass_id, ValCallbackFunc func);

void klass_set_hash_func(uint32_t klass_id, ValHashFunc func);

void klass_set_eq_func(uint32_t klass_id, ValEqFunc func);

// for fixed argc
void klass_def_method(uint32_t klass_id, uint32_t method_id, int32_t argc, ValMethodFunc func, bool is_final);

// for fixed or variadic argc
void klass_def_method_v(uint32_t klass_id, uint32_t method_id, int32_t min_argc, int32_t max_argc, ValMethodFuncV func, bool is_final);

// TODO expose shallow search?
// Method* klass_find_own_method(uint32_t klass_id, uint32_t method_id);

// deep search
void* klass_find_method(uint32_t klass_id, uint32_t method_id);

// klass_find_method -> klass_call_method provides more low level control than val_send
ValPair klass_call_method(Val obj, void* m, int argc, Val* argv);

void klass_include(uint32_t klass_id, uint32_t included_id);

typedef struct {
  Val matcher; // VAL_UNDEF if not defined
  uint32_t field_id;
  bool is_splat;
} NbStructField;

// list all klasses
void klass_debug();

#pragma mark ### memory functions

void val_begin_check_memory();
void val_end_check_memory();
void* val_alloc(uint32_t klass_id, size_t size);
void* val_dup(void* p, size_t osize, size_t nsize);
void* val_realloc(void* p, size_t osize, size_t nsize);
void val_free(void* p);
void val_perm(void* p);
void val_retain(Val p);
void val_release(Val p);

// for convenient in-place-update
#define AS_VAL(_obj_) *((Val*)(&(_obj_)))
#define REPLACE(_obj_, _expr_) do {\
  Val _tmp_ = (_expr_);\
  val_release(_obj_);\
  (_obj_) = _tmp_;\
} while (0)

#define RETAIN(_obj_) val_retain((Val)(_obj_))
#define RELEASE(_obj_) val_release((Val)(_obj_))

#pragma mark ### gens control (just delegates gens)

// create new generation (but not select it)
int32_t val_gens_new_gen();

// return max gen number
int32_t val_gens_max_gen();

// return current gen number
int32_t val_gens_get_current();

// set current gen number
void val_gens_set_current(int32_t i);

// drop generations after current
void val_gens_drop();

#pragma mark ### trace function

// global tracing flag, for debug use
// usage: if (val_is_tracing()) { ... print some verbose info }
void val_begin_trace();
bool val_is_tracing();
void val_end_trace();

// display backtrace on assertion failure
void val_trap_backtrace(const char* program_name);
