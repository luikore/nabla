# Structs

`struct` declares a new data structure. An upper case identifier after `struct` defines a constant struct:

    struct Foo[];

Which can be initialized:

    foo = Foo[]

It takes a list of fields, and fields can be type checked

    struct Foo[
      a as $/r \d+/
      b as Integer
      c as String
      d, e, f # 3 fields without type checker, note that `,` has the same meaning with new line
    ];

You can mutate fields in the code body of `struct`:

    struct Foo[a]
      a *= 2
    end
    foo = Foo[3]

    # fields can be accessed like a method
    foo.a # 6

[design NOTE]

> Adding ivars to core struct types is not allowed (so allocations and copying are easier), however, it should be easy to delegate methods to them.

A lower case identifier after `struct` makes it a local variable instead of constant:

    struct foo[a];
    foo[3]

Structs can also be declared in a map-style:

    struct Foo{
      a: x
      b: y
    };
    foo = Foo{a: 3, b: 4}
    foo.x # 3
    foo.y # 4

Structs can inherit data members from other struct types, and fields with the same names will overlap the previous one.

    # `Foo` inclucdes members from `Bar`
    struct Foo[
      include Bar
      x
      y
    ];

NOTE in above code: `Bar` must be defined before including into `Foo`.

[design NOTE]

> We use `include`, not `*` because `*` is used to match variable sized members.

Although we suggest using snake cased name as member, but you are allowed to to define a capital cased one.
But in order to make pattern matching syntax happy, when you define a member named `Bar`, you should write it as `Bar as _`.

Object creating can also be in map-style:

    Foo[1, 2, 3]   # ordered, new with operator []
    Foo{a: 1, b: 2, c: 3, d: 4} # looks like a hash, but much light weight in fact
                                # no operator `{}`: it is a built-in syntax

Like creating arrays and maps, you can use splats:

    Foo[*some_array]
    Foo{*some_map}
    Foo[-> a, ...;]

If an object is not a struct, it can be used as a prototype for new objects:

    foo = Foo{a: 2, b: 4}
    bar = foo{"a": 3, "b": 4} # overwrites members, also calls Foo's constructor

When a field name ends with `?`, it is converted and stored in boolean

    struct Foo[
      a?
      b?
    ]

    foo = Foo{1, nil}
    foo.a?    # true
    foo.b?    # false
    foo.b = 1 # Foo{true, true}

Like local constants, struct can be local-only:

    local struct Foo[
      ...
    ]

[design NOTE]

> The nabla object serialize format with class support:
>
>     {"foo": Date{"year": 1995, "month": 12, "day": 31}}
> 
> It is pure text, a bit readable, and can be parsed (with only the value rules, no other operations allowed).

## Struct with variable initializers

The splat matcher can be used to match arbitrary number of members

    struct Foo[
      x
      *xs as Array # other construct args are put into xs
    ];

    foo = Foo[1, 2, 3, 4]
    foo.x  # 1
    foo.xs # [2, 3, 4]

    # but the map-style constructor must use this form instead:
    foo = Foo{x: 1, xs: [2, 3, 4]}

Or the rest members in a map-like struct

    struct Foo[
      x
      *xs as Map
    ]
    end

    foo = Foo{x: 1, y: 2, z: 3, w: 4}
    foo.x  # 1
    foo.xs # {y: 2, z: 3, w: 4}

    # then array-style constructor must use this form instead:
    foo = Foo[1, {y: 2, z: 3, w: 4}]

## `make` constructor

It is as expressive as (or more) applicative do or list comprehensions.

For example:

    sums = make Array
      a <- as.each
      [b as Integer, *_] <- bs.each_slice 4
      pick a * b
    end

`make` may also be used in other struct types

    make Point
      x <- coords.each
      pick x
    end

    make Foo
      [k, v] <- kvs.each
      pick k => v
    end

since the following form doesn't look very pleasing:

    ->
      ...
    end[]

