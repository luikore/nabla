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

static uint32_t _klass(const char* name) {
  uint32_t namespace = sb_klass();
  return klass_find_c(name, namespace);
}

#define MATCH_REG(reg_ty)\
  memset(captures, 0, sizeof(captures));\
  res = sb_vm_regexp_exec(reg_ty, strlen(src), src, captures)

#define ASSERT_ISEQ_MATCH(expeted, iseq) do {\
  int expected_size = sizeof(expected) / sizeof(uint16_t);\
  assert_eq(expected_size, Iseq.size(&iseq));\
  assert_mem_eq(expected, Iseq.at(&iseq, 0), sizeof(expected));\
} while(0)

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
    Val char_node = VAL_FROM_INT('a');
    Val regexp = nb_struct_new(_klass("Regexp"), 1, &char_node);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {CHAR, 'a', 0, MATCH, END};
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("vm_regexp_compile seq") {
    Val list = nb_cons_new(VAL_FROM_INT('a'), VAL_NIL);
    list = nb_cons_new(VAL_FROM_INT('b'), list);
    Val seq = nb_struct_new(_klass("Seq"), 1, &list);
    Val regexp = nb_struct_new(_klass("Regexp"), 1, &seq);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {CHAR, 'a', 0, CHAR, 'b', 0, MATCH, END};
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("vm_regexp_compile branch") {
    Val list = nb_cons_new(VAL_FROM_INT('a'), VAL_NIL);
    list = nb_cons_new(VAL_FROM_INT('b'), list);
    Val regexp = nb_struct_new(_klass("Regexp"), 1, &list);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    printf("\n");
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {
      FORK, 5, 0, 11, 0,
      /*5*/ CHAR, 'a', 0,
      JMP, 14, 0,
      /*11*/ CHAR, 'b', 0,
      /*14*/ MATCH, END
    };

    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("vm_regexp_compile a?") {
    
  }

  ccut_test("vm_regexp_compile a+") {
  }

  ccut_test("vm_regexp_compile a*") {
  }
}
