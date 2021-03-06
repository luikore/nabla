#include "val.h"
#include <ccut.h>
#include <stdlib.h>

static ValPair f0() {
  return (ValPair){0, 0};
}

static ValPair f1(Val a1) {
  return (ValPair){a1, 1};
}

static ValPair f2(Val a1, Val a2) {
  return (ValPair){a1 + a2, 2};
}

static ValPair f3(Val a1, Val a2, Val a3) {
  return (ValPair){a1 + a2 + a3, 3};
}

static ValPair f4(Val a1, Val a2, Val a3, Val a4) {
  return (ValPair){a1 + a2 + a3 + a4, 4};
}

static ValPair f5(Val a1, Val a2, Val a3, Val a4, Val a5) {
  return (ValPair){a1 + a2 + a3 + a4 + a5, 5};
}

static ValPair f6(Val a1, Val a2, Val a3, Val a4, Val a5, Val a6) {
  return (ValPair){a1 + a2 + a3 + a4 + a5 + a6, 6};
}

static ValPair f7(Val a1, Val a2, Val a3, Val a4, Val a5, Val a6, Val a7) {
  return (ValPair){a1 + a2 + a3 + a4 + a5 + a6 + a7, 7};
}

static ValPair f8(Val a1, Val a2, Val a3, Val a4, Val a5, Val a6, Val a7, Val a8) {
  // printf("\n%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,\n", a1,a2,a3,a4,a5,a6,a7,a8);
  return (ValPair){(a1<<1) + (a2<<2) + (a3<<3) + (a4<<4) + (a5<<5) + (a6<<6) + (a7<<7) + (a8<<8), 8};
}

