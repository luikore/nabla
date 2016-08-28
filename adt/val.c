#include "val.h"
#include "utils/mut-map.h"
#include "utils/arena.h"
#include "klass.h"
#include "string.h"
#include "sym-table.h"
#include "gens.h"
#include <siphash.h>
#include <execinfo.h>
#include <signal.h>

// TODO move static global fields into vm initialization?

typedef union {
  ValHeader h;
  uint64_t i;
} HeaderCast;

typedef struct {
  uint32_t parent;
  uint32_t name_str;
} ConstSearchKey;

static uint64_t _const_search_key_hash(ConstSearchKey k) {
  return val_hash_mem(&k, sizeof(ConstSearchKey));
}

static uint64_t _const_search_key_eq(ConstSearchKey k1, ConstSearchKey k2) {
  return k1.parent == k2.parent && k1.name_str == k2.name_str;
}

MUT_ARRAY_DECL(Klasses, Klass*);
MUT_MAP_DECL(GlobalRefCounts, Val, int64_t, val_hash, val_eq);
MUT_MAP_DECL(KlassSearchMap, ConstSearchKey, uint32_t, _const_search_key_hash, _const_search_key_eq);
MUT_MAP_DECL(ConstSearchMap, ConstSearchKey, Val, _const_search_key_hash, _const_search_key_eq);
MUT_ARRAY_DECL(Allocators, void*);

typedef struct {
  struct Klasses klasses; // array index by klass_id
  struct KlassSearchMap klass_search_map; // { (parent, name_str_lit) => klass* }
  struct ConstSearchMap const_search_map; // { (parent, name_str_lit) => Val }
  struct GlobalRefCounts global_ref_counts;
  bool global_tracing; // for begin/end trace
  NbSymTable* literal_table;
} Runtime;

static uint8_t nb_hash_key[16];
static Runtime runtime = {
  .global_tracing = false
};

// thread local runtime
typedef struct {
  Gens* gens;
} TLRuntime;

static __thread TLRuntime tl_runtime;

#pragma mark ### helpers

void nb_box_init_module();
void nb_array_init_module();
void nb_dict_init_module();
void nb_map_init_module();
void nb_string_init_module();
void nb_cons_init_module();
void nb_token_init_module();

static void _init() __attribute__((constructor(0)));
static void _init() {
  // TODO random key (and re-gen key after fork? how to fork without re-gen key?)
  for (long i = 0; i < 16; i++) {
    nb_hash_key[i] = i*i;
  }

  tl_runtime.gens = nb_gens_new_gens();

  Klasses.init(&runtime.klasses, KLASS_USER + 10);
  KlassSearchMap.init(&runtime.klass_search_map);
  GlobalRefCounts.init(&runtime.global_ref_counts);
  ConstSearchMap.init(&runtime.const_search_map);

  runtime.literal_table = nb_sym_table_new();

  for (int i = 0; i < KLASS_USER; i++) {
    Klasses.push(&runtime.klasses, NULL);
  }

  nb_box_init_module();
  nb_array_init_module();
  nb_dict_init_module();
  nb_map_init_module();
  nb_string_init_module();
  nb_cons_init_module();
  nb_token_init_module();
}

static void _inc_ref_count(Val v) {
  ValHeader* p = (ValHeader*)v;

  if (p->rc_overflow) {
    int64_t ref_count;
    if (GlobalRefCounts.find(&runtime.global_ref_counts, v, &ref_count)) {
      GlobalRefCounts.insert(&runtime.global_ref_counts, v, ref_count + 1);
    } else {
      // todo error
    }
  } else {
    uint64_t* up = (uint64_t*)p;
    up[0]++;
    if (p->rc_overflow) {
      int64_t ref_count;
      if (GlobalRefCounts.find(&runtime.global_ref_counts, v, &ref_count)) {
        // todo error
      } else {
        GlobalRefCounts.insert(&runtime.global_ref_counts, v, VAL_MAX_EMBED_RC);
      }
    }
  }
}

