#pragma once

// provide minimal JIT utilility
// usage:
//
//   JitMem mem;
//   mini_jit_init(&mem);
//   mini_jit_asm_push(&mem, v);
//   mini_jit_asm_call(&mem, call_some_func);
//   mini_jit_asm_jmp(&mem, jmp_type, label_n);
//   ...
//   mini_jit_ready(&mem, true); // only after this the memory can be executed
//
//   void (*func)();
//   func = mem.data;
//   func();
//
//   mini_jit_cleanup(&mem);

#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#  include <windows.h>
#elif defined(__GNUC__)
#  include <unistd.h>
#  include <sys/mman.h>
#else
#error "not not windows nor gnu C"
#endif

typedef struct {
  void* data;
  int32_t cap;
  int32_t size;

  void** labels;
  int32_t labels_cap;
  int32_t labels_size;
} JitMem;

static void mini_jit_protect(JitMem* mem, bool exec) {
# if defined(_WIN32)
  DWORD old_protect;
  VirtualProtect(mem->data, mem->cap, exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &old_protect);
# else
  mprotect(mem->data, mem->cap, PROT_READ | PROT_WRITE | (exec ? PROT_EXEC : 0));
# endif
}

static void mini_jit_ready(JitMem* mem) {
  // fill jump table
  mini_jit_protect(mem, true);
}

static void mini_jit_init(JitMem* mem) {
  mem->cap = 100;
  mem->size = 0;
  mem->data = malloc(mem->cap);
  mini_jit_protect(mem, true);
}

static void mini_jit_cleanup(JitMem* mem) {
  if (mem->cap) {
    mini_jit_protect(mem, false);
    free(mem->data);
    mem->cap = 0;
    mem->size = 0;
  }
}

asm_call
asm_push
asm_inc
asm_dec
asm_jmp() {
  // if the distance is little
  // else load jump table
}

#pragma mark ## relatively private code

static void mini_jit_ensure_size(JitMem* mem) {
}
