local env is a hash-like object

## gc

incremental tracing

see also gc_ptr.md

## runtime introspection

    Kernel::GC
    Kernel::Lang

how powerful should it be?

## leveled runtime

the primitive should be abled to change

## mutual recursion detection

just an idea:

represent last n tail_call methods in a bitmap
in a tail\_call instruction, find method, then detect if it is in the bitmap, if yes, apply tail call stack rewrite?

to read: A First-Order One-Pass CPS Transformation

## JIT inlining trick

just an idea:

append the function being inlined, then rewrite longjmp-call to localmp-call

## tag pointer design

*double centric*

for computation-heavy jobs

*pointer centric + double*

just rotated version of double centric design

*pointer centric + mini string*

decimal/double are heap objs, should be good for servers

---

every pointer obj has `isa`, which points to the prototype

but value object doesn't have one

there's a special value object that represents `Object` class (todo consider the chain)

# the beneficial on GC

GC doesn't care if an object is mutable, it cares when they are mutated

so we can inform GC on every mutating actions!
