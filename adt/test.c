#include <ccut.h>
#include "val.h"
#include "utils/mut-array.h"
#include "utils/mut-map.h"

void val_suite();
void box_suite();
void map_suite();
void array_suite();
void dict_suite();
void sym_table_suite();
void string_suite();

#pragma mark ### tests for basic assumptions

// much code is critical on the word width of the system
// todo move these code into config step?
void base_suite() {
  ccut_test("data size") {
    assert_eq(8, sizeof(void*));
    assert_eq(8, sizeof(Val));
    assert_eq(8, sizeof(uint64_t));
    assert_eq(8, sizeof(unsigned long));
    assert_eq(8, sizeof(unsigned long long));
    assert_eq(8, sizeof(size_t));
    assert_eq(1, sizeof(char));
    assert_eq(-120, (char)(unsigned char)(char)(-120));
    assert_eq(255, (unsigned char)(char)(unsigned char)(255));
  }
}

#pragma mark ### test utils/mut-array.h
typedef struct {
  int x;
  int y;
} MyPoint;

MUT_ARRAY_DECL(Points, MyPoint);

void mut_array_suite() {
  ccut_test("insert elements") {
    struct Points da;
    Points.init(&da, 0);
    for (int i = 0; i < 50; i++) {
      MyPoint p = {i, i*2};
      Points.push(&da, p);
    }
    assert_eq(50, Points.size(&da));

    int mismatch = -1;
    for (int i = 0; i < 50; i++) {
      MyPoint* p = Points.at(&da, i);
      if (i != p->x || i*2 != p->y) {
        mismatch = i;
        break;
      }
    }
    assert_eq(-1, mismatch);
    Points.cleanup(&da);
  }

  ccut_test("reverse") {
    struct Points a;
    Points.init(&a, 3);
    for (int i = 0; i < 3; i++) {
      MyPoint p = {0, i + 1};
      Points.push(&a, p);
    }
    Points.reverse(&a);
    assert_eq(3, Points.at(&a, 0)[0].y);
    assert_eq(2, Points.at(&a, 1)[0].y);
    assert_eq(1, Points.at(&a, 2)[0].y);
  }
}

#pragma mark ### test utils/mut-map.h

static bool mm_eq(uint64_t k1, uint64_t k2) {
  return k1 == k2;
}
static uint64_t mm_hash(uint64_t k) {
  return (k >> 2);
}
MUT_MAP_DECL(MM, uint64_t, uint64_t, mm_hash, mm_eq);

