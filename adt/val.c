#include "val.h"
#include "utils/mut-map.h"
#include <siphash.h>

// TODO move static global fields into vm initialization?

#ifdef CHECK_MEMORY
#undef val_begin_check_memory
#undef val_end_check_memory
#undef val_alloc
#undef val_dup
#undef val_realloc
#undef val_free
#undef val_perm
#undef val_retain
#undef val_release
#endif

typedef union {
  ValHeader h;
  uint64_t i;
} HeaderCast;

typedef struct {
  ValHashFunc hash_func;
  ValDestructorFunc destructor_func;
  ValDeleteFunc delete_func;
} FuncMap;

static bool eq_func(Val v1, Val v2) {
  return VAL_EQ(v1, v2);
}

MUT_MAP_DECL(GlobalRefCounts, Val, int64_t, val_hash, eq_func);

static uint8_t nb_hash_key[16];
static FuncMap* func_map;
static size_t func_map_size = 100;
static uint32_t curr_klass_id = KLASS_USER;
static bool global_tracing = false; // for begin/end trace
static struct GlobalRefCounts global_ref_counts;

static void _init() __attribute__((constructor(0)));
static void _init() {
  // TODO random key (and re-gen key after fork? how to fork without re-gen key?)
  for (long i = 0; i < 16; i++) {
    nb_hash_key[i] = i*i;
  }
  func_map = malloc(sizeof(FuncMap) * func_map_size);
  memset(func_map, 0, sizeof(FuncMap) * func_map_size);
  GlobalRefCounts.init(&global_ref_counts);
}

static void _inc_ref_count(Val v) {
  ValHeader* p = (ValHeader*)v;

  if (p->rc_overflow) {
    int64_t ref_count;
    if (GlobalRefCounts.find(&global_ref_counts, v, &ref_count)) {
      GlobalRefCounts.insert(&global_ref_counts, v, ref_count + 1);
    } else {
      // todo error
    }
  } else {
    uint64_t* up = (uint64_t*)p;
    up[0]++;
    if (p->rc_overflow) {
      int64_t ref_count;
      if (GlobalRefCounts.find(&global_ref_counts, v, &ref_count)) {
        // todo error
      } else {
        GlobalRefCounts.insert(&global_ref_counts, v, VAL_MAX_EMBED_RC);
      }
    }
  }
}

static void _dec_ref_count(Val v) {
  ValHeader* p = (ValHeader*)v;

  if (p->rc_overflow) {
    int64_t ref_count;
    if (GlobalRefCounts.find(&global_ref_counts, v, &ref_count)) {
      if (ref_count == VAL_MAX_EMBED_RC) {
        GlobalRefCounts.remove(&global_ref_counts, v);
        p->rc_overflow = false;
        p->extra_rc = (VAL_MAX_EMBED_RC - 1);
      } else {
        GlobalRefCounts.insert(&global_ref_counts, v, ref_count - 1);
      }
    } else {
      // todo error
    }
  } else {
    p->extra_rc--;
  }
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

uint64_t val_hash(Val v) {
  if (VAL_IS_IMM(v) && !VAL_IS_STR(v)) {
    return siphash(nb_hash_key, (const uint8_t*)&v, 8);
  } else {
    uint32_t klass = VAL_KLASS(v);
    assert(klass <= curr_klass_id);
    ValHashFunc func = func_map[klass].hash_func;
    if (func) {
      return func(v);
    }
    return 0; // todo
  }
}

uint64_t val_hash_mem(const void* memory, size_t size) {
  return siphash(nb_hash_key, (const uint8_t*)memory, size);
}

uint32_t val_new_class_id() {
  if (curr_klass_id + 2 > func_map_size) {
    func_map = realloc(func_map, sizeof(FuncMap) * func_map_size * 2);
    memset(func_map + func_map_size, 0, sizeof(FuncMap) * func_map_size);
    func_map_size *= 2;
  }
  return ++curr_klass_id;
}

void val_register_hash_func(uint32_t klass, ValHashFunc hash_func) {
  assert(klass <= curr_klass_id);
  func_map[klass].hash_func = hash_func;
}

void val_register_destructor_func(uint32_t klass, ValDestructorFunc destructor_func) {
  assert(klass <= curr_klass_id);
  if (func_map[klass].delete_func) {
    log_warn("delete_func is registered on klass %u, this destructor will not be invoked", klass);
  }
  func_map[klass].destructor_func = destructor_func;
}

void val_register_delete_func(uint32_t klass, ValDeleteFunc delete_func) {
  assert(klass <= curr_klass_id);
  if (func_map[klass].destructor_func) {
    log_warn("destructor registered on klass %u will not be invoked", klass);
  }
  func_map[klass].delete_func = delete_func;
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
  global_tracing = true;
}

bool val_is_tracing() {
  return global_tracing;
}

void val_end_trace() {
  global_tracing = false;
}

void* val_alloc_cm(size_t size) {
  ValHeader* p = malloc(size);
  memset(p, 0, size);

  _heap_mem_store(p);
  return p;
}

void* val_alloc(size_t size) {
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
  r->extra_rc = 0;

  _heap_mem_store(r);
  return r;
}

void* val_dup(void* p, size_t osize, size_t nsize) {
  ValHeader* r = malloc(nsize);

  if (nsize > osize) {
    memcpy(r, p, osize);
    memset((char*)r + osize, 0, nsize - osize);
  } else {
    memcpy(r, p, nsize);
  }
  r->extra_rc = 0;

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

void* val_realloc(void* p, size_t osize, size_t nsize) {
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

void val_free(void* _p) {
  ValHeader* p = _p;
  assert(p->extra_rc == 0);

  free(p);
}

void val_perm_cm(void* _p) {
  ValHeader* p = _p;
  p->perm = true;
  _heap_mem_unstore(p);
}

void val_perm(void* _p) {
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

void val_retain(Val v) {
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

  if (p->extra_rc == 0) {
    FuncMap fm = func_map[p->klass];
    if (fm.delete_func) {
      fm.delete_func(p);
    } else {
      if (fm.destructor_func) {
        fm.destructor_func(p);
      }
      val_free_cm(p);
    }
  } else {
    _dec_ref_count(v);
  }
}

void val_release(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  if (p->extra_rc == 0) {
    FuncMap fm = func_map[p->klass];
    if (fm.delete_func) {
      fm.delete_func(p);
    } else {
      if (fm.destructor_func) {
        fm.destructor_func(p);
      }
      val_free(p);
    }
  } else {
    _dec_ref_count(v);
  }
}

int64_t val_global_ref_count(Val v) {
  int64_t res;
  bool found = GlobalRefCounts.find(&global_ref_counts, v, &res);
  return found ? res : -1;
}
