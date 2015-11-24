#include "siphash.h"
#include "siphash_impl.h"

#ifdef __SSE3__
# include "siphash_ssse3.c"
#else
# ifdef __SSE2__
#   include "siphash_sse2.c"
# else
#   include "siphash.c"
# endif
#endif
