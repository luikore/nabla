#pragma once

#include <stdint.h>
#include <stdio.h>

#pragma mark # pop count

// en.wikipedia.org/wiki/Hamming_weight

// for msvc, see http://msdn.microsoft.com/en-us/library/bb385231%28v=vs.90%29.aspx
// but useless because it can not compile C99 code, not to mention C11

#define NB_POPCNT __builtin_popcountll

// build with -march=native will make use of single instruction of SSE4.2 popcnt or NEON vcnt

#pragma mark # rotate

// gcc/clang knows rotate code, and replace them with rotl and rotr instructions

static inline uint64_t NB_ROTL(uint64_t v, unsigned n) {
  return (v << n) | (v >> (64 - n));
}

static inline uint64_t NB_ROTR(uint64_t v, unsigned n) {
  return (v >> n) | (v << (64 - n));
}

// for non-gcc/clang compilers, should try C11 _Noreturn
#define NB_UNREACHABLE __builtin_unreachable

#pragma mark # bytecode optimization

#define NB_LIKELY(x) ({__builtin_expect(!(x), 0); x;})

#define NB_UNLIKELY(x) ({__builtin_expect((x), 0); x;})

// to prefetch code in the address
// https://gcc.gnu.org/onlinedocs/gcc-3.3/gcc/Other-Builtins.html
#define NB_PREFETCH __builtin_prefetch