static void _dec_ref_count(Val v) {
  ValHeader* p = (ValHeader*)v;

  if (p->rc_overflow) {
    int64_t ref_count;
    if (GlobalRefCounts.find(&runtime.global_ref_counts, v, &ref_count)) {
      if (ref_count == VAL_MAX_EMBED_RC) {
        GlobalRefCounts.remove(&runtime.global_ref_counts, v);
        p->rc_overflow = false;
        p->extra_rc = (VAL_MAX_EMBED_RC - 1);
      } else {
        GlobalRefCounts.insert(&runtime.global_ref_counts, v, ref_count - 1);
      }
    } else {
      // todo error
    }
  } else {
    p->extra_rc--;
  }
}

static void _clear_rc(ValHeader* r) {
  r->extra_rc = 0;
  r->rc_overflow = false;
  r->perm = false;
}

static Klass* _klass_new(uint32_t klass_id, Val name, uint32_t parent) {
  Klass* k = val_alloc(KLASS_KLASS, sizeof(Klass));
  k->id = klass_id;
  k->name = name;
  k->parent_id = parent;

  IdMethods.init(&k->id_methods);
  Includes.init(&k->includes, 0);
  IdFieldIndexes.init(&k->id_field_indexes);
  Fields.init(&k->fields, 0);

  k->hash_func = NULL;
  k->eq_func = NULL;
  k->destruct_func = NULL;
  k->delete_func = NULL;
  k->debug_func = NULL;
  val_perm(k);

  ConstSearchKey key = {.parent = parent, .name_str = VAL_TO_STR(name)};
  KlassSearchMap.insert(&runtime.klass_search_map, key, klass_id);
  ConstSearchMap.insert(&runtime.const_search_map, key, (Val)k);

  return k;
}

static Method* _method_new(uint32_t method_id, int32_t min_argc, int32_t max_argc, bool is_final) {
  Method* meth = val_alloc(KLASS_METHOD, sizeof(Method));
  METHOD_ID(meth) = method_id;
  METHOD_IS_FINAL(meth) = is_final;
  METHOD_MIN_ARGC(meth) = min_argc;
  METHOD_MAX_ARGC(meth) = max_argc;
  val_perm(meth);
  return meth;
}

static Method* _search_own_method(Klass* klass, uint32_t method_id) {
  Method* method;
  if (IdMethods.find(&klass->id_methods, method_id, &method)) {
    return method;
  } else {
    return NULL;
  }
}

static void _check_final_method_conflict(Klass* klass, uint32_t method_id) {
  Method* prev_meth = _search_own_method(klass, method_id);
  if (prev_meth) {
    if (METHOD_IS_FINAL(prev_meth)) {
      assert(false);
      // TODO raise error
    }
  }
}

#pragma mark ### klass

void klass_def_internal(uint32_t klass_id, uint32_t name_id) {
  Val name = VAL_FROM_STR(name_id);
  Klass** k = Klasses.at(&runtime.klasses, klass_id);
  assert(*k == NULL);
  *k = _klass_new(klass_id, name, 0);
}

uint32_t klass_find(Val name, uint32_t parent) {
  ConstSearchKey k = {.parent = parent, .name_str = VAL_TO_STR(name)};
  uint32_t res;
  if (KlassSearchMap.find(&runtime.klass_search_map, k, &res)) {
    return res;
  } else {
    return 0;
  }
}

uint32_t klass_find_c(const char* name, uint32_t parent) {
  return klass_find(nb_string_new_literal_c(name), parent);
}

uint32_t klass_ensure(Val name, uint32_t parent) {
  uint32_t res = klass_find(name, parent);
  if (res) {
    return res;
  } else {
    uint32_t klass_id = Klasses.size(&runtime.klasses);
    Klass* k = _klass_new(klass_id, name, parent);
    Klasses.push(&runtime.klasses, k);
    return klass_id;
  }
}

Val klass_val(uint32_t klass_id) {
  // todo throw if not klass
  return (Val)(*Klasses.at(&runtime.klasses, klass_id));
}

Val klass_name(uint32_t klass_id) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  return klass->name;
}

