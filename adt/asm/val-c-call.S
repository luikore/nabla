// NOTE may #include some macro here too
// NOTE must use upper case S
.file "val_c_call.S"
.text
.globl _val_c_call
.align 16
// .type c_invoke, @function // for linux

_val_c_call:

  // prelude: save stack base
  // NOTE in mac the address in stack register must 16-byte aligned after add/sub %rsp
  // cfi = call frame info
  // cfa = canonical frame address
  .cfi_startproc // allow debugger/exception handler to unwind stack
  pushq %rbp
  .cfi_def_cfa_offset 16
  movq %rsp, %rbp

  // callee saves r12-r15
  // NOTE don't touch prelude, any change results in segfault...
  pushq %r12
  pushq %r12 // for alignment

  // System V: RDI, RSI, RDX, RCX, R8, R9
  // TODO windows calling convention
  movq %rsi, %r10 // argc
  movq %rdi, %r11 // cfunc
  movq %rdx, %rax // argv

  // save stack base
  // pushq %rbp
  // pushq %r12
  // movq %rsp, %rbp

  // first 6 args
  // TODO safety: do not change the registers when argc is less than 6
  movq 0(%rax), %rdi
  movq 8(%rax), %rsi
  movq 16(%rax), %rdx
  movq 24(%rax), %rcx
  movq 32(%rax), %r8
  movq 40(%rax), %r9
  // r12 to compute the stack offset
  // clear it before cmp -> jump operation
  xorq %r12, %r12
  cmpq $6, %r10
  jle loop_end

  // argv += argc*8
  pushq %r10
  shlq $3, %r10
  addq %r10, %rax
  popq %r10

  // push other args
loop_push_args:
  subq $8, %rax
  addq $8, %r12
  pushq 0(%rax)
  decq %r10
  cmpq $6, %r10
  jg loop_push_args
loop_end:

  callq *%r11

  // restore stack base
  addq %r12, %rsp

  // restore r12
  popq %r12
  popq %r12

  // finale
  popq %rbp
  retq
  .cfi_endproc
