#pragma once

typedef struct {
  uint16_t op;
  int32_t arg1;
} __attribute__((packed)) Arg32;

typedef struct {
  uint16_t op;
  uint32_t arg1;
} __attribute__((packed)) ArgU32;

typedef struct {
  uint16_t op;
  uint32_t arg1;
  uint32_t arg2;
} __attribute__((packed)) ArgU32U32;

typedef struct {
  uint16_t op;
  Val arg1;
} __attribute__((packed)) ArgVal;

typedef struct {
  uint16_t op;
  int32_t arg1, arg2;
} __attribute__((packed)) Arg3232;

typedef struct {
  uint16_t op;
  int32_t arg1, arg2, arg3;
} __attribute__((packed)) Arg323232;

typedef struct {
  uint16_t op;
  int16_t arg1;
} __attribute__((packed)) Arg16;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define AS_ARG32(c) c, 0
#define SPLIT_ARG32(x) ((x) & 0xFFFFU), ((x) >> 16)
#else
#define AS_ARG32(c) 0, c
#define SPLIT_ARG32(x) ((x) >> 16), ((x) & 0xFFFFU)
#endif
