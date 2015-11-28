#include "dict.h"
#include "dict-bucket.h"
#include "dict-map.h"
#include "box.h"
#include <ccut.h>

// stored elements:
// a -> [lex=0, ha=1] (2+3+8 + 2+2+8 + 1)
// z -> [not=2, hooz=3] (2+3+8 + 2+4+8 + 1)
static Val build_test_map_to_buckets() {
  Map* m = MAP_NEW(2);

  BIT_MAP_SET(m->bit_map, 'a', true);
  BIT_MAP_SET(m->bit_map, 'z', true);

  Bucket* b;
  int pos;

# define ADD_KV(_k, _v) pos = DATA_SET(b->data, pos, strlen(_k), _k, VAL_FROM_INT(_v))

  b = BUCKET_NEW(2+3+8 + 2+2+8);
  BUCKET_ENTRIES(b) = 2;
  pos = 0;
  ADD_KV("lex", 0);
  ADD_KV("ha", 1);
  assert(pos == BUCKET_BYTES(b));
  m->slots[0] = (Val)b;

  b = BUCKET_NEW(2+3+8 + 2+4+8);
  BUCKET_ENTRIES(b) = 2;
  pos = 0;
  ADD_KV("not", 2);
  ADD_KV("hooz", 3);
  assert(pos == BUCKET_BYTES(b));
  m->slots[1] = (Val)b;

# undef ADD_KV

  return nb_dict_new_with_root((Val)m, 4);
}

// full bucket with keys "2" and "4..." (size=x)
// 2+1+8 + 2+x+8 = BUCKET_MAX
static Val build_test_full_bucket() {
  Bucket* b = BUCKET_NEW(BUCKET_MAX_BYTES);
  BUCKET_ENTRIES(b) = 2;

  int pos = 0;
  pos = DATA_SET(b->data, pos, 1, "2", VAL_FROM_INT(2));

  uint16_t ksize = BUCKET_MAX_BYTES - (2+8) - (2+1+8);
  char k[ksize];
  memset(k, '4', ksize);
  pos = DATA_SET(b->data, pos, ksize, k, VAL_FROM_INT(4));
  assert(pos == BUCKET_BYTES(b));

  return nb_dict_new_with_root((Val)b, 2);
}

// full bucket with keys "foo" and "b..." (size=x)
// 2+3+8 + 2+x+8 = BUCKET_MAX
static Val build_test_full_bucket_2() {
  Bucket* b = BUCKET_NEW(BUCKET_MAX_BYTES);
  BUCKET_ENTRIES(b) = 2;
  Val v;

  int pos = 0;
  v = nb_box_new(2);
  pos = DATA_SET(b->data, pos, 3, "foo", v);

  uint16_t ksize = BUCKET_MAX_BYTES - (2+8) - (2+3+8);
  char k[ksize];
  memset(k, 'b', ksize);
  v = nb_box_new(4);
  pos = DATA_SET(b->data, pos, ksize, k, v);
  assert(pos == BUCKET_BYTES(b));

  return nb_dict_new_with_root((Val)b, 2);
}

