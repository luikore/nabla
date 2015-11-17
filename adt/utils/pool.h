#pragma once

// memory pool for fixed sized objects
// for small objects of fixed sizes, a pool allocator can be used to reduce time spent on malloc

// Customization:
// - POOL_CHUNK_MAX   max slot count in a chunk
// - POOL_SLOT_TYPE
// - POOL_SLOT_BSIZE  when POOL_SLOT_TYPE is not set, use this. NOTE: it must be > sizeof(void*)

#ifndef POOL_SLOT_TYPE
#define POOL_SLOT_TYPE void
#define POOL_SLOT_BSIZE sizeof(PoolSlot)
#endif

#ifndef POOL_SLOT_BSIZE
#define POOL_SLOT_BSIZE sizeof(POOL_SLOT_TYPE)
#endif

#ifndef POOL_CHUNK_MAX
#define POOL_CHUNK_MAX 255
#endif

#include <stdlib.h>
#include <stdint.h>

struct PoolSlotStruct;
typedef struct PoolSlotStruct PoolSlot;
struct PoolSlotStruct {
  PoolSlot* next;
  char pad[POOL_SLOT_BSIZE - sizeof(PoolSlot*)];
};

struct PoolChunkStruct;
typedef struct PoolChunkStruct PoolChunk;
struct PoolChunkStruct {
  uint64_t i; // 0...POOL_CHUNK_MAX
  PoolChunk* next;
  PoolSlot data[POOL_CHUNK_MAX];
};

struct PoolStruct;
typedef struct PoolStruct Pool;
struct PoolStruct {
  PoolChunk* chunk_head;
  PoolSlot* slot_head;
};

// todo make it atomic
static Pool* pool_new() {
  Pool* pool = malloc(sizeof(Pool));
  pool->slot_head = NULL;
  pool->chunk_head = malloc(sizeof(PoolChunk));
  pool->chunk_head->i = 0;
  pool->chunk_head->next = NULL;
  return pool;
}

static void pool_delete(Pool* pool) {
  while (pool->chunk_head) {
    PoolChunk* next = pool->chunk_head->next;
    free(pool->chunk_head);
    pool->chunk_head = next;
  }
  pool->slot_head = NULL;
  free(pool);
}

static POOL_SLOT_TYPE* pool_slot_alloc(Pool* pool) {
  if (pool->slot_head) {
    PoolSlot* ret = pool->slot_head;
    pool->slot_head = ret->next;
    return (POOL_SLOT_TYPE*)ret;
  }

  // if full, add new chunk
  if (pool->chunk_head->i == POOL_CHUNK_MAX - 1) {
    PoolChunk* ch = malloc(sizeof(PoolChunk));
    ch->i = 0;
    ch->next = pool->chunk_head;
    pool->chunk_head = ch;
  }

  PoolChunk* ch = pool->chunk_head;
  PoolSlot* ret = ch->data + ch->i++;
  return (POOL_SLOT_TYPE*)ret;
}

// NOTE there's no simple way to implement a pool_slot_each, some slots in a chunk may be already freed
