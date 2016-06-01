#include "gens.h"
#include "utils/arena.h"
#include "utils/mut-array.h"
#include "utils/mut-map.h"
#include "val.h"
#include <assert.h>
#include <siphash.h>
#include <stdint.h>

// generational memory management
// TODO atomicity for concurrency

static uint8_t hash_key[16];
static void _init() __attribute__((constructor(0)));
static void _init() {
  for (long i = 0; i < 16; i++) {
    hash_key[i] = i*i + 5;
  }
}

MUT_ARRAY_DECL(Arenas, Arena);

static uint64_t mm_hash(uint64_t k) {
  return siphash(hash_key, (const uint8_t*)&k, 8);
}

static bool mm_eq(uint64_t k1, uint64_t k2) {
  return k1 == k2;
}

MUT_MAP_DECL(MM, uint64_t, uint64_t, mm_hash, mm_eq);

struct GensStruct {
  struct Arenas arenas;
  int current; // -1 for checked memory, 0 for normal heap
  struct MM checked_memory_map;
};

static void _heap_mem_insert(Gens* gens, void* p) {
  assert(!VAL_IS_PERM(p));
  MM.insert(&gens->checked_memory_map, (uint64_t)p, 0);
  if (val_is_tracing()) {
    printf("heap store: %p, current heap:\n", p);
    MMIter it;
    for (MM.iter_init(&it, &gens->checked_memory_map); !MM.iter_is_end(&it); MM.iter_next(&it)) {
      printf("  %p\n", (void*)it.slot->k);
    }
  }
}

static void _heap_mem_remove(Gens* gens, void* p) {
  assert(((ValHeader*)p)->extra_rc == 0);
  uint64_t v;
  if (!MM.find(&gens->checked_memory_map, (uint64_t)p, &v)) {
    log_err("Memory check failed, freeing memory not allocated: %p", p);
    assert(false);
  }
  MM.remove(&gens->checked_memory_map, (uint64_t)p);
  if (val_is_tracing()) {
    printf("heap remove: %p, current heap:\n", p);
    MMIter it;
    for (MM.iter_init(&it, &gens->checked_memory_map); !MM.iter_is_end(&it); MM.iter_next(&it)) {
      printf("  %p\n", (void*)it.slot->k);
    }
  }
}

Gens* nb_gens_new_gens() {
  Gens* g = malloc(sizeof(Gens));
  Arenas.init(&g->arenas, 4);

  Arena stub_arena;
  Arenas.push(&g->arenas, stub_arena); // arenas[0] is not used

  MM.init(&g->checked_memory_map);

  g->current = 0;
  return g;
}

void nb_gens_delete_gens(Gens* g) {
  // skip 0 which doesn't require free
  for (int i = 1; i < Arenas.size(&g->arenas); i++) {
    Arena* arena = Arenas.at(&g->arenas, i);
    arena_cleanup(arena);
  }
  Arenas.cleanup(&g->arenas);
  MM.cleanup(&g->checked_memory_map);
  free(g);
}

void* nb_gens_malloc(Gens* g, size_t size) {
  if (g->current == 0) {
    return malloc(size);
  } else if (g->current > 0) {
    Arena* arena = Arenas.at(&g->arenas, g->current);
    int qword_count = (size + 7) / 8;
    return arena_slot_alloc(arena, qword_count);
  } else {
    void* data = malloc(size);
    _heap_mem_insert(g, data);
    return data;
  }
}

void nb_gens_free(Gens* g, void* p) {
  if (g->current == 0) {
    free(p);
  } else if (g->current > 0) {
    // TODO mark object as freed
  } else {
    _heap_mem_remove(g, p);
    free(p);
  }
}

void* nb_gens_realloc(Gens* g, void* p, size_t osize, size_t nsize) {
  void* new_p;
  if (g->current == 0) {
    new_p = realloc(p, nsize);
  } else if (g->current > 0) {
    new_p = nb_gens_malloc(g, nsize);
    if (new_p != p) {
      memcpy(new_p, p, osize);
      nb_gens_free(g, p);
    }
  } else {
    new_p = realloc(p, nsize);
    _heap_mem_remove(g, p);
    _heap_mem_insert(g, new_p);
  }

  memset((char*)new_p + osize, 0, nsize - osize);
  return new_p;
}

// add new gen, and return the number (doesn't select it)
int32_t nb_gens_new_gen(Gens* g) {
  Arena arena;
  arena_init(&arena);
  int index = Arenas.size(&g->arenas);
  Arenas.push(&g->arenas, arena);
  return index;
}

// return max gen number
int32_t nb_gens_max_gen(Gens* g) {
  int size = Arenas.size(&g->arenas);
  if (size) {
    return size - 1;
  } else {
    return 0;
  }
}

// return current gen number
int32_t nb_gens_get_current(Gens* g) {
  return g->current;
}

// set current gen number
void nb_gens_set_current(Gens* g, int32_t i) {
  g->current = i;
}

// drop generations after current
void nb_gens_drop(Gens* g) {
  for (int i = g->current + 1; i < Arenas.size(&g->arenas); i++) {
    Arena* arena = Arenas.at(&g->arenas, i);
    arena_cleanup(arena);
  }
  g->arenas.size = g->current;
}

void nb_gens_check_memory(Gens* g) {
  if (MM.size(&g->checked_memory_map)) {
    log_err("Memory check failed, unfreed memory (%lu):", MM.size(&g->checked_memory_map));
    MMIter it;
    for (MM.iter_init(&it, &g->checked_memory_map); !MM.iter_is_end(&it); MM.iter_next(&it)) {
      log_err("  %p: klass=%u, extra_rc=%hu", (void*)it.slot->k, VAL_KLASS(it.slot->k), ((ValHeader*)it.slot->k)->extra_rc);
    }
    assert(false);
  }
}
