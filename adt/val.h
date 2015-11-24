#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
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
static inline bool VAL_IS_IMM(Val v) {
  return (v & 7) || !(v & ~VAL_FALSE);
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
#define VAL_FROM_DBL(_dbl_) PDLEX_ROTL(VAL_DBL_AS_UINT(_dbl_), 3)
#define VAL_TO_DBL(_v_) VAL_UINT_AS_DBL(PDLEX_ROTR((_v_), 3))

static inline uint64_t VAL_INT_AS_UINT(int64_t i) {
  ValCast c = {.i = i};
  return c.u;
}
static inline int64_t VAL_UINT_AS_INT(uint64_t u) {
  ValCast c = {.u = u};
  return c.i;
}
#define VAL_INT_MAX ((1LL << 62) - 1)
#define VAL_INT_MIN (-1LL << 62)
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

// TODO more compressed header:

//       MSB
// 24bit extra_rc    (rc - 1) - don't need to extract when changing rc
// 1bit  has_dtor     (for faster dealloc)
// 1bit  rc_overflow  (counted in global table)
// 1bit  rc_perm      (permanent)
// 1bit  deallocating (it is being deallocated)
// 1bit  has_assoc    (has associated memory, for extensions)
// 1bit  user1
// 1bit  user2
// 1bit  user3
//       LSB
// 32bit klass

// klass can be combined with 32bit method id for the method search? (think about the HAMT method table)
typedef struct {
  uint64_t ref_count;
  uint32_t klass;
  uint32_t flags;
} ValHeader;

#define VAL_REF_COUNT_MAX (~(uint64_t)0)
#define VAL_IS_PERM(_p_) (((ValHeader*)(_p_))->ref_count == VAL_REF_COUNT_MAX)

enum {
  KLASS_NIL,
  KLASS_BOOLEAN,
  KLASS_INTEGER,
  KLASS_RATIONAL,
  KLASS_DOUBLE,
  KLASS_COMPLEX,
  KLASS_VECTOR,
  KLASS_RANGE,
  KLASS_STRING,
  KLASS_ARRAY,
  KLASS_MAP,
  KLASS_MAP_ITER,
  KLASS_DICT,
  KLASS_BOX,

  KLASS_USER // start of dynamic allocated classes
};

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

inline static uint64_t VAL_REF_COUNT(Val v) {
  return ((ValHeader*)(v))->ref_count;
}

#pragma mark ### misc

Val val_c_call(void* cfunc, uint64_t argc, Val* argv);

void val_debug(Val v);

uint64_t val_hash_mem(const void* memory, size_t size);

uint64_t val_hash(Val v);

uint32_t val_new_class_id();

typedef uint64_t (*ValHashFunc)(Val obj);
void val_register_hash_func(uint32_t klass, ValHashFunc hash_func);

// todo: content comparison for real equality
#define VAL_EQ(_v1_, _v2_) (_v1_ == _v2_)

// numeric layout:
// {
//   ValHeader header;
//   dbl / mp_int / mp_q / mp_complex / decimal
// }

// vector layout: (for more complex linear algebra data structures, let third party implement them)
// {
//    ValHeader header; // elem type in flags
//    Val / dbl / mp_int / mp_q / mp_complex / decimal
// }

//
// typedef struct {
//   ValHeader header; // user_flags = ivar size
//   Val ivars[];
// } ValData;

#pragma mark ### memory functions

// global tracing flag, for debug use
// usage: if (val_is_tracing()) { ... print some verbose info }
void val_begin_trace();
bool val_is_tracing();
void val_end_trace();

// NOTE for memory check to work when linking other libs to libadt, we need to static dispatch the functions in header
#ifdef CHECK_MEMORY

void val_begin_check_memory_cm();
void val_end_check_memory_cm();
void* val_alloc_cm(size_t size);
void* val_dup_cm(void* p, size_t osize, size_t nsize);
void* val_realloc_cm(void* p, size_t osize, size_t nsize);
void val_free_cm(void* p);
void val_perm_cm(void* p);
void val_retain_cm(Val p);
void val_release_cm(Val p);

#define val_begin_check_memory val_begin_check_memory_cm
#define val_end_check_memory val_end_check_memory_cm
#define val_alloc val_alloc_cm
#define val_dup val_dup_cm
#define val_realloc val_realloc_cm
#define val_free val_free_cm
#define val_perm val_perm_cm
#define val_retain val_retain_cm
#define val_release val_release_cm

#else

#define val_begin_check_memory()
#define val_end_check_memory()
void* val_alloc(size_t size);
void* val_dup(void* p, size_t osize, size_t nsize);
// mainly used for transient in-place updates
void* val_realloc(void* p, size_t osize, size_t nsize);
void val_free(void* p);
void val_perm(void* p);
void val_retain(Val p);
// if ref_count on pointer val == 1, free it, else decrease it. no effect on immediate vals
void val_release(Val p);

#endif // CHECK_MEMORY

// register destructor before val_free() is called on the object of the klass.
// to avoid memory leak, a standard implementaion is to call val_release on all Val members.
typedef void (*ValDestructorFunc)(void*);
void val_register_destructor_func(uint32_t klass, ValDestructorFunc func);

// replace the standard destructor() -> val_free() for objects of the klass.
typedef void (*ValDeleteFunc)(void*);
void val_register_delete_func(uint32_t klass, ValDeleteFunc func);

// for convenient in-place-update
#define REPLACE(_obj_, _expr_) do {\
  Val _tmp_ = (_expr_);\
  val_release(_obj_);\
  (_obj_) = _tmp_;\
} while (0)

#define RETAIN(_obj_) val_retain((Val)(_obj_))
#define RELEASE(_obj_) val_release((Val)(_obj_))