void val_suite() {
  ccut_test("rotl and rotr") {
    assert_eq(1239, NB_ROTR(NB_ROTL(1239, 3), 3));
  }

  ccut_test("header size") {
    assert_eq(sizeof(uint64_t), sizeof(ValHeader));
  }

  ccut_test("from and to dbl") {
    assert_true(VAL_DBL_CAN_IMM(3.6), "should be able to be expressed as immediate value");
    assert_true(VAL_DBL_CAN_IMM(-3600.0), "should be able to be expressed as immediate value");
    Val v = VAL_FROM_DBL(3.6);
    assert_true(VAL_IS_DBL(v), "should be double");
    assert_true(3.6 == VAL_TO_DBL(v), "expected 3.6, but got %f", VAL_TO_DBL(v));
  }

  ccut_test("from and to int") {
    assert_true(VAL_INT_CAN_IMM(1LL<<61), "should be able to express 1<<61 as immediate value");
    assert_true(!VAL_INT_CAN_IMM(1ULL<<63), "should not be able to express 1<<63 as immediate value");
    Val v = VAL_FROM_INT(3);
    assert_true(VAL_IS_INT(v), "should be int");
    assert_true(3 == VAL_TO_INT(v), "expected 3, but got %ld", VAL_TO_INT(v));

    v = VAL_FROM_INT(-12);
    assert_true(VAL_IS_INT(v), "should be int");
    assert_true(-12 == VAL_TO_INT(v), "expected -12, but got %ld", VAL_TO_INT(v));
  }

  ccut_test("from and to str") {
    assert_eq(3, VAL_TO_STR(VAL_FROM_STR(3)));
  }

  ccut_test("immediate value test") {
    assert_true(VAL_IS_IMM(VAL_FROM_INT(-12)), "should be immediate value");
    assert_true(VAL_IS_IMM(VAL_FROM_DBL(123.2)), "should be immediate value");

    int64_t a[2] __attribute__ ((aligned (8)));
    a[0] = 0;
    a[1] = 0;
    assert_true(!VAL_IS_IMM((Val)a), "should be pointer");
    assert_true(!VAL_IS_IMM((Val)(a + 1)), "should be pointer");
  }

  ccut_test("truth test") {
    assert_true(VAL_IS_TRUE(VAL_FROM_DBL(3.6)), "should be true");
    assert_true(VAL_IS_TRUE(VAL_FROM_INT(1)), "should be true");
    assert_true(VAL_IS_TRUE(VAL_TRUE), "should be true");
    int64_t a[2] __attribute__ ((aligned (8)));
    a[0] = 0;
    a[1] = 0;
    assert_true(VAL_IS_TRUE((Val)a), "should be true");
    assert_true(VAL_IS_TRUE((Val)(a + 1)), "should be true");

    assert_true(VAL_IS_FALSE(VAL_FALSE), "should be false");
    assert_true(VAL_IS_FALSE(VAL_NIL), "should be false");
  }

  ccut_test("alloc default ref_count == 1") {
    ValHeader* h = val_alloc(KLASS_STRING, 30);
    assert_eq(1, VAL_REF_COUNT((Val)h));
    val_free(h);
  }

  ccut_test("retain/release") {
    ValHeader* h = val_alloc(KLASS_BOOLEAN, 10);
    Val v = (Val)h;
    RETAIN(v);
    assert_eq(2, VAL_REF_COUNT((Val)h));
    RELEASE(v);
    assert_eq(1, VAL_REF_COUNT((Val)h));
    val_free(h);
  }

  ccut_test("val_c_call") {
    Val argv[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ValPair ret;

    ret = val_c_call((void*)f0, 0, argv);
    assert_eq(0, ret.fst);
    assert_eq(0, ret.snd);

    ret = val_c_call((void*)f1, 1, argv);
    assert_eq(1, ret.fst);
    assert_eq(1, ret.snd);

    ret = val_c_call((void*)f2, 2, argv);
    assert_eq(1+2, ret.fst);
    assert_eq(2, ret.snd);

    ret = val_c_call((void*)f3, 3, argv);
    assert_eq(1+2+3, ret.fst);
    assert_eq(3, ret.snd);

    ret = val_c_call((void*)f4, 4, argv);
    assert_eq(1+2+3+4, ret.fst);
    assert_eq(4, ret.snd);

    ret = val_c_call((void*)f5, 5, argv);
    assert_eq(1+2+3+4+5, ret.fst);
    assert_eq(5, ret.snd);

    ret = val_c_call((void*)f6, 6, argv);
    assert_eq(1+2+3+4+5+6, ret.fst);
    assert_eq(6, ret.snd);

    ret = val_c_call((void*)f7, 7, argv);
    assert_eq(1+2+3+4+5+6+7, ret.fst);
    assert_eq(7, ret.snd);

    ret = val_c_call((void*)f8, 8, argv);
    assert_eq((1<<1)+(2<<2)+(3<<3)+(4<<4)+(5<<5)+(6<<6)+(7<<7)+(8<<8), ret.fst);
    assert_eq(8, ret.snd);
  }

  ccut_test("val_c_call2") {
    Val argv[] = { 2, 3, 4, 5, 6, 7, 8 };
    ValPair ret;

    ret = val_c_call2(1, (void*)f1, 0, argv);
    assert_eq(1, ret.fst);
    assert_eq(1, ret.snd);

    ret = val_c_call2(1, (void*)f2, 1, argv);
    assert_eq(1+2, ret.fst);
    assert_eq(2, ret.snd);

    ret = val_c_call2(1, (void*)f3, 2, argv);
    assert_eq(1+2+3, ret.fst);
    assert_eq(3, ret.snd);

    ret = val_c_call2(1, (void*)f4, 3, argv);
    assert_eq(1+2+3+4, ret.fst);
    assert_eq(4, ret.snd);

    ret = val_c_call2(1, (void*)f5, 4, argv);
    assert_eq(1+2+3+4+5, ret.fst);
    assert_eq(5, ret.snd);

    ret = val_c_call2(1, (void*)f6, 5, argv);
    assert_eq(1+2+3+4+5+6, ret.fst);
    assert_eq(6, ret.snd);

    ret = val_c_call2(1, (void*)f7, 6, argv);
    assert_eq(1+2+3+4+5+6+7, ret.fst);
    assert_eq(7, ret.snd);

    ret = val_c_call2(1, (void*)f8, 7, argv);
    assert_eq((1<<1)+(2<<2)+(3<<3)+(4<<4)+(5<<5)+(6<<6)+(7<<7)+(8<<8), ret.fst);
    assert_eq(8, ret.snd);
  }
}