void klass_set_unsafe(uint32_t klass_id) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  KLASS_IS_UNSAFE(klass) = true;
}

void klass_set_data(uint32_t klass_id, void* data) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  klass->data = data;
}

void* klass_get_data(uint32_t klass_id) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  return klass->data;
}

void klass_set_destruct_func(uint32_t klass_id, ValCallbackFunc func) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  assert(klass);
  klass->destruct_func = func;
}

void klass_set_delete_func(uint32_t klass_id, ValCallbackFunc func) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  assert(klass);
  klass->delete_func = func;
}

void klass_set_debug_func(uint32_t klass_id, ValCallbackFunc func) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  assert(klass);
  klass->debug_func = func;
}

void klass_set_hash_func(uint32_t klass_id, ValHashFunc func) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  assert(klass);
  klass->hash_func = func;
}

void klass_set_eq_func(uint32_t klass_id, ValEqFunc func) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  assert(klass);
  klass->eq_func = func;
}

void klass_def_method(uint32_t klass_id, uint32_t method_id, int32_t argc, ValMethodFunc func, bool is_final) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  _check_final_method_conflict(klass, method_id);

  Method* meth = _method_new(method_id, argc, argc, is_final);
  METHOD_IS_CFUNC(meth) = true;
  METHOD_FUNC_TAKES_ARGV(meth) = false;
  meth->as.func = func;

  IdMethods.insert(&klass->id_methods, method_id, meth);

}

void klass_def_method_v(uint32_t klass_id, uint32_t method_id, int32_t min_argc, int32_t max_argc, ValMethodFuncV func, bool is_final) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  _check_final_method_conflict(klass, method_id);

  Method* meth = _method_new(method_id, min_argc, max_argc, is_final);
  METHOD_IS_CFUNC(meth) = true;
  METHOD_FUNC_TAKES_ARGV(meth) = true;
  meth->as.func2 = func;

  IdMethods.insert(&klass->id_methods, method_id, meth);
}

void* klass_find_method(uint32_t klass_id, uint32_t method_id) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  Method* m = _search_own_method(klass, method_id);
  if (m) {
    return m;
  }

  int size = Includes.size(&klass->includes);
  for (int i = size - 1; i >= 0; i--) {
    uint32_t include_id = *Includes.at(&klass->includes, i);
    Method *m = klass_find_method(include_id, method_id);
    if (m) {
      return m;
    }
  }
  return NULL;
}

ValPair klass_call_method(Val obj, void* m, int argc, Val* argv) {
  if (m) {
    return METHOD_INVOKE(obj, m, argc, argv);
  } else {
    // TODO error class hierachy
    Val no_method_err = nb_string_new_literal_c("method not found");
    return (ValPair){VAL_NIL, no_method_err};
  }
}

void klass_include(uint32_t klass_id, uint32_t included_id) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  Klass* included_klass = *Klasses.at(&runtime.klasses, included_id);
  if (KLASS_IS_UNSAFE(included_klass)) {
    val_throw(nb_string_new_literal_c("can not include unsafe klass!"));
  }

  int size = Includes.size(&klass->includes);
  for (int i = size - 1; i >= 0; i--) {
    if (included_id == *Includes.at(&klass->includes, i)) {
      Includes.remove(&klass->includes, i);
      break;
    }
  }
  Includes.push(&klass->includes, included_id);
}

void klass_debug() {
  int size = Klasses.size(&runtime.klasses);
  printf("--- klasses (user=%d) ---\n", KLASS_USER);
  for (int i = 0; i < size; i++) {
    Klass* k = *Klasses.at(&runtime.klasses, i);
    if (k) {
      printf("index:%d, id:%d, name:%.*s, is_struct:%d, is_unsafe:%d\n",
      i, k->id, (int)nb_string_ptr(k->name), nb_string_ptr(k->name), (int)KLASS_IS_STRUCT(k), (int)KLASS_IS_UNSAFE(k));
      ConstSearchKey key = {.parent = k->parent_id, .name_str = VAL_TO_STR(k->name)};
      uint32_t res;
      assert(KlassSearchMap.find(&runtime.klass_search_map, key, &res));
    } else {
      printf("index:%d, -not-defined-\n", i);
    }
  }
  printf("\n");
}

