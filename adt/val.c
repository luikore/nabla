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

typedef struct {
  ValHashFunc hash_func;
  ValDestructorFunc destructor_func;
  ValDeleteFunc delete_func;
} FuncMap;

static uint8_t nb_hash_key[16];
static FuncMap* func_map;
static size_t func_map_size = 100;
static uint32_t curr_klass_id = KLASS_USER;
static bool global_tracing = false; // for begin/end trace

static void _init() __attribute__((constructor(0)));
static void _init() {
  // TODO random key (and re-gen key after fork? how to fork without re-gen key?)
  for (long i = 0; i < 16; i++) {
    nb_hash_key[i] = i*i;
  }
  func_map = malloc(sizeof(FuncMap) * func_map_size);
  memset(func_map, 0, sizeof(FuncMap) * func_map_size);
}

void val_debug(Val v) {
  printf("debug val 0x%lx (%s)\n", v, VAL_IS_IMM(v) ? "immediate" : "pointer");
  if (VAL_IS_IMM(v)) {
    printf("  klass=%u\n", VAL_KLASS(v));
  } else {
    ValHeader* p = (ValHeader*)v;
    printf("  ref_count=%llu, klass=%u, flags=%u\n", p->ref_count, p->klass, p->flags);
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
      log_err("  %p: klass=%u, ref_count=%llu", (void*)it.slot->k, VAL_KLASS(it.slot->k), ((ValHeader*)it.slot->k)->ref_count);
    }
    assert(false);
  }
  MM.cleanup(&heap_mem);
  heap_mem_checking = false;
}

static void _heap_mem_store(void* p) {
  if (heap_mem_checking) {
    assert(((ValHeader*)p)->ref_count != VAL_REF_COUNT_MAX);
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
    assert(((ValHeader*)p)->ref_count <= 1);
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
  p->ref_count = 1;

  _heap_mem_store(p);
  return p;
}

void* val_alloc(size_t size) {
  // malloc is already aligned at least 8 bytes
  // see http://en.cppreference.com/w/c/types/max_align_t
  ValHeader* p = malloc(size);
  memset(p, 0, size);
  p->ref_count = 1;

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
  r->ref_count = 1;

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
  r->ref_count = 1;

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
  assert(p->ref_count <= 1);

  _heap_mem_unstore(p);
  free(p);
}

void val_free(void* _p) {
  ValHeader* p = _p;
  assert(p->ref_count <= 1);

  free(p);
}

void val_perm_cm(void* _p) {
  ValHeader* p = _p;
  p->ref_count = VAL_REF_COUNT_MAX;
  _heap_mem_unstore(p);
}

void val_perm(void* _p) {
  ValHeader* p = _p;
  p->ref_count = VAL_REF_COUNT_MAX;
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
  p->ref_count++;
}

void val_retain(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }
  p->ref_count++;
}

void val_release_cm(Val v) {
  if (VAL_IS_IMM(v)) {
    return;
  }
  ValHeader* p = (ValHeader*)v;
  if (VAL_IS_PERM(p)) {
    return;
  }

  if (p->ref_count == 1) {
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
    p->ref_count--;
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

  if (p->ref_count == 1) {
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
    p->ref_count--;
  }
}
