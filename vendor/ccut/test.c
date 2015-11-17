#include "ccut.h"
#include <stdio.h>
#include <assert.h>

void success_suite() {

  ccut_test(assert_true) {
    assert_true(2 == 2, "true");
    assert_true(3, "true");
  }

  ccut_test(assert_false) {
    assert_false(1 == 2, "false");
    assert_false(0, "false");
  }

  ccut_test(assert_eq) {
    assert_eq(1, 1);
    int p;
    assert_eq((void*)&p, (void*)&p);
  }

  ccut_test(assert_neq) {
    assert_neq(1, 2);
    int p, q;
    assert_neq((void*)&p, (void*)&q);
  }

  ccut_test(assert_str_eq) {
    char p[] = "first string";
    char q[] = "first string";
    assert_str_eq(p, q);
    assert_neq((void*)p, (void*)q);
  }

  ccut_test(assert_str_neq) {
    char p[] = "first string";
    char q[] = "first strang";
    assert_str_neq(p, q);
  }

  ccut_test(assert_ull_eq) {
    assert_ull_eq(1ULL << 62, 1ULL << 62);
  }

  ccut_test(assert_ull_neq) {
    assert_ull_neq((1ULL << 63) | 3, 3);
  }

  ccut_test(assert_mem_eq) {
    char p[] = "first string";
    char q[] = "first strang";
    assert_mem_eq(p, q, 9);
    assert_mem_eq((int*)p, (int*)q, 9);
  }

  ccut_test(assert_mem_neq) {
    char p[] = "first string";
    char q[] = "first strang";
    assert_mem_neq(p, q, 10);
  }

  ccut_test(assert_eps_eq) {
    assert_eps_eq(1.5, 1.501, 0.01);
  }

  ccut_test(assert_eps_neq) {
    assert_eps_neq(1.5, 1.51, 0.0001);
  }
}

void unsuccess_suite() {

  ccut_test(fail) {
    fail;
  }

  ccut_test(pending) {
    pending;
  }
}

void assert_failure_func() {
  assert(0);
}

void assert_failure_suite() {
  assert_failure_func();
}

int main (int argc, char const *argv[]) {
  printf("\n\n===== the following tests should pass =====\n");
  ccut_run_suite(success_suite);

  printf("\n\n===== the following tests should not pass =\n");
  ccut_run_suite(unsuccess_suite);

  ccut_print_stats();

  ccut_print_trace_on(SIGABRT);
  printf("\n\n===== the following tests should print stack trace of assert_failure_suite\n\n");
  assert_failure_suite();

  return 0;
}
