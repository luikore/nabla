
## misc

top level is `class Kernel`

web page: display the parser tree, spinning to impact them!

soundness proof of the syntax:

    learn Coq Proof Assistant
    read http://www.cis.upenn.edu/~bcpierce/sf/ software foundations
    read http://adam.chlipala.net/cpdt/ certified programing with dependent types

# runtime

## wrapped require

    class X
    :require_wrapped 'a.nb' X

then newly created constants during loading will be created inside X

to make less conflicts, `X` is better a new class

## fixing other people's code

you can, easily. but only when the method / class is not final, and constants can only be mutated but not replaced

## coroutines (fibers)

exception implemented with coroutine

to emulate a `try` block

    Fiber.new \cc # cc is the continuation channel
      ...
      :call some that yields
      yield hello world
      ...
    switch .resume
    case <pattern>
    case <pattern>
      $=.trace
      $=.resume
    case e    # matches everything
      yield e # re-yield the error
    ensure
      # always happens, even if there is non-local jump inside the case statements

`$=` is the last yielded coroutine object, can resume it maybe. we can also get backtrace from it.

`$#` is number of times last fiber block that has executed, which can be useful for redo strategy or other purposes.

fiber methods

    f.resume # in current thread
    f.fork   # run in concurrent manner
    f.join   # wait for the fiber to complete
    f.send   # async send message to the cc channel
    f.call   # synchronous send message to the cc channel, alias of resume

cc methods

    cc.recv   # get message
    cc.reply  # send message back to the caller fiber
    cc.yield  # in current fiber, there is a syntax sugar `yield`
    cc.send   # note no call in here

we can schedule at the yield points of coroutines

`ensure` always runs, even there's an uncaught exception

`switch` without following clauses should be syntax error

tree-stack can be reaped

## channels

fibers communicate through channels, in channel read/write, fiber gives control to the scheduler

# parsing

the first working interpreter can use bison and an indent-powered context-sensitive lexer

operator token has precedence attached

# concurrency - all objects are immutable, STM is not required maybe?

these method names indicates the object is changed:

    def init!
    def meth!
    def x=
    def [k]= v

in the method it can just change ivar or array members, but when calling them, it will create a new object then assign to the var

    a = [1,2,3]
    b = a
    c.do_sth a # c can mut a, but it is wrapped in block and ok
    a.push! 4  # now a is different from b, or, it is the same as `a = a.concat 4`

in compile-time, program can do escape analysis, and replace some of the consecutive calls with optimized initializers.

when you need to mutate the instance a variable points to instead of making a new shadow variable, use the pointer method operator `->`

    a->push! 4 # a is still the same array, but content changed

note: a tm with pointer ref changer is automatically wrapped with context lock

    \, tm, a <- 1
    # is in fact this pseudo code
    \, tm $local_context, a <- 1

if a dead-lock error involves some local context, positions of the lambdas should be pointed out.

    a = 3
    def foo x
      tm, x->[]= 0 0 # duplicates
    foo a # 2
    a     # 3

to avoid `write skew`, you should lock related objects, and it will generate an error on failure

    tm a b
      a->push! 4
      b->pop! 4

it also generates compiler error when replacing pointer to the locked objects

    tm a
      a->push! 4
      a.push! 4  # error: can not replace locked object, use `a->push!` instead

nested transaction? should see how clojure does.

or to force synchronized lock, which will lock the whole VM

    sync
      ...

a note on `init!`: it locks the object being initialized if `self` escapes. there should be a compiler warning for this

tm blocks can pattern match the yield results and do `redo` -- you can control the redo behavior to avoid resource exortion

    tm
      ...
      # persistent changes in the end
    case DeadlockError # there can be other errors stopping the transaction, but only DeadlockError is raised by tm
      # log or do sth else...
      if $# < 3 # see also the `try... case` section
        redo # pass the control flow back

