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

    src = "ab";
    MATCH_REG(simple_reg);
    assert_eq(true, res);

    src = "";
    MATCH_REG(simple_reg);
    assert_eq(false, res);
  }

  ccut_test("vm_regexp_exec complex regexp") {
    int32_t captures[20];
    const char* src;
    bool res;

    src = "aaab";
    MATCH_REG(complex_reg);
    assert_eq(true, res);

    src = "abb";
    MATCH_REG(complex_reg);
    assert_eq(true, res);
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
}
