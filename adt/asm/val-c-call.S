// NOTE may #include some macro here too
// NOTE must use upper case S
.file "val_c_call.S"
.text
.globl _val_c_call
.align 16
// .type c_invoke, @function // for linux

_val_c_call:

  // prologue: save stack base
  // TODO windows https://msdn.microsoft.com/en-us/library/tawsa7cb.aspx
  // NOTE in mac the address in stack register must 16-byte aligned after add/sub %rsp
  // cfi = call frame info
  // cfa = canonical frame address
  .cfi_startproc // allow debugger/exception handler to unwind stack
  pushq %rbp
  .cfi_def_cfa_offset 16
  movq %rsp, %rbp

  // callee saves r12-r15 for both win32 and posix
  // NOTE don't touch prelude, any change results in segfault...
  pushq %r12
  pushq %r12 // for alignment
  // r12 is for computing the stack offset
  // clear it before cmp -> jump operation
  xorq %r12, %r12

#ifndef _WIN32
  // System V: RDI, RSI, RDX, RCX, R8, R9
  movq %rdi, %r11 // cfunc
  movq %rsi, %r10 // argc
  movq %rdx, %rax // argv

  // first 6 args
  // NOTE do not leak registers for safety
  cmpq $0, %r10
  jle args_end

  movq 0(%rax), %rdi
  decq %r10
  jz args_end

  movq 8(%rax), %rsi
  decq %r10
  jz args_end

  movq 16(%rax), %rdx
  decq %r10
  jz args_end

  movq 24(%rax), %rcx
  decq %r10
  jz args_end

  movq 32(%rax), %r8
  decq %r10
  jz args_end

  movq 40(%rax), %r9
  decq %r10
  jz args_end

  addq $40, %rax
#else
  // Windows: RCX, RDX, R8, R9
  movq %rcx, %r11 // cfunc
  movq %rdx, %r10 // argc
  movq %r8, %rax  // argv

  // first 4 args
  cmpq $0, %r10
  jle args_end

  movq 0(%rax), %rcx
  decq %r10
  jz args_end

  movq 8(%rax), %rdx
  decq %r10
  jz args_end

  movq 16(%rax), %r8
  decq %r10
  jz args_end

  movq 24(%rax), %r9
  decq %r10
  jz args_end

  addq $24, %rax
#endif

  // push other args
loop_push_args:
  addq $8, %rax
  pushq 0(%rax)
  addq $8, %r12
  decq %r10
  jnz loop_push_args

args_end:

  callq *%r11

  // restore stack base
  addq %r12, %rsp

  // restore r12
  popq %r12
  popq %r12

  // epilogure
  popq %rbp
  retq
  .cfi_endproc
