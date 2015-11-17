#pragma once

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

inline static void print_backtrace() {
  void *array[15];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 15);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
}
