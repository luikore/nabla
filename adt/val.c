#include "val.h"
#include "utils/mut-map.h"
#include "klass.h"
#include "string.h"
#include "sym-table.h"
#include <siphash.h>

// TODO move static global fields into vm initialization?

typedef union {
  ValHeader h;
  uint64_t i;
} HeaderCast;

typedef struct {
  uint32_t parent;
  uint32_t name_str;
} KlassSearchKey;

static uint64_t klass_search_key_hash(KlassSearchKey k) {
  return val_hash_mem(&k, sizeof(KlassSearchKey));
}

static uint64_t klass_search_key_eq(KlassSearchKey k1, KlassSearchKey k2) {
  return k1.parent == k2.parent && k1.name_str == k2.name_str;
}

MUT_ARRAY_DECL(Klasses, Klass*);
MUT_MAP_DECL(GlobalRefCounts, Val, int64_t, val_hash, val_eq);
MUT_MAP_DECL(KlassSearchMap, KlassSearchKey, uint32_t, klass_search_key_hash, klass_search_key_eq);

typedef struct {
  struct Klasses klasses;
  struct KlassSearchMap klass_search_map;
  struct GlobalRefCounts global_ref_counts;
  bool global_tracing; // for begin/end trace
  NbSymTable* literal_table;
} Runtime;

static uint8_t nb_hash_key[16];
static Runtime runtime = {
  .global_tracing = false
};

#pragma mark ### helpers

void nb_array_init_module();
void nb_dict_init_module();
void nb_map_init_module();
void nb_string_init_module();

static void _init() __attribute__((constructor(0)));
static void _init() {
  // TODO random key (and re-gen key after fork? how to fork without re-gen key?)
  for (long i = 0; i < 16; i++) {
    nb_hash_key[i] = i*i;
  }

  Klasses.init(&runtime.klasses, KLASS_USER + 10);
  GlobalRefCounts.init(&runtime.global_ref_counts);

  runtime.literal_table = nb_sym_table_new();

  nb_array_init_module();
  nb_dict_init_module();
  nb_map_init_module();
  nb_string_init_module();
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
  Klass* k = val_alloc(sizeof(Klass));
  k->h.klass = KLASS_KLASS;
  k->id = klass_id;
  k->name = name;
  k->parent_id = parent;
  Fields.init(&k->fields, 0);
  MethodSearches.init(&k->method_searches, 0);
  return k;
}

static Method* _search_method(Klass* klass, uint32_t method_id) {
  int size = MethodSearches.size(&klass->method_searches);
  for (int i = size; i >= 0; i--) {
    MethodSearch* s = MethodSearches.at(&klass->method_searches, i);
    if (s->method && s->method->method_id == method_id) {
      return s->method;
    }
  }
  return NULL;
}

#pragma mark ### klass

void klass_def_internal(uint32_t klass_id, uint32_t name_id) {
  Val name = VAL_FROM_STR(name_id);
  if (Klasses.size(&runtime.klasses) <= klass_id) {
    while (Klasses.size(&runtime.klasses) < klass_id) {
      Klasses.push(&runtime.klasses, NULL);
    }
    Klass* k = _klass_new(klass_id, name, 0);
    Klasses.push(&runtime.klasses, k);
  } else {
    Klass** k = Klasses.at(&runtime.klasses, klass_id);
    assert(*k == NULL);
    *k = _klass_new(klass_id, name, 0);
  }
}

uint32_t klass_find(Val name, uint32_t parent) {
  KlassSearchKey k = {.parent = parent, .name_str = VAL_TO_STR(name)};
  uint32_t res;
  if (KlassSearchMap.find(&runtime.klass_search_map, k, &res)) {
    return res;
  } else {
    return 0;
  }
}

