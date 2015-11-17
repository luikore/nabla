(TODO) concept

optimization should consider:

- reloading
- dynamic semantics (methods can be re-defined)
- debugging

## iterating

if iter body is lambda, then no need to trap break/next/return

if iter body is sub, and the array is large, build a tempral byte code then run it

    label:
      getnext
      ... inlined byte code, replacing longjmp with jmp
      push should_cont
      jmpif label

## break and return impl

C level context preserving is still required in the cases where we can't optimize the block.

## object new literal

The `Data{}` syntax is final, so each key can be mapped to a field index

    obj_new klass # pushes obj
    set_field 1 local5
    set_field 3 local2

but reloading may change data layout, then field indices no longer works. and 

      obj_new klass
      jmp_vm_version label1
      # opt set field
      jmp label2
    label1:
      # normal set field
    label2:
      ... rest

for the hash way `Data{**hash}`, no need genenerate the opt branch

## immutability

Very few locks are required

Refcount based in-place update -- sadly other thread may inc the refcount, then a lock is required

## chaining (scope, []=)

Use stack objects

## vector

## refcount

## perfect hashing

For normal lookups, use traditional hashing

When something can be compiled (like parser with a set of node definition), may use http://cmph.sourceforge.net/ for generating the perfect hashes
