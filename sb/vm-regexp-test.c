#include <ccut.h>
#include "sb.h"

#include "vm-regexp-opcodes.c"

// ab
static uint16_t simple_reg[] = {
  CHAR, 'a', 0, // little endian
  CHAR, 'b', 0,
  MATCH
};

// (a+)(b+)
static uint16_t complex_reg[] = {
  SAVE, 2,
  /*2*/ CHAR, 'a', 0,
  FORK, 2, 0, 10, 0,
  /*10*/ SAVE, 3,
  SAVE, 4,
  /*14*/ CHAR, 'b', 0,
  FORK, 14, 0, 22, 0,
  /*22*/ SAVE, 5,
  MATCH
};

#define MATCH_REG(reg_ty)\
  memset(captures, 0, sizeof(captures));\
  res = sb_vm_regexp_exec(reg_ty, strlen(src), src, captures)

void vm_regexp_suite() {
  ccut_test("vm_regexp_exec simple regexp") {
    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 1;

    src = "ab";
    MATCH_REG(simple_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("ab"), captures[1]);

    src = "ab3";
    MATCH_REG(simple_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("ab"), captures[1]);

    src = "";
    MATCH_REG(simple_reg);
    assert_eq(false, res);
  }

  ccut_test("vm_regexp_exec complex regexp") {
    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 5;

    src = "aaab";
    MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("aaab"), captures[1]);

    src = "aaab3";
    MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("aaab"), captures[1]);
  }

  ccut_test("vm_regexp_exec greedy") {
    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 5;

    src = "abb";
    MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("abb"), captures[1]);
  }

  ccut_test("vm_regexp_from_string") {
    struct Iseq iseq;
    Iseq.init(&iseq, 0);

    const char* src = "foo-bar-baz";

    Val s = nb_string_new_c(src);
    Val err = sb_vm_regexp_from_string(&iseq, s);
    assert_eq(VAL_NIL, err);

    int32_t captures[20];
    bool res = sb_vm_regexp_exec(Iseq.at(&iseq, 0), strlen(src), src, captures);
    assert_eq(true, res);

    Iseq.cleanup(&iseq);
  }

  ccut_test("vm_regexp_compile char") {
    Val a = VAL_FROM_INT('a');
    // nb_struct
    // Val b = VAL_FROM_INT('b');
  }

  ccut_test("vm_regexp_compile range") {
  }

  ccut_test("vm_regexp_compile seq") {
  }

  ccut_test("vm_regexp_compile branch") {
  }
}
