#include <ccut.h>
#include "sb.h"

#include "vm-regexp-opcodes.c"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define AS_ARG32(c) c, 0
#else
#define AS_ARG32(c) 0, c
#endif

// (a+)(b+)
static uint16_t complex_reg[] = {
  SAVE, 2,
  /*2*/ CHAR, AS_ARG32('a'),
  FORK, AS_ARG32(2), AS_ARG32(10),
  /*10*/ SAVE, 3,
  SAVE, 4,
  /*14*/ CHAR, AS_ARG32('b'),
  FORK, AS_ARG32(14), AS_ARG32(22),
  /*22*/ SAVE, 5,
  MATCH
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
  void* arena = val_arena_new();
  Val quantifier = nb_string_new_literal_c(type);
  Val a = VAL_FROM_INT(chr);
  Val quantified = _struct("Quantified", 2, (Val[]){a, quantifier});
  Val regexp = _struct("Regexp", 1, (Val[]){quantified});
  Val err = sb_vm_regexp_compile(iseq, arena, VAL_NIL, regexp);
  if (err != VAL_NIL) {
    fatal_err("%.*s", (int)nb_string_byte_size(err), nb_string_ptr(err));
  }
  val_arena_delete(arena);
}

#define MATCH_REG(reg_ty) ({\
  memset(captures, 0, sizeof(captures));\
  sb_vm_regexp_exec(reg_ty, strlen(src), src, captures);\
})

#define ASSERT_ISEQ_MATCH(expeted, iseq) do {\
  int expected_size = sizeof(expected) / sizeof(uint16_t);\
  assert_eq(expected_size, Iseq.size(&iseq));\
  assert_mem_eq(expected, Iseq.at(&iseq, 0), sizeof(expected));\
} while(0)

void vm_regexp_suite() {
  ccut_test("sb_vm_regexp_exec /ab/") {
    uint16_t simple_reg[] = {
      CHAR, AS_ARG32('a'),
      CHAR, AS_ARG32('b'),
      MATCH
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
      /*0*/ FORK, AS_ARG32(5), AS_ARG32(11),
      /*5*/ CHAR, AS_ARG32('a'),
      /*8*/ JMP, AS_ARG32(0),
      /*11*/ MATCH,
      /*12*/ END
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
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {CHAR, AS_ARG32('a'), MATCH, END};
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
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {CHAR, AS_ARG32('a'), CHAR, AS_ARG32('b'), MATCH, END};
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
    sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    uint16_t expected[] = {
      FORK, AS_ARG32(5), AS_ARG32(11),
      /*5*/ CHAR, AS_ARG32('a'),
      JMP, AS_ARG32(14),
      /*11*/ CHAR, AS_ARG32('b'),
      /*14*/ MATCH, END
    };

    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));
    ASSERT_ISEQ_MATCH(expected, iseq);

    Iseq.cleanup(&iseq);
    RELEASE(regexp);
  }

  ccut_test("sb_vm_regexp_compile /^/") {
    Val anchor = _struct("PredefAnchor", 1, (Val[]){nb_string_new_literal_c("^")});
    Val regexp = _struct("Regexp", 1, (Val[]){anchor});

    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));
    uint16_t expected[] = {
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

    Val err = sb_vm_regexp_compile(&iseq, NULL, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));
    uint16_t expected[] = {
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

    void* arena = val_arena_new();
    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, arena, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));
    int range_ops = 0;
    for (int i = 0; i < Iseq.size(&iseq); i++) {
      if (*Iseq.at(&iseq, i) == JIF_RANGE) {
        range_ops++;
      }
    }
    assert_eq(2, range_ops);

    Iseq.cleanup(&iseq);
    val_arena_delete(arena);
    RELEASE(regexp);
  }

  // the following regexp are too complex, we have to do actual match instead

  ccut_test("sb_vm_regexp_compile /[a[^b]]/") {
    Val inner = nb_cons_new(_range('b', 'b'), VAL_NIL);
    Val inner_cg = _struct("BracketCharGroup", 2, (Val[]){VAL_FALSE, inner});
    Val outer = nb_cons_list(2, (Val[]){_range('a', 'a'), inner_cg});
    Val outer_cg = _struct("BracketCharGroup", 2, (Val[]){VAL_TRUE, outer});
    Val regexp = _struct("Regexp", 1, (Val[]){outer_cg});

    void* arena = val_arena_new();
    struct Iseq iseq;
    Iseq.init(&iseq, 5);

    Val err = sb_vm_regexp_compile(&iseq, arena, VAL_NIL, regexp);
    assert_eq(VAL_NIL, err);
    // sb_vm_regexp_decompile(&iseq, 0, Iseq.size(&iseq));

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
    val_arena_delete(arena);
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
  }
}
