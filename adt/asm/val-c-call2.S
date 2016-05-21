// NOTE may #include some macro here too
// NOTE must use upper case S
.file "val_c_call2.S"
.text
.globl _val_c_call2

// Mark stack as non-executable
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack, "", @progbits
#endif


// align code and fill with nop
.align 4, 0x90

// .type val_c_call2, @function // for linux

// see reference of val_c_call
_val_c_call2:

  .cfi_startproc // allow debugger/exception handler to unwind stack
  pushq %rbp
  .cfi_def_cfa_offset 16 // cfa = cfa_register + 16
  movq %rsp, %rbp

  .cfi_def_cfa_register rbp

  pushq %r12 // for tmp compute
  pushq %rbx // rbx stores stack offset

  // clear it before cmp -> jump operation
  xorq %rbx, %rbx

#ifndef _WIN32
  // System V: RDI, RSI, RDX, RCX, R8, R9
  // first arg %rdi is passed down directly
  movq %rsi, %r11 // cfunc
  movq %rdx, %r10 // argc
  movq %rcx, %rax // argv

  // first 6 args
  // NOTE do not leak registers for safety
  cmpq $0, %r10
  jle args_end

  movq 0(%rax), %rsi
  decq %r10
  jz args_end

  movq 8(%rax), %rdx
  decq %r10
  jz args_end

  movq 16(%rax), %rcx
  decq %r10
  jz args_end

  movq 24(%rax), %r8
  decq %r10
  jz args_end

  movq 32(%rax), %r9
  decq %r10
  jz args_end

  addq $32, %rax
#else
  // Windows: RCX, RDX, R8, R9
  // first arg %rcx is passed down directly
  movq %rdx, %r11 // cfunc
  movq %r8, %r10  // argc
  movq %r9, %rax  // argv

  // first 4 args
  cmpq $0, %r10
  jle args_end

  movq 0(%rax), %rdx
  decq %r10
  jz args_end

  movq 8(%rax), %r8
  decq %r10
  jz args_end

  movq 16(%rax), %r9
  decq %r10
  jz args_end

  addq $16, %rax
#endif

  // %rax += %r10 * 8
  movq %r10, %r12
  shlq $3, %r12
  addq %r12, %rax

  // push other args, from right to left
loop_push_args:
  pushq 0(%rax)
  subq $8, %rax
  addq $8, %rbx
  decq %r10
  jnz loop_push_args

args_end:

#ifndef _WIN32
  callq *%r11
#else
  // shadow space for rcx, rdx, r8, r9
  subq $32, %rsp
  callq *%r11
  addq $32, %rsp
#endif

  addq %rbx, %rsp

  popq %rbx
  popq %r12

  // epilogure
  popq %rbp
  .cfi_def_cfa_register rsp // restore default cfa register
  retq
  .cfi_endproc