void mut_map_suite() {
  ccut_test("insert different hash") {
    struct MM mm;
    MM.init(&mm);
    MM.insert(&mm, 1 << 2, 1);
    MM.insert(&mm, 2 << 2, 2);

    assert_eq(2, MM.size(&mm));
    uint64_t v;
    assert_true(MM.find(&mm, 1 << 2, &v), "should find key1");
    assert_eq(1, v);
    assert_true(MM.find(&mm, 2 << 2, &v), "should find key2");
    assert_eq(2, v);

    MM.remove(&mm, 1 << 2);
    assert_eq(1, MM.size(&mm));
    assert_false(MM.find(&mm, 1 << 2, &v), "should not find removed key1");

    MM.remove(&mm, 1 << 2);
    assert_eq(1, MM.size(&mm));

    MM.cleanup(&mm);
  }

  ccut_test("insert same key") {
    struct MM mm;
    MM.init(&mm);
    MM.insert(&mm, 1 << 2, 1);
    MM.insert(&mm, 1 << 2, 2);

    assert_eq(1, MM.size(&mm));
    uint64_t v;
    assert_true(MM.find(&mm, 1 << 2, &v), "should find key1");
    assert_eq(2, v);

    MM.cleanup(&mm);
  }

  ccut_test("insert hash collision keys") {
    struct MM mm;
    MM.init(&mm);
    MM.insert(&mm, 0, 0);
    MM.insert(&mm, 1, 1);
    MM.insert(&mm, 2, 2);

    assert_eq(3, MM.size(&mm));
    uint64_t v;
    assert_true(MM.find(&mm, 0, &v), "should find key0");
    assert_eq(0, v);
    assert_true(MM.find(&mm, 1, &v), "should find key1");
    assert_eq(1, v);
    assert_true(MM.find(&mm, 2, &v), "should find key2");
    assert_eq(2, v);

    MM.remove(&mm, 2);
    assert_eq(2, MM.size(&mm));
    assert_true(MM.find(&mm, 0, &v), "should find key0");
    assert_true(MM.find(&mm, 1, &v), "should find key1");
    assert_false(MM.find(&mm, 2, &v), "should not find key2");

    MM.remove(&mm, 0);
    assert_eq(1, MM.size(&mm));
    assert_false(MM.find(&mm, 0, &v), "should not find key0");
    assert_true(MM.find(&mm, 1, &v), "should find key1");
    assert_false(MM.find(&mm, 2, &v), "should not find key2");

    MM.remove(&mm, 1);
    assert_eq(0, MM.size(&mm));
    assert_false(MM.find(&mm, 0, &v), "should not find key0");
    assert_false(MM.find(&mm, 1, &v), "should not find key1");
    assert_false(MM.find(&mm, 2, &v), "should not find key2");

    MM.cleanup(&mm);
  }

  ccut_test("rehash") {
    struct MM mm;
    MM.init(&mm);
    // insert over 64 keys and it should be fine
    for (int i = 0; i < 200; i++) {
      MM.insert(&mm, i, i);
    }
    for (int i = 0; i < 200; i++) {
      uint64_t v;
      if (!MM.find(&mm, i, &v)) {
        assert_true(false, "should find i=%d", i);
      }
    }
    MM.cleanup(&mm);
  }
}

#pragma mark ### test utils/pool.h

typedef struct {
  char data[100];
} Slot;
#define POOL_SLOT_TYPE Slot
#include "utils/pool.h"
void pool_suite() {
  ccut_test("test pool") {
    Pool* p = pool_new();
    for (int i = 0; i < 1000; i++) {
      Slot* slot = pool_slot_alloc(p);
    }
    pool_delete(p);
  }
}

#pragma mark ### test utils/arena.h

#include "utils/arena.h"
void arena_suite() {
  ccut_test("test arena") {
    Arena* a = arena_new();
    for (int i = 0; i < 100; i++) {
      void* pointer = arena_slot_alloc(a, i);
    }
    arena_delete(a);
  }

  ccut_test("arena pointer distance") {
    Arena* a = arena_new();
    void* p1 = arena_slot_alloc(a, 2);
    void* p2 = arena_slot_alloc(a, 1);
    assert_eq(2 * sizeof(void*), (uintptr_t)p2 - (uintptr_t)p1);
    arena_delete(a);
  }

  ccut_test("push and pop state") {
    Arena* a = arena_new();
    for (int i = 0; i < 100; i++) {
      arena_slot_alloc(a, 1);
    }
    arena_push(a);
    ArenaChunk* chunk = a->head;
    int i = chunk->i;

    for (int i = 0; i < 10; i++) {
      arena_slot_alloc(a, 1);
    }
    arena_pop(a);
    assert_eq((void*)chunk, a->head);
    assert_eq(i, a->head->i);

    arena_delete(a);
  }
}

#pragma mark ### run them all

int main (int argc, char const *argv[]) {
  // ccut_trap_asserts();
  ccut_run_suite(base_suite);
  ccut_run_suite(box_suite);
  ccut_run_suite(mut_array_suite);
  ccut_run_suite(mut_map_suite);
  ccut_run_suite(pool_suite);
  ccut_run_suite(arena_suite);
  ccut_run_suite(val_suite);
  ccut_run_suite(map_suite);
  ccut_run_suite(array_suite);
  ccut_run_suite(dict_suite);
  ccut_run_suite(sym_table_suite);
  ccut_run_suite(string_suite);
  ccut_print_stats();
  return 0;
}
