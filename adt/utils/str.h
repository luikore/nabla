#pragma once

// string utils

#include <stdbool.h>

// NOTE strncmp not platform compable and not able to handle 2 sizes
static int str_compare(int size1, const char* s1, int size2, const char* s2) {
  for (int i = 0; i < size1 && i < size2; i++) {
    if (s1[i] > s2[i]) {
      return 1;
    } else if (s1[i] < s2[i]) {
      return -1;
    }
  }
  return (size1 > size2) ? 1 : (size1 < size2) ? -1 : 0;
}

static bool str_is_prefix(int size1, const char* s1, int size2, const char* s2) {
  if (size1 > size2) {
    return false;
  }
  return str_compare(size1, s1, size1, s2) == 0;
}
