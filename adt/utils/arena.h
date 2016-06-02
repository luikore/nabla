#pragma once

// memory arena for batch free objects
// very similar to a pool, but without free list, so it can be var-lengthed.

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define ARENA_CHUNK_SIZE 253

struct ArenaChunkStruct;
typedef struct ArenaChunkStruct ArenaChunk;
struct ArenaChunkStruct {
  ArenaChunk* next;
  uint64_t i;
  uint64_t data[ARENA_CHUNK_SIZE];
};

typedef struct {
  ArenaChunk* chunk;
  uint64_t i;
} ArenaStack;

typedef struct {
  ArenaChunk* head;
  ArenaChunk init_chunk; // inline first chunk
} Arena;

static void arena_init(Arena* arena) {
  ArenaChunk* chunk = &arena->init_chunk;
  arena->head = chunk;
  chunk->i = 0;
  chunk->next = NULL;
}

static Arena* arena_new() {
  Arena* arena = malloc(sizeof(Arena));
  arena_init(arena);
  return arena;
}

#include <stdbool.h>
bool val_is_tracing();
static void* arena_slot_alloc(Arena* arena, uint8_t qword_count) {
  // TODO
  // - refactor to use byte size
  // - when size is very large, use single chunk
  assert(qword_count <= ARENA_CHUNK_SIZE);

  if (val_is_tracing()) {
    //content
  }

  if (arena->head->i + qword_count > ARENA_CHUNK_SIZE) {
    ArenaChunk* chunk = malloc(sizeof(ArenaChunk));
    chunk->i = 0;
    chunk->next = arena->head;
    arena->head = chunk;
  }

  ArenaChunk* chunk = arena->head;
  void* res = chunk->data + chunk->i;
  chunk->i += qword_count;
  return res;
}

static void arena_cleanup(Arena* arena) {
  ArenaChunk* chunk = arena->head;
  // NOTE do not free the last chunk since it is together allocated with arena
  while (chunk->next) {
    ArenaChunk* next = chunk->next;
    free(chunk);
    chunk = next;
  }
}

static void arena_delete(Arena* arena) {
  arena_cleanup(arena);
  free(arena);
}