#pragma mark ### misc func

uint32_t val_strlit_new(size_t size, const char* s) {
  uint64_t sid;
  nb_sym_table_get_set(runtime.literal_table, size, s, &sid);
  return (uint32_t)sid;
}

uint32_t val_strlit_new_c(const char* s) {
  return val_strlit_new(strlen(s), s);
}

const char* val_strlit_ptr(uint32_t sid) {
  size_t size;
  char* s;
  bool found = nb_sym_table_reverse_get(runtime.literal_table, &size, &s, sid);
  assert(found);
  return s;
}

size_t val_strlit_byte_size(uint32_t sid) {
  size_t size;
  char* s;
  bool found = nb_sym_table_reverse_get(runtime.literal_table, &size, &s, sid);
  assert(found);
  return size;
}

void val_debug(Val v) {
  printf("debug val 0x%lx (%s)", v, VAL_IS_IMM(v) ? "immediate" : "pointer");
  if (v == VAL_UNDEF) {
    printf(" undef\n");
    return;
  } else {
    uint32_t klass_id = VAL_KLASS(v);
    Klass* klass_ptr = *Klasses.at(&runtime.klasses, klass_id);
    if (klass_ptr) {
      uint32_t str = VAL_TO_STR(klass_ptr->name);
      printf(" klass=%.*s,", (int)val_strlit_byte_size(str), val_strlit_ptr(str));
    } else {
      printf(" klass=0,");
    }
  }

  if (!VAL_IS_IMM(v)) {
    printf("\n");
  } else {
    ValHeader* p = (ValHeader*)v;
    printf(" extra_rc=%hu, flags=%u, user1=%u, user2=%u\n",
    p->extra_rc, p->flags, p->user1, p->user2);
  }
}

bool val_eq(Val l, Val r) {
  if (l == r) {
    return true;
  }
  if (VAL_IS_IMM(l) && VAL_IS_IMM(r)) {
    return false;
  }

  uint32_t klass_id = VAL_KLASS(l);
  Klass* k = *Klasses.at(&runtime.klasses, klass_id);
  if (k->eq_func) {
    return k->eq_func(l, r);
  }

  ValPair res = val_send(l, val_strlit_new_c("=="), 1, &r);
  if (res.snd) {
    val_throw(res.snd); // error: raise in eq function
  }
  return VAL_IS_TRUE(res.fst);
}

uint64_t val_hash(Val v) {
  if (VAL_IS_IMM(v) && !VAL_IS_STR(v)) {
    return siphash(nb_hash_key, (const uint8_t*)&v, 8);
  } else {
    uint32_t klass_id = VAL_KLASS(v);
    Klass* k = *Klasses.at(&runtime.klasses, klass_id);
    if (k->hash_func) {
      return k->hash_func(v);
    }

    ValPair res = val_send(v, val_strlit_new_c("hash"), 0, NULL);
    if (res.snd) {
      val_throw(res.snd); // error: raise in hash function
    }
    // TODO to uint 64
    return VAL_TO_INT(res.fst);
  }
}

uint64_t val_hash_mem(const void* memory, size_t size) {
  return siphash(nb_hash_key, (const uint8_t*)memory, size);
}

ValPair val_send(Val obj, uint32_t method_id, int32_t argc, Val* args) {
  uint32_t klass_id = VAL_KLASS(obj);
  Method* m = klass_find_method(klass_id, method_id);
  return klass_call_method(obj, m, argc, args);
}

noreturn void val_throw(Val obj) {
  // todo
  if (VAL_KLASS(obj) == KLASS_STRING) {
    fprintf(stderr, "%.*s\n", (int)nb_string_byte_size(obj), nb_string_ptr(obj));
  }
  _Exit(-2);
}