uint32_t klass_ensure(Val name, uint32_t parent) {
  uint32_t res = klass_find(name, parent);
  if (res) {
    return res;
  } else {
    KlassSearchKey key = {.parent = parent, .name_str = VAL_TO_STR(name)};
    uint32_t klass_id = Klasses.size(&runtime.klasses);
    KlassSearchMap.insert(&runtime.klass_search_map, key, klass_id);

    Klass* k = _klass_new(klass_id, name, parent);
    Klasses.push(&runtime.klasses, k);
    return klass_id;
  }
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

void klass_def_fields(uint32_t klass_id, Val fields) {
  
}

// negative for arbitrary argc
void klass_def_method(uint32_t klass_id, uint32_t method_id, int argc, ValMethodFunc func, bool is_final) {
  Klass* klass = *Klasses.at(&runtime.klasses, klass_id);
  Method* meth = val_alloc(sizeof(Method));
  METHOD_IS_FINAL(meth) = is_final;
  METHOD_IS_CFUNC(meth) = true;
  METHOD_ARGC(meth) = argc;
  METHOD_ID(meth) = method_id;
  meth->as.func = func;

  MethodSearch s = {.method = meth};
  // NOTE if method was defined before, should not remove it so we can invoke `super`
  MethodSearches.push(&klass->method_searches, s);
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
  printf("debug val 0x%lx (%s)\n", v, VAL_IS_IMM(v) ? "immediate" : "pointer");
  if (VAL_IS_IMM(v)) {
    printf("  klass=%u\n", VAL_KLASS(v));
  } else {
    ValHeader* p = (ValHeader*)v;
    printf("  extra_rc=%hu, klass=%u, flags=%u\n", p->extra_rc, p->klass, p->flags);
  }
}

bool val_eq(Val l, Val r) {
  if (VAL_IS_STR(l)) {
    if (VAL_IS_STR(r)) {
      return l == r;
    }
  } else if (VAL_IS_IMM(l) && VAL_IS_IMM(r) && !VAL_IS_STR(r)) {
    return l == r;
  }
  Val res = val_send(l, val_strlit_new_c("=="), 1, &r);
  return VAL_IS_TRUE(res);
}

uint64_t val_hash(Val v) {
  if (VAL_IS_IMM(v) && !VAL_IS_STR(v)) {
    return siphash(nb_hash_key, (const uint8_t*)&v, 8);
  } else {
    Val res = val_send(v, val_strlit_new_c("hash"), 0, NULL);
    // TODO to uint 64
    return VAL_TO_INT(res);
  }
}

uint64_t val_hash_mem(const void* memory, size_t size) {
  return siphash(nb_hash_key, (const uint8_t*)memory, size);
}

Val val_send(Val obj, uint32_t method_id, int32_t argc, Val* args) {
  uint32_t klass_id = VAL_KLASS(obj);
  Klass* k = *Klasses.at(&runtime.klasses, klass_id);
  Method* m = _search_method(k, method_id);
  if (m) {
    return METHOD_INVOKE(m, argc, args);
  } else {
    return VAL_UNDEF;
  }
}

#pragma mark ### memory function interface

static uint64_t mm_hash(uint64_t k) {
  return siphash(nb_hash_key, (const uint8_t*)&k, 8);
}
static bool mm_eq(uint64_t k1, uint64_t k2) {
  return k1 == k2;
}

MUT_MAP_DECL(MM, uint64_t, uint64_t, mm_hash, mm_eq);

static struct MM heap_mem;
static bool heap_mem_checking = false;

void val_begin_check_memory_cm() {
  assert(!heap_mem_checking);
  MM.init(&heap_mem);
  heap_mem_checking = true;
}

void val_end_check_memory_cm() {
  assert(heap_mem_checking);
  if (MM.size(&heap_mem)) {
    log_err("Memory check failed, unfreed memory (%lu):", MM.size(&heap_mem));
    MMIter it;
    for (MM.iter_init(&it, &heap_mem); !MM.iter_is_end(&it); MM.iter_next(&it)) {
      log_err("  %p: klass=%u, extra_rc=%hu", (void*)it.slot->k, VAL_KLASS(it.slot->k), ((ValHeader*)it.slot->k)->extra_rc);
    }
    assert(false);
  }
  MM.cleanup(&heap_mem);
  heap_mem_checking = false;
}

static void _heap_mem_store(void* p) {
  if (heap_mem_checking) {
    assert(!VAL_IS_PERM(p));
    if (val_is_tracing()) {
      printf("heap store: %p\n", p);
      MMIter it;
      for (MM.iter_init(&it, &heap_mem); !MM.iter_is_end(&it); MM.iter_next(&it)) {
        printf("  %p\n", (void*)it.slot->k);
      }
    }
    MM.insert(&heap_mem, (uint64_t)p, 0);
  }
}

static void _heap_mem_unstore(void* p) {
  if (heap_mem_checking) {
    assert(((ValHeader*)p)->extra_rc == 0);
    uint64_t v;
    if (!MM.find(&heap_mem, (uint64_t)p, &v)) {
      log_err("Memory check failed, freeing memory not allocated: %p", p);
      assert(false);
    }
    MM.remove(&heap_mem, (uint64_t)p);
  }
}

void val_begin_trace() {
  runtime.global_tracing = true;
}

bool val_is_tracing() {
  return runtime.global_tracing;
}

void val_end_trace() {
  runtime.global_tracing = false;
}

void* val_alloc_cm(size_t size) {
  ValHeader* p = malloc(size);
  memset(p, 0, size);

  _heap_mem_store(p);
  return p;
}

void* val_alloc_f(size_t size) {
  // malloc is already aligned at least 8 bytes
  // see http://en.cppreference.com/w/c/types/max_align_t
  ValHeader* p = malloc(size);
  memset(p, 0, size);

  return p;
}

void* val_dup_cm(void* p, size_t osize, size_t nsize) {
  ValHeader* r = malloc(nsize);

  if (nsize > osize) {
    memcpy(r, p, osize);
    memset((char*)r + osize, 0, nsize - osize);
  } else {
    memcpy(r, p, nsize);
  }
  _clear_rc(r);

  _heap_mem_store(r);
  return r;
}

void* val_dup_f(void* p, size_t osize, size_t nsize) {
  ValHeader* r = malloc(nsize);

  if (nsize > osize) {
    memcpy(r, p, osize);
    memset((char*)r + osize, 0, nsize - osize);
  } else {
    memcpy(r, p, nsize);
  }
  _clear_rc(r);

  return r;
}

void* val_realloc_cm(void* p, size_t osize, size_t nsize) {
  assert(nsize > osize);

  _heap_mem_unstore(p);

  p = realloc(p, nsize);
  memset((char*)p + osize, 0, nsize - osize);

  _heap_mem_store(p);
  return p;
}

void* val_realloc_f(void* p, size_t osize, size_t nsize) {
  assert(nsize > osize);

  p = realloc(p, nsize);
  memset((char*)p + osize, 0, nsize - osize);

  return p;
}

void val_free_cm(void* _p) {
  ValHeader* p = _p;
  assert(p->extra_rc == 0);

  _heap_mem_unstore(p);
  free(p);
}

void val_free_f(void* _p) {
  ValHeader* p = _p;
  assert(p->extra_rc == 0);

  free(p);
}

void val_perm_cm(void* _p) {
  ValHeader* p = _p;
  p->perm = true;
  _heap_mem_unstore(p);
}

void val_perm_f(void* _p) {
  ValHeader* p = _p;
  p->perm = true;
}

// same as val_retain
void val_retain_cm(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  _inc_ref_count(v);
}

void val_retain_f(Val v) {
  val_retain_cm(v);
}

void val_release_cm(Val v) {
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
      val_free_cm(p);
    }
  } else {
    _dec_ref_count(v);
  }
}

void val_release_f(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  if (p->klass == 6) {
    printf("release node xtra_rc=%d\n", p->extra_rc);
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
