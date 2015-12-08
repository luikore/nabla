#pragma once

// (map internal node)
// collision array, when hash of 2 entries are equal, collision array is used

#include "map-node.h"

typedef struct {
  ValHeader header; // klass = KLASS_MAP_COLA, flags = MAX_NODE_LEVEL, user1 = int valued
  int64_t size;
  Kv kvs[];
} Cola;

#define COLA_BYTES(n) (sizeof(Cola) + n * sizeof(Kv))

static Cola* COLA_ALLOC(int size, bool is_int_valued) {
  Cola* cola = val_alloc(COLA_BYTES(size));
  cola->header.klass = KLASS_MAP_COLA;
  LEVEL(cola) = MAX_NODE_LEVEL;
  IS_INT_VALUED(cola) = is_int_valued;
  SIZE(cola) = size;
  return cola;
}

static void COLA_KV_RETAINS(Cola* cola) {
  for (int i = 0; i < SIZE(cola); i++) {
    KV_RETAIN(cola->kvs[i], IS_INT_VALUED(cola));
  }
}

// prereq: kv.k != k
static Cola* COLA_NEW2(Kv kv, Val k, Val v, bool is_int_valued) {
  Cola* cola = COLA_ALLOC(2, is_int_valued);
  cola->kvs[0] = kv;
  cola->kvs[1].k = k;
  cola->kvs[1].v = v;
  COLA_KV_RETAINS(cola);
  return cola;
}

static bool COLA_FIND(Cola* cola, Val k, Val* v) {
  for (int i = 0; i < SIZE(cola); i++) {
    if (val_eq(cola->kvs[i].k, k)) {
      *v = cola->kvs[i].v;
      RETAIN(*v);
      return true;
    }
  }
  return false;
}

static Cola* COLA_INSERT(Cola* old, Val k, Val v, bool* size_increased) {
  int insert_i = SIZE(old);
  for (int i = 0; i < SIZE(old); i++) {
    if (val_eq(old->kvs[i].k, k)) {
      insert_i = i;
      break;
    }
  }
  *size_increased = (insert_i == SIZE(old));
  int new_size = (*size_increased ? SIZE(old) + 1 : SIZE(old));
  int new_bytes = COLA_BYTES(new_size);

  Cola* cola = val_dup(old, COLA_BYTES(SIZE(old)), new_bytes);
  SIZE(cola) = new_size;
  cola->kvs[insert_i].k = k;
  cola->kvs[insert_i].v = v;
  COLA_KV_RETAINS(cola);
  return cola;
}

// may return:
// - Cola
// - Kv
static Slot COLA_REMOVE(Cola* old, Val k, Val* prev_v, bool* size_decreased) {
  assert(SIZE(old) > 1);

  int remove_i = -1;
  for (int i = 0; i < SIZE(old); i++) {
    if (val_eq(old->kvs[i].k, k)) {
      remove_i = i;
      break;
    }
  }

  if (remove_i < 0) {
    *size_decreased = false;
    RETAIN(old);
    Slot res = {.h = (Val)old};
    return res;
  } else if (SIZE(old) == 2) {
    int replace_i = -1, keep_i = -1;
    if (val_eq(old->kvs[0].k, k)) {
      replace_i = 0;
      keep_i = 1;
    } else if (val_eq(old->kvs[1].k, k)) {
      replace_i = 1;
      keep_i = 0;
    }

    if (replace_i >= 0) {
      *size_decreased = true;
      *prev_v = old->kvs[replace_i].v;
      KV_RETAIN(old->kvs[keep_i], IS_INT_VALUED(old));
      Slot res = {.kv = old->kvs[keep_i]};
      return res;
    } else {
      *size_decreased = false;
      RETAIN(old);
      Slot res = {.h = (Val)old};
      return res;
    }
  }

  // remove_i >= 0, must alloc new
  Cola* cola = COLA_ALLOC(SIZE(old) - 1, IS_INT_VALUED(old));
  for (int i = 0, j = 0; i < SIZE(old); i++) {
    if (i == remove_i) {
      *prev_v = old->kvs[i].v;
    } else {
      cola->kvs[j] = old->kvs[i];
      KV_RETAIN(cola->kvs[j], IS_INT_VALUED(cola));
      j++;
    }
  }
  *size_decreased = true;
  Slot res = {.h = (Val)cola};
  return res;
}

static void COLA_DESTROY(void* ptr) {
  Cola* cola = ptr;
  for (int i = 0; i < SIZE(cola); i++) {
    KV_RELEASE(cola->kvs[i], IS_INT_VALUED(cola));
  }
}