void val_def_const(uint32_t namespace, uint32_t name_str, Val v) {
  Klass* k = (Klass*)klass_val(namespace);
  ConstSearchKey key = {.parent = namespace, .name_str = name_str};
  Val existed;
  if (ConstSearchMap.find(&runtime.const_search_map, key, &existed)) {
    val_throw(nb_string_new_literal_c("const already defined")); // todo error types
  } else {
    if (!VAL_IS_IMM(v)) {
      val_perm((void*)v);
    }
    ConstSearchMap.insert(&runtime.const_search_map, key, v);
  }
}

#pragma mark ### memory function interface

void val_begin_check_memory() {
  nb_gens_set_current(tl_runtime.gens, -1);
}

void val_end_check_memory() {
  assert(nb_gens_get_current(tl_runtime.gens) == -1);
  nb_gens_check_memory(tl_runtime.gens);
  nb_gens_set_current(tl_runtime.gens, 0);
}

void* val_alloc(uint32_t klass_id, size_t size) {
  ValHeader* p = nb_gens_malloc(tl_runtime.gens, size);
  memset(p, 0, size);
  p->klass = klass_id;

  return p;
}

void* val_dup(void* p, size_t osize, size_t nsize) {
  ValHeader* r = nb_gens_malloc(tl_runtime.gens, nsize);

  if (nsize > osize) {
    memcpy(r, p, osize);
    memset((char*)r + osize, 0, nsize - osize);
  } else {
    memcpy(r, p, nsize);
  }
  _clear_rc(r);

  return r;
}

void* val_realloc(void* p, size_t osize, size_t nsize) {
  assert(nsize > osize);

  p = nb_gens_realloc(tl_runtime.gens, p, osize, nsize);
  return p;
}

void val_free(void* _p) {
  ValHeader* p = _p;
  assert(p->extra_rc == 0);

  nb_gens_free(tl_runtime.gens, p);
}

void val_perm(void* _p) {
  ValHeader* p = _p;
  p->perm = true;
}

// same as val_retain
void val_retain(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  _inc_ref_count(v);
}

void val_release(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  if (!p->rc_overflow && p->extra_rc == 0) {
    uint32_t klass_id = VAL_KLASS((Val)p);
    Klass* k = *Klasses.at(&runtime.klasses, klass_id);
    if (k->delete_func) {
      k->delete_func(p);
    } else {
      if (k->destruct_func) {
        k->destruct_func(p);
      }
      val_free(p);
    }
  } else {
    _dec_ref_count(v);
  }
}

int64_t val_global_ref_count(Val v) {
  int64_t res;
  bool found = GlobalRefCounts.find(&runtime.global_ref_counts, v, &res);
  return found ? res : -1;
}

#pragma mark ### gens control (just delegates gens)

int32_t val_gens_new_gen() {
  return nb_gens_new_gen(tl_runtime.gens);
}

int32_t val_gens_max_gen() {
  return nb_gens_max_gen(tl_runtime.gens);
}

int32_t val_gens_get_current() {
  return nb_gens_get_current(tl_runtime.gens);
}

void val_gens_set_current(int32_t i) {
  nb_gens_set_current(tl_runtime.gens, i);
}

void val_gens_drop() {
  nb_gens_drop(tl_runtime.gens);
}

#pragma mark ### trace

void val_begin_trace() {
  runtime.global_tracing = true;
}

bool val_is_tracing() {
  return runtime.global_tracing;
}

void val_end_trace() {
  runtime.global_tracing = false;
}

static const char* _program_name;
static void _print_trace(int n) {
  void* callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);

  printf("\n");
  for (i = 0; i < frames; ++i) {
    printf("%s\n", strs[i]);

// not working and too slow:
// https://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
//     char cmd[256];
// #   ifdef __APPLE__
//     sprintf(cmd, "atos -o %.256s %p", _program_name, callstack[i]);
// #   else
//     sprintf(cmd, "addr2line -f -p -e %.256s %p", _program_name, callstack[i]);
// #   endif
//     system(cmd);
  }

  free(strs);
}

void val_trap_backtrace(const char* program_name) {
  _program_name = program_name;
  signal(SIGABRT, _print_trace);
}
