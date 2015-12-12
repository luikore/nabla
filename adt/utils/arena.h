#pragma once

// memory arena for batch free objects
// very similar to a pool, but without free list, so it can be var-lengthed.

#include <stdlib.h>
#include <stdint.h>

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

struct ArenaStruct {
  ArenaChunk* head;
  ArenaStack* stack;
  uint32_t stack_size;
  uint32_t stack_cap;
};

typedef struct ArenaStruct Arena;

static Arena* arena_new() {
  Arena* arena = malloc(sizeof(Arena) + sizeof(ArenaChunk));
  ArenaChunk* chunk = (void*)(arena + 1);
  arena->head = chunk;
  chunk->i = 0;
  chunk->next = NULL;
  arena->stack = NULL;
  arena->stack_size = arena->stack_cap = 0;
  return arena;
}

static void* arena_slot_alloc(Arena* arena, uint8_t qword_count) {
  assert(qword_count <= ARENA_CHUNK_SIZE);

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

// push state
// save a state, create temporally nodes, then pop, can save a lot of spaces
static void arena_push(Arena* arena) {
  if (!arena->stack) {
    arena->stack_cap = 4;
    arena->stack = malloc(sizeof(ArenaStack) * arena->stack_cap);
  } else {
    if (arena->stack_size + 1 >= arena->stack_cap) {
      arena->stack_cap *= 2;
      arena->stack = realloc(arena->stack, sizeof(ArenaStack) * arena->stack_cap);
    }
  }
  arena->stack[arena->stack_size++] = (ArenaStack){.chunk = arena->head, .i = arena->head->i};
}

// pop state
static void arena_pop(Arena* arena) {
  assert(arena->stack && arena->stack_size);

  arena->stack_size--;
  ArenaStack* top = arena->stack + arena->stack_size;
  if (top->chunk == arena->head) {
    arena->head->i = top->i;
  } else {
    // pop-cross a chunk is too complex... so only pop i
    arena->head->i = 0;
  }
}

static void arena_delete(Arena* arena) {
  ArenaChunk* chunk = arena->head;
  // NOTE do not free the last chunk since it is together allocated with arena
  while (chunk->next) {
    ArenaChunk* next = chunk->next;
    free(chunk);
    chunk = next;
  }
  free(arena);
}
