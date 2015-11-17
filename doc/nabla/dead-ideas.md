# local slots

TODO this syntax is weird, we can just add some assumption to normal code and produce optimized result.

Since variables are the only way of mutable state, and for efficiently implementing many algorithms, a `var` keyword can declare an (mutable) array of variables (you can think that `@` means "array")

    var a@20@10 # declare slots of 20*10

[design NOTE] the `@` symbol must be placed after `a`, so it won't confuse the meaning of the brackets after it.

After the declaration, `a` alone can not be used as right value any more, you can only use:

    a@4@1 # get element value
    a@*@* # get an array of array
    a@3@* # get an array as right value (the `@` has higher precedence than `.`)

What are the dimensions of `a`? You already have it when creating.

The members of a slots can be changed (and it is the only way to change it):

    a@i@0 = 3 # set value

Slots can not be captured by lambdas, you must assign it to an array before passing into lambda or other methods

    arr = a@*
    \, :p arr;

Slots is not a fully featured array, its existence is only for effeciency and state management.

Slots are allocated on stack, they can not be resized after declaration. One way to achieve dynamic slots, is to make a new local scope:

    1.upto 3 \i
      var a@i
      ...
    end

Slots can reduce a lot of memory allocations for algorithms depending on in-place array updates.

Note in this immutable world, changing one member doesn't effect another

    var a@3
    a@* = []    # assign all members to []
    a@1.push! 1 # [1]
    a@2         # []

Note a second call to `var a@i` under the same scope shadows prev variable (without releasing it). But in one case can the stack usage be optimized:

    1.upto 1000 do i
      var a@i
      ... # if there are no `var` in here, `var a@i` can be optimized to grow in-place
    end

[design NOTE] Enumerator methods are not fit for slots, consider `a.each do e, a@[3] = e` -- changing the content while looping is bad style.

Slots support some array programming features

    a@*@* = 0 # all members of a is assigned 0
    a@0@* = 1 # all members of a@0 is assigned 1
    a@*@1 = 2 # all members of a is assigned 2 to the slot of 1

    a@0@* *= 3
    a@0@*.inc!

# actor concurrency

[XXX deprecated, see pi-calculus-concurrency.md instead]

Not pi-calculus style, it is simpler (only `Actor` class is required)

It is provided as a library form, no primitives.

[design NOTE] if we add `spawn` as keyword primitive, then hard to make the syntax to fit both object style and explicit receive style.

---

problems:

It is in fact no way to do static analysis in a dynamic language.

`spawn`, `yield`, etc... are in fact more complex than channel systems.

### Actor.oneshot and Actor.loop

`Actor.oneshot` creates a one-shot actor with an object as responder, and route all messages into `call` and loops.

    a = Actor.oneshot obj
    res <- a.send ['foo', 'bar'] # back-stabby style callback

[Note] the first arg of `send` is the message, the second is the callback, so we distinguish which lambda is message and which is not.

Usage on lambdas

    a = Actor.oneshot \foo, ...
    a.send 'foo' 'bar'

To turn a actor back to object (but not always the same object):

    obj = actor.sync
    res = obj.foo 'bar'

The essential difference: `call` returns, but `send` doesn't.

To create a loop respond actor:

    Actor.loop obj

### spawn

The explicit `spawn`, `recv` style

    spawn
      switch recv
      case ...
      case ...

Instead of one-shot actors, we may have loop responders so we can preserve some states

    spawn
      called = 0
      while true
        called += 1
        [method, args, pid as Lambda] = recv
        res = obj.call method, args
        pid.call res # callback func

[NOTE] we use callback func so the 

[NOTE] this is syntax sugar for `Actor.spawn`, `Actor:recv`, but with syntax we can eliminate errors like:

    a = Actor.new obj
    a.recv # blocked

For safety, these methods are not exposed to user

# Error handling

in `sync`, the child actor can raise error to the container actor.

in spawn calls, we can check `actor.error` (actor is synchronized when error happens)

    res <- actor.send 'foo' 'bar'
    if actor.error
      actor.respawn
      ...
    else
      ...

### Addressing

So we can invoke actor on other machine

    Actor.assign "foo" actor

### actor lifecycle

it dies when it has ref count = 0. (assigning address adds refcount too)
it dies when an uncaught exception happens

### actor utils

- keep spawning some actors and check and record its death...
- load balancing actor pool
- serializing lambdas
- insert break point to one of the actors

### special messages

- send lambda to other actor
  - for local, the pointer is used
  - for remote, the lambda is serialized
- send file descriptor
  - for local, just the fd
  - for remote, create a TCP connection
  - split and broadcast?

# optional block closers

no need

      end if
      end def
      end class
      ...