we can use `make` without `pick` to achieve the same effect

    make
      ...
    end

example with IO:

    make
      h <- IO.open 'f' 'w'
      line <- a.read_line
      if /foo/.match? line
        h.write line
      end
    end

or simplify with pattern match:

    make
      h <- IO.open 'f' 'w'
      [line as /foo/] <- a.read_line
      h.write line
    end

example with try (`Object[o]` yields `o`, and `Object[]` yields `nil`):

    make Object
      a <- b.try
      pick a.foo
    end

but there is no "state monad", since we only build result in last phase

[NOTE] we may make many things applicatives...

the "if" applicative

    make Object
      <- :if foo
      pick bar
    end

the "while" applicative

    make Object
      <- :while ->, foo;
      pick bar # only the first works
    end

    make Array
      <- :while ->, foo;
      pick bar # all elements are collected into the array
    end

[NOTE] we don't have Elvis operator `?.`, it makes syntax hard to recognize when combined with question mark method names. but we can specify syntax for this kind of visits, see [Custom Syntax](custom-syntax.md) for more information.

[impl NOTE]:

We add an implicit continuation arg in lambda calling, with it we can aggregate struct into the primary stack.

See also https://ghc.haskell.org/trac/ghc/wiki/ApplicativeDo for applicative do notation

[design NOTE]:

- It is in essential a monad
- In the block is just normal nabla code, with just one addition: `pick elem` or `pick k => v` (NOTE we should disable the syntax `pick k: v` since in this case k is usually string)
- if we use `collect Array[...]` syntax, then it becomes a bit ambiguous with `Array[...]`, and we know the languages between
- it is so more like a control syntax instead of a constructor syntax
- the syntax is a relieve for lambdas not being able to change outer variables
- if simplify `x <- xs.each` to `x <- xs`? (calling `bind` method). but then it this expression is ambiguous: `x <- xs.foo bar`.

[TODO] in monad comprehension there can also be `take` and `group by`
https://ghc.haskell.org/trac/ghc/wiki/MonadComprehensions


# Behavior types (class, include, scope, delegate)

a struct type is inherently a behavior type, but there can be behavior types that are not struct types

    class Foo
      include Bar # note: you must define Bar first
    end

Some notes on `def` vs `class`.

- `def` can not be opened after definition. but `class` can.
- Under `def` we can not define methods.
- `def` generates default getter and setters, and they are `final`.

the name being included must be class

there is only one kind of inheritance via `include`, methods defined in `include` module is looked up hierachically

`class` can be re-opened, but `struct` can not.

classes can also be scoped

    class Foo
      scope bar
        include Bar
      end
    end

scoping is a way to separate concerns. `scope` in fact creates a new class and provide ways

    struct Foo[a]
    class Foo
      def a; # no way: can not overwrite final method
      scope foo
        def a; # ok, since scope is an implicit sub struct type
      end
    end

The goodness: one struct type can re-use other struct-types methods.

the order of behavior type and struct type can be switched

    class Foo
      ...
    end
    struct Foo[...]

but accessor methods are overridden:

    class Foo
      def x= v
        ...
      end
    end
    struct Foo[x] # overrides Foo#x and Foo#x=

if you define final methods that are overridden by struct, it throws error

    class Foo
      final def x=v
        ...
      end
    end
    struct Foo[x] # error overriding final method

A local class means the constant is not available in global constant lookup:

    local class Foo
    end

## Scope with parameters

Scope accepts parameters just like methods, and they can be accessed via reader methods

    struct Foo[]

    class Foo
      scope foo x y
        def bar
          :x
        end
      end
    end

    f = Foo[]
    (f.foo 3 4).x   # 3
    (f.foo 3 4).bar # 3

Scope parameters can use pattern match too, see more in [Pattern Match](pattern-match.md)

