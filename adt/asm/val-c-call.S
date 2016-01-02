// NOTE may #include some macro here too
// NOTE must use upper case S
.file "val_c_call.S"
.text
.globl _val_c_call

// align code and fill with nop
.align 4, 0x90

// .type val_c_call, @function // for linux

// overview reference: http://agner.org/optimize/calling_conventions.pdf
// TODO add ARM64 http://eli.thegreenplace.net/2011/09/06/stack-frame-layout-on-x86-64/
_val_c_call:

  // prologue: save stack base
  // http://x86-64.org/documentation/abi.pdf from page 100
  // cfi = call frame info
  // cfa = canonical frame address
  .cfi_startproc // allow debugger/exception handler to unwind stack
  pushq %rbp
  .cfi_def_cfa_offset 16 // cfa = cfa_register + 16
  movq %rsp, %rbp

  // tell debugger: use rbp instead of rsp as base call frame address
  // cfa register has to be aligned by 16 bytes, but push/pop operations on rsp breaks the constraint
  .cfi_def_cfa_register rbp

  // callee saves r12-r15 for both win32 and posix
  // in posix, callee should also save rbx
  pushq %r12 // for tmp compute
  pushq %rbx // rbx stores stack offset

  // clear it before cmp -> jump operation
  xorq %rbx, %rbx

#ifndef _WIN32
  // System V: RDI, RSI, RDX, RCX, R8, R9 -> RAX, RDX
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
  // Windows: RCX, RDX, R8, R9 -> RAX
  // TODO paired return (add return address as first arg), check if shadow space works
  // NOTE R10, R11 are preserved by caller, they also be used in syscall/sysret instructions
  //      luckily we don't use them after call
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

  // NOTE don't touch rax & rdx after call

  // restore stack base
  addq %rbx, %rsp

  // restore rbx & r12
  popq %rbx
  popq %r12

  // epilogure
  popq %rbp
  .cfi_def_cfa_register rsp // restore default cfa register
  retq
  .cfi_endproc
