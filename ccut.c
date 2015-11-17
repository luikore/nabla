/**
Copyright (c) 2013, 2014, http://github.com/luikore/ccut
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of the {organization} nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include "ccut.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

// for assert trace, TODO
// ref https://gist.github.com/jvranish/4441299 for windows support
#ifndef _WIN32
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

typedef struct {
  int runned_tests_cap;
  int runned_tests_size;
  const char** runned_tests;

  const char* current_name; // name of current test
  int current_state;  // 0: success, 1: pending, 2: failure

  int success_size;
  int failure_size;
  int pending_size;
  int assertion_size;
} CUTContext;

static CUTContext ctx = {
  0, 0, NULL,
  NULL, 0,
  0, 0, 0, 0
};

#ifndef _WIN32
static void print_trace_handler(int sig) {
  void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  _Exit(1);
}
#endif

void ccut_print_trace_on(int sig) {
# ifndef _WIN32
  signal(sig, print_trace_handler);
# else
  fprintf(stderr, "ccut_trap_on is only available on POSIX systems\n");
# endif
}

void __ccut_run_suite(const char* sname, void (*s)()) {
  printf("\n\e[38;5;6m%s\e[38;5;7m", sname);

  ctx.runned_tests_size = 0;
  ctx.runned_tests_cap = 8;
  ctx.runned_tests = malloc(sizeof(char*) * ctx.runned_tests_cap);

  ctx.current_name = NULL;
  ctx.current_state = 0;
  while (1) {
    int last_tests_size = ctx.runned_tests_size;
    s();
    if (last_tests_size == ctx.runned_tests_size) {
      // last success
      if (ctx.runned_tests_size && !ctx.current_state) {
        printf("\e[38;5;2mSuccess\e[38;5;7m");
        fflush(stdout);
        ctx.success_size++;
      }
      break;
    }
  }

  free(ctx.runned_tests);
  ctx.runned_tests_size = 0;
  ctx.runned_tests_cap = 0;
  ctx.runned_tests = NULL;
}

void __ccut_print_stats() {
  int all = ctx.success_size + ctx.failure_size + ctx.pending_size;
  printf("\n\n%d tests, %d success, %d failure, %d pending\n%d assertions passed\n",
    all, ctx.success_size, ctx.failure_size, ctx.pending_size, ctx.assertion_size
  );
}

static void add_runned_test(const char* c) {
  ctx.current_name = c;
  printf("\n  %s ", c);
  if (ctx.runned_tests_size >= ctx.runned_tests_cap) {
    ctx.runned_tests_cap *= 2;
    ctx.runned_tests = realloc(ctx.runned_tests, sizeof(char*) * ctx.runned_tests_cap);
  }
  ctx.runned_tests[ctx.runned_tests_size++] = c;
}

static int test_runned(const char* c) {
  for (int i = 0; i < ctx.runned_tests_size; i++) {
    if (strcmp(ctx.runned_tests[i], c) == 0) {
      return 1;
    }
  }
  return 0;
}

int __ccut_dispatch(const char* c) {
  if (!ctx.runned_tests) {
    fprintf(stderr, "Please use ccut_run_suite(suite_func) to run the tests");
    _Exit(1);
  }

  if (test_runned(c)) {
    return 0;
  }

  // no current_name, we are clean to start first run of the test
  if (!ctx.current_name) {
    add_runned_test(c);
    return 1;
  }

  // c != current_name, last test finished, start new test
  if (strcmp(ctx.current_name, c)) {
    if (!ctx.current_state) {
      printf("\e[38;5;2mSuccess\e[38;5;7m");
      fflush(stdout);
      ctx.success_size++;
    }
    add_runned_test(c);
    return 1;
  }

  // c == current_name, last test should have been failed
  if (ctx.current_state != 1 && ctx.current_state != 2) {
    fprintf(stderr, "return called from %s", ctx.current_name);
  }
  return 0;
}

int __ccut_pending(int line) {
  printf("\e[38;5;3m%d: Pending\e[38;5;7m", line);
  fflush(stdout);
  ctx.pending_size++;
  ctx.current_state = 1;
  return 1;
}

static void fail_before(int line) {
  printf("\e[38;5;1m%d: ", line);
  ctx.failure_size++;
  ctx.current_state = 2;
}

static void fail_after() {
  printf("\e[38;5;7m ");
}

static void inc_assertion_size() {
  printf("\e[38;5;2m.\e[38;5;7m");
  fflush(stdout);
  ctx.assertion_size++;
}

int __ccut_fail(int line) {
  fail_before(line);
  printf("Failure");
  fail_after();
  fflush(stdout);
  return 1;
}

int __ccut_assert_true(int line, int expr, const char* fmt, ...) {
  if (expr) {
    inc_assertion_size();
    return 0;
  } else {
    va_list args;
    fail_before(line);
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fail_after();
    return 1;
  }
}

int __ccut_assert_ll_eq(int line, long long expected, long long actual) {
  return __ccut_assert_true(line, expected == actual, "Expected %lld, but got %lld", expected, actual);
}

int __ccut_assert_ll_neq(int line, long long expected, long long actual) {
  return __ccut_assert_true(line, expected != actual, "Expected not %lld", actual);
}

int __ccut_assert_ptr_eq(int line, const void* expected, const void* actual) {
  return __ccut_assert_true(line, expected == actual, "Expected %p, but got %p", expected, actual);
}

int __ccut_assert_ptr_neq(int line, const void* expected, const void* actual) {
  return __ccut_assert_true(line, expected != actual, "Expected not %p", actual);
}

int __ccut_assert_str_eq(int line, const char* expected, const char* actual) {
  if (!expected && !actual) {
    return 0;
  } else if (expected && !actual) {
    fail_before(line);
    printf("Expected string %s, but got NULL", expected);
    fail_after();
    return 1;
  } else if (!expected && actual) {
    fail_before(line);
    printf("Expected NULL, but got string %s", actual);
    fail_after();
    return 1;
  }
  return __ccut_assert_true(line, strcmp(expected, actual) == 0,
    "Expected string %s (%lu), but got %s (%lu)", expected, strlen(expected), actual, strlen(actual));
}

int __ccut_assert_str_neq(int line, const char* expected, const char* actual) {
  if (!expected && !actual) {
    fail_before(line);
    printf("Expected string to be not NULL");
    fail_after();
    return 1;
  } else if (expected && !actual) {
    return 0;
  } else if (!expected && actual) {
    return 0;
  }
  return __ccut_assert_true(line, strcmp(expected, actual) != 0,
    "Expected string not equals to %s (%lu)", expected, strlen(expected));
}

int __ccut_assert_ull_eq(int line, unsigned long long expected, unsigned long long actual) {
  return __ccut_assert_true(line, expected == actual, "Expected %llu, but got %llu", expected, actual);
}

int __ccut_assert_ull_neq(int line, unsigned long long expected, unsigned long long actual) {
  return __ccut_assert_true(line, expected != actual, "Expected not %llu", expected);
}

int __ccut_assert_mem_eq(int line, const void* expected, const void* actual, size_t size) {
  // todo: print some bytes?
  return __ccut_assert_true(line, memcmp(expected, actual, size) == 0,
    "Expected content of memory %p equals %p (%llu bytes)", actual, expected, size);
}

int __ccut_assert_mem_neq(int line, const void* expected, const void* actual, size_t size) {
  // todo: print some bytes?
  return __ccut_assert_true(line, memcmp(expected, actual, size) != 0,
    "Expected content of memory %p not equals %p (%llu bytes)", actual, expected, size);
}

int __ccut_assert_eps_eq(int line, long double expected, long double actual, long double eps) {
  eps = fabsl(eps);
  return __ccut_assert_true(line, (actual < expected + eps) && (actual > expected - eps),
    "Expected %llf (\xc2\xb1 %llf), but got %llf", expected, eps, actual);
}

int __ccut_assert_eps_neq(int line, long double expected, long double actual, long double eps) {
  eps = fabsl(eps);
  return __ccut_assert_true(line, (actual >= expected + eps) || (actual <= expected - eps),
    "Expected not the same as %llf (\xc2\xb1 %llf), but got %llf within", expected, eps, actual);
}