Scope can be considered currying in OO programming, and it is not necessary ordered.

    class Foo
      scope bar x
      end

      scope baz y
      end
    end

    foo = Foo[]
    foo.bar! $ 1 $ .baz! 2
    foo.x # 1
    foo.y # 2

## Delegate

[design NOTE]

> It is a bit too much to design a syntax for delegate... so make it a macro method instead

    class Foo
      :delegate Bar {on: 'bar', only: ['x', 'y', 'z']}
    end

If we delegate every method like this:

    class Foo
      :delegate Bar {on: 'bar'}
    end

It snapshots method searches on Bar and define them all on the class.


## Class with struct

`def` defines a new data type and a class with the same name, and adds final attribute accessors. But when `include` the class defined by `struct`, the attribute accessors are not included.

    def Foo[foo];

    class Bar
      include Foo
      def :bar
        :foo # error, method not defined
      end
    end

We may define the behavior before defining the data, an example:

    class Foo
      def :double_foo
        :foo (+) :foo
      end
    end

    def Foo[foo]

[design NOTE]

> If we allow methods defined under `struct`, then the difference from `class` is unclear (let `struct` mutate members? but how about more members?)

## Defining method

TODO: The difference between top level method and class methods.

It is similar as defining a `struct`, but it returns last value (or the value after `return`) in local scope instead of the method struct

    def plus_one[x]
      return x + 1
    end

There is a syntax sugar for method params, with just spaces:

    def plus_one x
      x + 1
    end

This syntax sugar also applies to struct definition:

    struct PlusOne x
      x += 1
    end
    PlusOne[3].x # 4

Methods can also be defined on an object

    def Obj.foo
      ...
    end

Mutator methods end with `!` or `=`

    def my_mutate!
      self.b = 3 # good, changes self
      :b = 3 # warning, changes self while not putting self on the left
    end

    def v= x
      self.double_v = x * 2
      self.tripple_v = x * 3
    end

note: `!` acts as a method modifier. if there are 2 methods in the same name but one ends with `!` while the other doesn't, the later defined one can override the first defined one.

NOTE: there is a limit to `!` and `=` terminated methods: it must not change the data type of the original object.

NOTE: if you don't want to change `self`, use other left-values:

    def foo
      a = self
      a.mutate! # doesn't change self
    end

Predicate methods end with `?` and return values are always converted to `true` or `false`

    def good? x
      x
    end

To mention a method, we can use `Klass:method`, but it is not valid syntax (just for simplicity of reflection API).

To make a method only visible to local file, can add a `local` prefix

    local def foo
      ...
    end

`local` methods are always `final`.

Method definition is the boundary of lexical scoping for local vars.

## Method calling

Methods can be called with 3 styles in order to fit different needs:

    :foo bar baz
    # backslash makes it lambda-like
    :\foo[bar, baz]
    :\foo{bar: bar, baz: baz}

For methods with a receiver:

    x.foo bar baz
    x.\foo[bar, baz]
    x.\foo{"bar": bar, "baz": baz}

To find out the method struct

    m = (:methobject 'foo')[true, 10]
    m.bar # true
    m.baz # 10
    # only the variables in signature are accessible
    m[]   # calls method

## Special methods

    def hash

    def eql?

[design NOTE]

> If we use, `def_hash`, `def_eql` may introduce too many keywords and syntaces. though the other way introduces more special rules.

[style guide]: `.build` is a convention for customized constructors. For example

    def Foo.build a
      Foo[a * 3, 4]
    end

## Default params

params with default values must be put after other params

    def foo a b=3 # usually we don't put spaces around `=` here, but it is allowed

if some default value contains something with looser precedence than method space, it should be wrapped in brackets

    def foo a=(:bar 3 4)
    end

there are no "named params", the following code just sets a default map to param `b`

    def foo a b={c: 1} d=2
      {c: c, d: d} ~ b # but you can match against the map here
    end

    :foo a {c: 1, d: 2} d
    :foo a {c: 1} d # match error

## Default params can be put in reverse order

    def foo a=3 b=4 c
      ...
    end