void dict_suite() {
  ccut_test("bucket new insert") {
    val_begin_check_memory();

    Val vs[] = {nb_box_new(0), nb_box_new(1), nb_box_new(2)};

    Bucket* b = BUCKET_NEW(2+3+8 + 2+3+8);
    BUCKET_ENTRIES(b) = 2;
    int pos = 0;
    RETAIN(vs[0]);
    pos = DATA_SET(b->data, pos, 3, "foo", vs[0]);
    RETAIN(vs[1]);
    pos = DATA_SET(b->data, pos, 3, "bar", vs[1]);

    Bucket* new_b = BUCKET_NEW_INSERT(b, "baz", 3, vs[2]);
    assert_eq(3, BUCKET_ENTRIES(new_b));
    assert_eq(BUCKET_BYTES(b) + 2 + 3 + 8, BUCKET_BYTES(new_b));

    RELEASE(new_b);
    RELEASE(b);
    RELEASE(vs[0]);
    RELEASE(vs[1]);
    RELEASE(vs[2]);

    val_end_check_memory();
  }

  ccut_test("find element") {
    val_begin_check_memory();
    // stored elements:
    // alex, aha, znot, zhooz
    Val d = build_test_map_to_buckets();
    Val v;
    assert_true(nb_dict_find(d, "alex", 4, &v), "should contain alex");
    assert_eq(VAL_FROM_INT(0), v);
    assert_true(nb_dict_find(d, "aha", 3, &v), "should contain aha");
    assert_eq(VAL_FROM_INT(1), v);
    assert_true(nb_dict_find(d, "znot", 4, &v), "should contain znot");
    assert_eq(VAL_FROM_INT(2), v);
    assert_true(nb_dict_find(d, "zhooz", 5, &v), "should contain zhooz");
    assert_eq(VAL_FROM_INT(3), v);

    assert_false(nb_dict_find(d, "a", 1, &v), "should not contain a");
    assert_false(nb_dict_find(d, "z", 1, &v), "should not contain z");
    assert_false(nb_dict_find(d, "blex", 4, &v), "should not contain blex");

    RELEASE(d);
    val_end_check_memory();
  }

  ccut_test("dict insert involving only buckets") {
    val_begin_check_memory();
    Val d = nb_dict_new();
    Val new_d;
    Val v;
    assert_eq(0, nb_dict_size(d));

    // insert to empty
    char* k = "Ty Lee";
    new_d = nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(0));
    REPLACE(d, new_d);
    assert_eq(1, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(0), v);

    // insert same k but different value
    new_d = nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(1));
    REPLACE(d, new_d);
    assert_eq(1, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(1), v);

    // insert before
    k = "Azula";
    new_d = nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(2));
    REPLACE(d, new_d);
    assert_eq(2, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(2), v);

    // insert after
    k = "Zuko";
    new_d = nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(3));
    REPLACE(d, new_d);
    assert_eq(3, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(3), v);

    // search prev inserted key
    k = "Azula";
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(2), v);

    RELEASE(d);
    val_end_check_memory();
  }

  ccut_test("heap val retain in bucket") {
    val_begin_check_memory();

    Val d = nb_dict_new();
    char* keys[] = {"Main", "Lex", "Peg"};
    Val vs[] = {nb_box_new(0), nb_box_new(1), nb_box_new(2)};
    size_t vs_size = sizeof(vs) / sizeof(Val);

    for (int i = 0; i < vs_size; i++) {
      REPLACE(d, nb_dict_insert(d, keys[i], strlen(keys[i]), vs[i]));
    }
    RELEASE(d);

    for (int i = 0; i < vs_size; i++) {
      RELEASE(vs[i]);
    }

    val_end_check_memory();
  }

  ccut_test("bucket burst and map insert") {
    val_begin_check_memory();
    // full bucket with keys "2", "4..."
    Val d = build_test_full_bucket();
    char* k;
    Val v;

    assert_eq(2, nb_dict_size(d));
    // same key: there should be no burst
    k = "2";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(22)));
    assert_eq(2, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(22), v);

    // burst
    k = "3";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(3)));
    assert_eq(3, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(3), v);

    // map insert
    k = "1";
    REPLACE(d, nb_dict_insert(d, k, strlen(k), VAL_FROM_INT(1)));
    assert_eq(4, nb_dict_size(d));
    assert_true(nb_dict_find(d, k, strlen(k), &v), "should contain k");
    assert_eq(VAL_FROM_INT(1), v);

    RELEASE(d);
    val_end_check_memory();
  }

  ccut_test("bucket burst and map insert retain") {
    val_begin_check_memory();

    // full bucket with keys "2", "4..."
    Val d = build_test_full_bucket();

    Val vs[] = {nb_box_new(0), nb_box_new(1)};
    size_t vs_size = sizeof(vs) / sizeof(Val);

    // burst
    REPLACE(d, nb_dict_insert(d, "3", 1, vs[0]));

    // map insert
    REPLACE(d, nb_dict_insert(d, "1", 1, vs[1]));

    RELEASE(d);
    for (int i = 0; i < vs_size; i++) {
      RELEASE(vs[i]);
    }

    val_end_check_memory();
  }

  ccut_test("bucket burst retain 2") {
    val_begin_check_memory();

    Val d = build_test_full_bucket_2();
    Val v = nb_box_new(0);
    REPLACE(d, nb_dict_insert(d, "3", 1, v));
    RELEASE(d);
    RELEASE(v);

    val_end_check_memory();
  }
}
