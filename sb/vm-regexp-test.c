#include <ccut.h>
#include "sb.h"

#include "vm-regexp-op-codes.h"

// (a+)(b+)
static uint16_t complex_reg[] = {
  SPLIT_META(33, 0),
  SAVE, 2,
  /*9*/ CHAR, AS_ARG32('a'),
  FORK, AS_ARG32(9), AS_ARG32(17),
  /*17*/ SAVE, 3,
  SAVE, 4,
  /*21*/ CHAR, AS_ARG32('b'),
  FORK, AS_ARG32(21), AS_ARG32(29),
  /*29*/ SAVE, 5,
  MATCH, END
};

static Val _struct(const char* name, int argc, Val* argv) {
  uint32_t namespace = sb_klass();
  uint32_t klass = klass_find_c(name, namespace);
  return nb_struct_new(klass, argc, argv);
}

static Val _range(int from, int to) {
  return _struct("CharRange", 2, (Val[]){VAL_FROM_INT(from), VAL_FROM_INT(to)});
}

static void _compile_quantified_reg(struct Iseq* iseq, int chr, const char* type) {
  val_gens_set_current(val_gens_new_gen());

  Val quantifier = nb_string_new_literal_c(type);
  Val a = VAL_FROM_INT(chr);
  Val quantified = _struct("Quantified", 2, (Val[]){a, quantifier});
  Val regexp = _struct("Regexp", 1, (Val[]){quantified});
  Val err = sb_vm_regexp_compile(iseq, VAL_NIL, regexp);
  if (err != VAL_NIL) {
    fatal_err("%.*s", (int)nb_string_byte_size(err), nb_string_ptr(err));
  }

  val_gens_set_current(0);
  val_gens_drop();

  RELEASE(regexp);
}

#define MATCH_REG(reg) ({\
  memset(captures, 0, sizeof(captures));\
  sb_vm_regexp_exec(reg, strlen(src), src, captures);\
})

#define ASSERT_ISEQ_MATCH(expeted, iseq) do {\
  int expected_size = sizeof(expected) / sizeof(uint16_t);\
  assert_eq(expected_size, Iseq.size(&iseq));\
  assert_mem_eq(expected, Iseq.at(&iseq, 0), sizeof(expected));\
} while(0)