Arity is still the same as normal order (`1..3` in the above example). `:method('foo').reverse_default_args?` is `true`

[design NOTE]

> There is no arbitrary arity, the syntax is harder to fit in.

## Matching args

see [Pattern Match](pattern-match.md)


## Namespaces

Every class also acts as a namespace

    class Foo
      class Baz
        Bar # searches Bar inside Baz, if not, search under Foo, if not, search top namespace
            # if not found: raise error - constant 'Bar' not found under 'Foo::Baz'
      end
    end

Macros are searched via namespaces too. The following example shows how to use diffrent sql dialect macros:

    class Foo
      include Pg
      $(select ...)
    end

    class Bar
      include Mysql
      $(select ...)
      Pg::$(select ...)
    end

## Method search rule

search own methods, then search methods defined in latest included class

    class A
      include M1
      def foo # first foo
        ...
      end
      include M2
      def foo # overwrite the first foo
        ... super # search in M2, if not found, search in M1
      end
    end

`super` calls method defined in included class

if some class included shall overwrite self-defined methods, use the `:prepend` macro instead

    class B
      def foo
        :puts 'B'
      end
    end
    class A
      def foo
        :puts 'A'
      end
      include B
    end
    A[].foo # 'A'

    class A
      :prepend B
    end
    A[].foo # 'B'

Note `:prepend` is a macro method, not declarative instruction like `include`, only methods available at the call site are put into A.

[design NOTE]

> This is the same method search mechanism as in Ruby, so all own-defined methods can easily be put in a map for less search steps

## The final modifier

a `final` method can not be modified, even in the inherited class.

    final def foo
      ...
    end

    def foo;   # error
    undef foo # error

[design NOTE]

> We should not do `final class`, this will lead to bad-to-explain semantics. for example: do we freeze every method in the beginning or end of `final class` block? should we inline included modules?

`final` effects only within some classes, in child classes, you still can re-define those methods -- because it is not mutating, but overlapping.

    class Parent
      final def foo;
      final def bar;
    end

    class Child1
      include Parent
      def foo; # OK
      undef bar # OK
    end

`final` can also be used in `undef`, which makes a method with the same name un-definable

    class Foo
      final undef bar
      def bar # error
      end
    end

NOTE: it is mainly used for certain optimizations to work, but not recommended to be used everywhere. source files with `final` can not be reloaded!

NOTE: there is no modifiers like `private` and `protected` -- we can always use `scope` and `.delegate` to reuse code while keeping concerns separated. and they are not easy to test. for some cases of private/protected methods, we can also use lambdas.

[design NOTE]

> The modifiers are not methods? `:final`, `:include`, `:scope` makes compiler optimizations harder, and not as easy to write either. And `include`, `scope` all require implicit receiver: the scoped class, making them methods is a bit complex.

## Scoping

constants follow `class` scoping, while variables follow lexical rules and are separated by `def`s

    A = 3
    a = 3
    def foo
      A # 3
      a # error
    end

[design NOTE]

> Even constant folding with literals is not viable in a dynamic language.
> Consider we may have a "commit current constant" operation, which forbids all the constants from reloading then they become real constants and we can start constant folding?
> But we still can change name mangling of a constant symbol after that. Think this code:
> 
>     A = 3
>     class B
>       def foo, A;
>     end
> 
>     # then after a while
>     class B
>       A = 5
>     end
> 
> So we can cache constants, but folding is not allowed. --- should we force all constant references to be concrete? no, the syntax is bad for initialization-heavy code. Constant caching
> 
>     A = 3
>     class B
>       def foo
>         A   # can cache but not fixed cache
>         ::A # can cache and make final method lookup fixed without class tests
>       end
>     end
> 
> Constants have a `source` meta connected to it, when reloading a source, all constants defined by it is undefined and all constant / final caches are cleared. -- so `final` method just skips a little bit overhead of class testing?
