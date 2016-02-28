#include <stdint.h>

#define UTF_8_MAX 0x7FFFFFFF

// assume all the bytes can be consumed.
// return -2 if invalid byte sequence (byte 2-6 not starting with 10)
static int32_t utf_8_fast_scan(const unsigned char* s, int32_t bytes) {
  // complement of leading_masks
  static const unsigned char masks[] = {0,  0b01111111, 0b00111111, 0b00011111, 0b00001111, 0b00000111, 0b00000011};

  int32_t c = (int32_t)(s[0] & masks[bytes]);
  for (size_t i = 1; i < bytes; i++) {
    if ((s[i] >> 6) != 0b10) {
      return -2;
    }
    c = (c << 6) | (s[i] & 0b00111111);
  }
  return c;
}

// return -1 if truncated char
// return -2 if invalid char
// `size` as input is the limit of bytes
// `size` as output is the scanned bytes
static int32_t utf_8_scan(const char* signed_s, int32_t* size) {
  static const unsigned char leading_masks[] = {0, 0b0, 0b11000000, 0b11100000, 0b11110000, 0b11111000, 0b11111100};

  const unsigned char* s = (const unsigned char*)signed_s;
  if (*size < 1) {
    return -1;
  }

  for (int32_t i = 6; i >= 1; i--) {
    if ((s[0] & leading_masks[i]) == leading_masks[i]) {
      if (i > *size) {
        return -1;
      }

      int32_t res = utf_8_fast_scan(s, i);
      if (res >= 0) {
        *size = i;
      }
      return res;
    }
  }
  NB_UNREACHABLE();
}

// return -1 if truncated char
// return -2 if invalid char
// `size` as input is the limit of bytes
// `size` as output is the scanned bytes
static int32_t utf_8_scan_back(const char* signed_s, int32_t* size) {
  static const unsigned char leading_masks[] = {0, 0b0, 0b11000000, 0b11100000, 0b11110000, 0b11111000, 0b11111100};

  const unsigned char* s = (const unsigned char*)signed_s;
  if (*size < 1) {
    return -1;
  }

  for (int32_t i = (*size < 6 ? *size : 6); i >= 1; i--) {
    for (int32_t j = 6; j > i; j--) {
      if ((s[-i] & leading_masks[j]) == leading_masks[j]) {
        return -1;
      }
    }
    if ((s[-i] & leading_masks[i]) == leading_masks[i]) {
      int32_t res = utf_8_fast_scan(s - i, i);
      if (res >= 0) {
        *size = i;
      }
      return res;
    }
  }
  return -1;
}

// return bytes required to encode the char
static int32_t utf_8_calc(int32_t c) {
  if (c < 0x80) {
    return 1;
  } else if (c < 0x0800) {
    return 2;
  } else if (c < 0x10000) {
    return 3;
  } else if (c < 0x200000) {
    return 4;
  } else if (c < 0x4000000) {
    return 5;
  } else {
    return 6;
  }
}

// pos is increased
// return bytes appended
static int32_t utf_8_append(char* signed_s, int64_t pos, int32_t c) {
  unsigned char* s = (unsigned char*)signed_s;
# define MASK_C(rshift) (((c >> rshift) & 0b00111111) | 0b10000000)
  if (c < 0x80) {
    s[pos++] = c;
    return 1;
  } else if (c < 0x0800) {
    s[pos++] = (0b11000000 | (c >> 6));
    s[pos++] = MASK_C(0);
    return 2;
  } else if (c < 0x10000) {
    s[pos++] = (0b11100000 | (c >> 12));
    s[pos++] = MASK_C(6);
    s[pos++] = MASK_C(0);
    return 3;
  } else if (c < 0x200000) {
    s[pos++] = (0b11110000 | (c >> 18));
    s[pos++] = MASK_C(12);
    s[pos++] = MASK_C(6);
    s[pos++] = MASK_C(0);
    return 4;
  } else if (c < 0x4000000) {
    s[pos++] = (0b11111000 | (c >> 24));
    s[pos++] = MASK_C(18);
    s[pos++] = MASK_C(12);
    s[pos++] = MASK_C(6);
    s[pos++] = MASK_C(0);
    return 5;
  } else {
    s[pos++] = (0b11111100 | (c >> 30));
    s[pos++] = MASK_C(24);
    s[pos++] = MASK_C(18);
    s[pos++] = MASK_C(12);
    s[pos++] = MASK_C(6);
    s[pos++] = MASK_C(0);
    return 6;
  }
# undef MASK_C
}