you can use infinite redo with [exponential backoff](en.wikipedia.org/wiki/Exponential_backoff)

    tm
      ...
    case DeadlockError
      redo {after: 2 ** $#}

you can make one of the servers stat transaction failure and help scalability.

example use with database transactions

    tm
      :begin_transaction
      ...
      :commit_transaction
    case DeadlockError
      :rollback_transaction
      redo
    case TransactionError
      redo

example use with databases with atomic updates

    tm
      ...
      :atomic_update_your_doc
    case t as DeadlockError
      redo

shared memory on multiple processes are not recommended, there will be race conditions if tm on them. should use an actor model instead. however, it is possible to implement a distributed lock with remote channels, and register the channel for serial locks.

## taking the advantage of immutability

    b = Seq.new 2 0
    a = 2.times.map \, b
    a[0] = 3 # [[3, 0], 0, 0] # everything is value-like! yay!

## on IO and GUI objects

a GUI object can be programed as an IO-like object

    $my_gui_io # this macro replaces -> calls with IO actions
      h = o->height
      o->height = h + 12

which is equivalent to

    h = o.read 'height'
    o.write 'height' h + 12

since io actions can not be `redo`, when you need to maintain some consistency between several objects, transactions should be replaced with `sync`.

note this operation doesn't change object's internal state in nabla (address to gui toolkit object, attributes, ... etc), but makes difference to the world.

note file reads are not like GUI objects, so just use

    File.read 'file'

and it's done

## remote actor channel/message protocol

websocket protocol on top of socket/unix socket. so the actor can communicate with web requests.

# reflections

objects are rich

    obj.class
    obj.hash_code # wraps result of def_hash, can be re-defined but not affecting the obj as map key
    obj.==        # wraps result of def_eql, can be re-defined but not affecting the obj as map key, != is defined on this method
    obj.match?    # default impl: alias of ==
    obj.inspect
    obj.respond_to?
    obj.call m *args

    a.present?
    a.presence
    a.blank?

    obj.iv_get k
    obj.iv_set! k v
    obj.methods
    obj.object_id
    obj.method m

    obj.def! m lambda
    obj.extend! module

limited dynamic dispatching

    # for normal method dispatch
    def . m *args
      ...

    # for mutator dispatch (m always end with `!` or `=`)
    def -> m *args
      ...

they are called only when a method can not be found on the class or its ancestors

methods can be undefined with `undef` or replaced (just def one with the same name again)

to finalize methods to forbid re-def (raise error on re-def):

    final def foo
      ...

to forbid include or re-open of a class

    final class Foo
      ...

if a child class re-def the method, it will raise an error

## method arity

method arity is always limited to `const_int..const_int`, which matches C-call convention.

the arity of lambda is the same as methods.

for lambdas there is a robust call which ignores extra params, and fills missing params with nil.

    l = \x y
      ...
    l.robust_call 3

# impl notes

bit hacks

http://graphics.stanford.edu/~seander/bithacks.html

# optimization based on alias analysis

for example, we can replace immutable operations with mutable operations if some array is confirmed to be not aliased

    a = some_other_a[3..5] # a is a slice, still aliased
    a[0] = 2    # not aliased because slice -> insert copies the whole array
    b = 4       # a is still not aliased
    a[1] = 4    # can use mutable replacement of []= here
    b = a[1..3] # a is aliased

# alternative thoughts

### how we can improve alias analysis?

the tree-like data structures make aliases happen more (when slicing, inserting...), but usual method calls (`:foo a`, `a.foo` also add aliases), should we use logarithm-scaled data structures, can the condition be better? seems NOT

transient operations on array tail can be detected with alias analysis, for other nodes, we use ref counting to reduce memory allocations.

### lazy operations

lazy operations may improve performance, because in many cases, only the last result requires memory allocation.

## feedback directed optimization to dynamic memory

to reduce allocations, the initial size may be changed by runtime info?

## memory management for mutable objects

assume we store an object `x` into a mutable object `a`

    a.set_x x

if x is immutable, then it's ok.

if x is mutable, then we may create a reference loop

    a.set_x x
    x.set_a a

it is nearly impossible to know if an object is leaked...

for IO, this can be handled by hand : open & close . if it is inherited and with normal attrs, the ref_count is handled normally.

for GUI, a dom object can contain some other dom objects, but they are also memory-managed by hand

    div1.set_children [div2, div3]
    div2.set_children [div5]

    div2.remove

    some_divs.push div2 # deleted object!

but for GUI, the memory management system is another version of ref counting, we can still use that for the ease of parent-child management.

for sql serializing, we are still fine if stick to sql level.

so for every mutable object, the memory management is left the user!

think about the most usual mutable object: context
it is also managed by hand: we have the block-end for its death

in real life, we may still use that object after we remove it, all methods will not be able to access after object is deleted. it is hard to add it as syntax, but we may provide C-level function for the `delete`.

for less `delete` bugs, we recommend the open-close principle: `delete` the object at the same level of where it is created. but there are cases that objects are created and deleted dynamically: one can use a method to wrap the generating, and return the object, then use another method to delete the object (in a forked stack for example). it is hard to think of a syntax to restrict it, so we don't restrict it and let them play their own way.

to implement mutable objects, a wrapper immutable object is assigned and ref-counted, and assignments may dup it but the id is not changed. the delete-operation will NULL the instance pointer, and further use will raise the error.

the wrapper object usually will not come to a ref_count of 0, because there is a global OODB table :
{(type,id)=>underlying instance}, and it can be searched and removed by the table. but implementer should be able to add a hook when ref_count==1, then it can be deleted from the table if required.

It is attracting to use the OODB table as a second level cache for database records, but it becomes too complex for concurrent operations. so no. for ORMs, do not use table id as object id!

mutable objects are actors. all methods calls to actors are queued, and processed one by one.

orm example: (just works like normal ORMs!)

    user = User.find 3
    user.name = 'haha' # message!
    user.save          # message!

## do not do eql joining

if obj a eqls obj b, and we know all of obj b's refs (we can deduce that in compile time), can we remove obj b and use obj a only? no, it is not faster, and usually obj b can be mutated if the ref_count == 1.

## VM internals

- source to lex token sequence
- on execution of code, generate AST on the lex token seq (NOTE execution is usually not ordered the same way as we build the AST, code to execute while generating AST seems very hard...)
- AST -> byte code

byte code is essential for running in some devices that not permit allocating executable code.

for x86 server mode, a JIT may be used instead of byte code. but it is future work.

run time information for fast code: http://vee2014.cs.technion.ac.il/docs/VEE14-present35.pdf

## syntax for limited typing

if we specialize IO objects with `<>`, we may also need to specify method return type, and the language is too complex.

if we specialize block object types with `&`, can we benefit from it? not much

    def foo &block
      block.call

so the check should still be: when a block object is released, check the ref count and raise error