void vm_regexp_suite() {
  ccut_test("sb_vm_regexp_exec /ab/") {
    uint16_t simple_reg[] = {
      SPLIT_META(15, 0),
      CHAR, AS_ARG32('a'),
      CHAR, AS_ARG32('b'),
      MATCH, END
    };

    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 1;

    src = "ab";
    res = MATCH_REG(simple_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("ab"), captures[1]);

    src = "ab3";
    res = MATCH_REG(simple_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("ab"), captures[1]);

    src = "";
    res = MATCH_REG(simple_reg);
    assert_eq(false, res);
  }

  ccut_test("sb_vm_regexp_exec /a*/") {
    uint16_t reg[] = {
      SPLIT_META(20, 0),
      /*7*/ FORK, AS_ARG32(12), AS_ARG32(18),
      /*12*/ CHAR, AS_ARG32('a'),
      /*15*/ JMP, AS_ARG32(19),
      /*18*/ MATCH,
      /*19*/ END
    };

    int32_t captures[20];
    const char* src;
    bool res;

    src = "b";
    res = MATCH_REG(reg);
    assert_eq(true, res);
    assert_eq(0, captures[1]);
  }

  ccut_test("sb_vm_regexp_exec /(a+)(b+)/") {
    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 5;

    src = "aaab";
    res = MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("aaab"), captures[1]);

    src = "aaab3";
    res = MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("aaab"), captures[1]);
  }

  ccut_test("sb_vm_regexp_exec /(a+)(b+)/ greedy") {
    int32_t captures[20];
    const char* src;
    bool res;
    int max_capture_index = 5;

    src = "abb";
    res = MATCH_REG(complex_reg);
    assert_eq(true, res);
    assert_eq(max_capture_index, captures[0]);
    assert_eq(strlen("abb"), captures[1]);
  }

  ccut_test("sb_vm_regexp_from_string") {
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

  ccut_test("sb_vm_regexp_compile char") {
    Val char_node = VAL_FROM_INT('a');
    Val regexp = _struct("Regexp", 1, &char_node);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    uint16_t expected[] = {
      SPLIT_META(12, 0),
      CHAR, AS_ARG32('a'),
      MATCH, END
    };
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("sb_vm_regexp_compile seq") {
    Val list = nb_cons_new(VAL_FROM_INT('a'), VAL_NIL);
    list = nb_cons_new(VAL_FROM_INT('b'), list);
    Val seq = _struct("Seq", 1, &list);
    Val regexp = _struct("Regexp", 1, &seq);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    uint16_t expected[] = {
      SPLIT_META(15, 0),
      CHAR, AS_ARG32('a'),
      CHAR, AS_ARG32('b'),
      MATCH, END
    };
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("sb_vm_regexp_compile branch") {
    Val list = nb_cons_new(VAL_FROM_INT('a'), VAL_NIL);
    list = nb_cons_new(VAL_FROM_INT('b'), list);
    Val regexp = _struct("Regexp", 1, &list);

    struct Iseq iseq;
    Iseq.init(&iseq, 0);
    sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    uint16_t expected[] = {
      SPLIT_META(23, 0),
      FORK, AS_ARG32(12), AS_ARG32(18),
      /*12*/ CHAR, AS_ARG32('a'),
      JMP, AS_ARG32(21),
      /*18*/ CHAR, AS_ARG32('b'),
      /*21*/ MATCH, END
    };

    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("sb_vm_regexp_compile /^/") {
    Val anchor = _struct("PredefAnchor", 1, (Val[]){nb_string_new_literal_c("^")});
    Val regexp = _struct("Regexp", 1, (Val[]){anchor});

    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));
    uint16_t expected[] = {
      SPLIT_META(10, 0),
      ANCHOR_BOL, MATCH, END
    };
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
  }

  ccut_test("sb_vm_regexp_compile /\\w/") {
    Val anchor = _struct("CharGroupPredef", 1, (Val[]){nb_string_new_literal_c("\\w")});
    Val regexp = _struct("Regexp", 1, (Val[]){anchor});

    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));
    uint16_t expected[] = {
      SPLIT_META(10, 0),
      CG_W, MATCH, END
    };
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
  }

  ccut_test("sb_vm_regexp_compile /[^a]/") {
    Val range = _range('a', 'a');
    Val ranges = nb_cons_new(range, VAL_NIL);
    // negative char group
    Val cg = _struct("BracketCharGroup", 2, (Val[]){VAL_FALSE, ranges});
    Val regexp = _struct("Regexp", 1, (Val[]){cg});

    int32_t gen = val_gens_new_gen();
    val_gens_set_current(gen);
    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));
    int range_ops = 0;
    for (int i = 0; i < Iseq.size(&iseq); i++) {
      if (*Iseq.at(&iseq, i) == JIF_RANGE) {
        range_ops++;
      }
    }
    assert_eq(2, range_ops);

    Iseq.cleanup(&iseq);
    val_gens_set_current(0);
    val_gens_drop();
    RELEASE(regexp);
  }

  // the following regexp are too complex, we have to do actual match instead

  ccut_test("sb_vm_regexp_compile /[a[^b]]/") {
    Val inner = nb_cons_new(_range('b', 'b'), VAL_NIL);
    Val inner_cg = _struct("BracketCharGroup", 2, (Val[]){VAL_FALSE, inner});
    Val outer = nb_cons_list(2, (Val[]){_range('a', 'a'), inner_cg});
    Val outer_cg = _struct("BracketCharGroup", 2, (Val[]){VAL_TRUE, outer});
    Val regexp = _struct("Regexp", 1, (Val[]){outer_cg});

    int32_t gen = val_gens_new_gen();
    val_gens_set_current(gen);
    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));

    int32_t captures[20];
    const char* src = "a";
    bool res;
    res = MATCH_REG(Iseq.at(&iseq, 0));
    assert_eq(true, res);
    src = "b";
    res = MATCH_REG(Iseq.at(&iseq, 0));
    assert_eq(false, res);
    src = "c";
    res = MATCH_REG(Iseq.at(&iseq, 0));
    assert_eq(true, res);

    Iseq.cleanup(&iseq);
    val_gens_set_current(0);
    val_gens_drop();
    RELEASE(regexp);
  }

  ccut_test("sb_vm_regexp_compile /a?/") {
    struct Iseq iseq;
    Iseq.init(&iseq, 5);
    _compile_quantified_reg(&iseq, 'a', "?");
    uint16_t* byte_code = Iseq.at(&iseq, 0);

    int32_t captures[20];
    const char* src;
    bool res;

    src = "a";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(1, captures[1]);

    src = "";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(0, captures[1]);

    src = "aa";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(1, captures[1]);

    src = "b";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(0, captures[1]);

    Iseq.cleanup(&iseq);
  }

  ccut_test("sb_vm_regexp_compile /a+/") {
    struct Iseq iseq;
    Iseq.init(&iseq, 5);
    _compile_quantified_reg(&iseq, 'a', "+");
    uint16_t* byte_code = Iseq.at(&iseq, 0);

    int32_t captures[20];
    const char* src;
    bool res;

    src = "a";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(1, captures[1]);

    src = "";
    res = MATCH_REG(byte_code);
    assert_eq(false, res);

    src = "aa";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(2, captures[1]);

    src = "b";
    res = MATCH_REG(byte_code);
    assert_eq(false, res);

    Iseq.cleanup(&iseq);
  }

  ccut_test("sb_vm_regexp_compile /a*/") {
    struct Iseq iseq;
    Iseq.init(&iseq, 5);
    _compile_quantified_reg(&iseq, 'a', "*");
    uint16_t* byte_code = Iseq.at(&iseq, 0);

    int32_t captures[20];
    const char* src;
    bool res;

    src = "a";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(1, captures[1]);

    src = "";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(0, captures[1]);

    src = "aa";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(2, captures[1]);

    src = "b";
    res = MATCH_REG(byte_code);
    assert_eq(true, res);
    assert_eq(0, captures[1]);

    Iseq.cleanup(&iseq);
  }

  ccut_test("sb_vm_regexp_compile /(?:b)(c)/") {
    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val special = nb_string_new_literal_c("?:");
    Val non_capture_group = _struct("Group", 2, (Val[]){special, VAL_FROM_INT('b')});
    special = nb_string_new_literal_c("");
    Val capture_group = _struct("Group", 2, (Val[]){special, VAL_FROM_INT('c')});
    Val seq_content = nb_cons_list(2, (Val[]){capture_group, non_capture_group});
    Val seq = _struct("Seq", 1, (Val[]){seq_content});
    Val regexp = _struct("Regexp", 1, (Val[]){seq});

    int32_t gen = val_gens_new_gen();
    val_gens_set_current(gen);

    Val err = sb_vm_regexp_compile(&iseq, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    RELEASE(regexp);

    val_gens_set_current(0);
    val_gens_drop();

    // sb_vm_regexp_decompile(Iseq.at(&iseq, 0));

    int32_t captures[20];
    const char* src = "bc";
    bool res = MATCH_REG(Iseq.at(&iseq, 0));
    assert_eq(true, res);
    assert_eq(1, captures[2]);
    assert_eq(2, captures[3]);

    Iseq.cleanup(&iseq);
  }
}
