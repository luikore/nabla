#include "string.h"
#include "map.h"
#include <ccut.h>
#include <string.h>

void string_suite() {
  ccut_test("slice and concat") {
    val_begin_check_memory();
    Val s1 = nb_string_new(strlen("slice and"), "slice and");
    assert_eq(strlen("slice and"), nb_string_byte_size(s1));

    Val s2 = nb_string_new(strlen("concat"), "concat");
    assert_eq(strlen("concat"), nb_string_byte_size(s2));

    REPLACE(s1, nb_string_slice(s1, 0, 5));
    assert_eq(5, nb_string_byte_size(s1));
    REPLACE(s1, nb_string_concat(s1, s2));

    size_t sz = nb_string_byte_size(s1);
    const char* s = nb_string_ptr(s1);
    assert_eq(strlen("sliceconcat"), sz);
    assert_eq(0, memcmp(s, "sliceconcat", sz));

    RELEASE(s2);
    RELEASE(s1);
    val_end_check_memory();
  }

  ccut_test("string literal") {
    val_begin_check_memory();
    Val s1 = nb_string_new_literal_c("hello");
    Val s2 = nb_string_new_literal_c("world");
    assert_neq(s1, s2);
    char seq[] = "hello\0";
    Val s3 = nb_string_new_literal_c(seq);
    assert_eq(s1, s3);
    val_end_check_memory();
  }

  ccut_test("string literal ptr and size") {
    val_begin_check_memory();
    Val s = nb_string_new_literal_c("foo");
    assert_eq(3, nb_string_byte_size(s));
    assert_mem_eq("foo", nb_string_ptr(s), 3);
    val_end_check_memory();
  }

  ccut_test("string literal in map") {
    val_begin_check_memory();
    Val m = nb_map_new();

    REPLACE(m, nb_map_insert(m, nb_string_new_literal_c("foo"), 3));
    REPLACE(m, nb_map_insert(m, nb_string_new_literal_c("bar"), 4));
    Val v = nb_map_find(m, nb_string_new_literal_c("foo"));
    assert_eq(3, v);
    v = nb_map_find(m, nb_string_new_literal_c("bar"));
    assert_eq(4, v);

    RELEASE(m);
    val_end_check_memory();
  }

  ccut_test("hash of string literals") {
    val_begin_check_memory();
    Val s1 = nb_string_new_literal_c("char");
    Val s2 = nb_string_new(4, "char");
    uint64_t h1 = val_hash(s1);
    uint64_t h2 = val_hash(s2);
    assert_eq(h2, h1);

    RELEASE(s2);
    val_end_check_memory();
  }
}
