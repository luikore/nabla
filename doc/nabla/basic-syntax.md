This article introduces Nabla's basic syntax.

# Literals

We first start with literals -- values that are known before compile time. You know the type when you see the literal.

## Integer numbers

Examples:

    0
    105
    -2

There's no limit of how large an integer you can write:

    999888777666555444333222111

Underlines in between digits will be dropped:

    1_234_567

Binary integers are prefixed with `0b`:

    0b1011000

Hex integers are prefixed with `0x`:

    0x3f2A

Oct numbers have a `0o` prefix, so that number parsing won't make mistakes on numbers that start with `0`:

    0o123

## Floating numbers

Examples:

    1.1
    5.0
    1e10
    0.2E-3

Hex numbers with scientific notions (use `p` instead of `e`)

    0xFFp3 # == (0xFF * 2 ** 3).to_f

## Rational and complex numbers

The rational suffix is `r`

    3/4r

The imaginary suffix is `i`

    2i

[design NOTE]: since we don't have `/.../` regexp, there is no regexp suffix flags (o x u i g m etc), just use `(?i)` and `(?m)`

## Strings

Strings can be single quoted, the only escapes are `'` and `\`:

    'a\'string\\'

Or double quoted, can contain escapes, hex, unicode:

    "a\n\x33\u{2345}"

And strings can take multiple lines

    '
    oh
    yes
    '

The spaces are significant. See [Custom Syntax](custom-syntax.md) for more ways to define string literals.

## Ranges

    a .. b  # b inclusive
    a ... b # b exclusive

NOTE: since `..` and `...` possess very high precedence (only lower than `.`), adding paren around it is very common pattern (see below for operator precedence)

    a.b (...) c + 1

## Other direct literals

Boolean values:

    true
    false

`nil` value:

    nil

`nil` also denotes the empty list.

# Basic syntax elements

## Constants and variables

Names starting with upper case is constant, they can not be re-assigned:

    A = 3
    A = 4  # error
    A = 3  # OK, no conflict and no effect (this design makes `load` a bit easier)

Names starting with lower case is variable, they can be re-assigned:

    a = 3
    a = 4 # not shadow naming

Variables better be named in snake case. Examples:

    foo
    bar3
    foo_bar
    fooBar # still OK, but not recommended as code style

The local environment is mutable, local vars can be changed by another assignment, even in a lambda:

    a = 0
    l = ->
      a = 3
    end
    l[] # note: have to give a [] so it won't confuse with local var reference
    a # 3

Variables are declared the first time you assign it.
Can use `var` to force declare variable instead of search up captures

    a = 0
    ->
      var a = 3
      a # 3
    end
    a # 0

See more in [Constant Management](constant-management.md).

## Non-exposing constants

By default constants will be exposed in current scope, use `local` to not expose constants:

    local A = 3

## Anonymous value `_`

If it is "left value" which lies on the left of assignment or matching-assignment, then it has no effect. (see also pattern matching)

Since `_` is so special, it can not be used as variable name or method name.

    [_, x] = [3, 4]
    x as _ ~ 4
    x as _.f ~ 4 # syntax error
    _.f ~ 4 # syntax error
    x._ ~ 4 # syntax error

## Comments

line comments

    # foo
    # bar

block comment

    #||
      foo
      bar
      baz

block comment with preprocessor (should use no space between `#` and `<<` and `html`)

    #|html|
      foo
      bar
      baz

    #|markdown|
      foo
      bar
      baz

    #|asciidoc|
      foo
      bar
      baz

    #|call_seq|
      runnable code

NOTE: block comments are not allowed after a non-empty line, the comment below is line comment and generate a warning:

    foo #|md|

## Comma, new line, join lines, semicolon and `end`

There are two important syntax sugars to make it easy to compose single-line or multi-line programs for fit your need of most readable program.

`,` means new line

    [a,
      b, c]

`;` means `end`

    if foo, bar;

A single `$` means evaluate the expression before, then use it as input of next expression (can be put in the next line)

    :foo bar $
    baz

    # is equivalent to

    :foo bar baz

A single `$` in the middle of line makes the spaces around it "more significant"

    :foo :bar :baz          #=> :foo (:bar :baz)
    :foo :bar $ :baz        #=> :foo (:bar) :baz
    :foo :bar $ :baz $ :xip #=> :foo (:bar) (:baz) (:xip)

# Operators

## Operator precedence

method dot is tightest

    :foo
    foo.bar
    foo@bar

then operators (mapped to method, can redefine) and the space in method calls

    ! + - * **              # 90  prefix and splat
    .. ... **               # 80  range and power
    * / %                   # 70  multiplicative
    + - & | ^ << >>         # 60  additive and bit
    < > != >= <= == <=>     # 50  compare
    && || ^^                # 40  logic, can be redefined. note: ^^ means logic xor
    :foo a b                # 30  space in method name and params

NOTE: for more compatibility with other languages, `&& || ^^` do shortcut

knot operators: it means an operator is wrapped by `()`, the effect is precedence -= 100, making it looser than all of the above. but still can not be looser than assignments. for example:

    x = a (**) :foo a b  # means `x = (a ** (:foo a b))`

NOTE: we can't do the same as in Haskell, or operator expressions would require a pair of parens wrapping around
NOTE: no loop shift operator: method `.shl`/`.shr` requires a second param of bit length.
NOTE: no bit flip operator: only method `.flip`.

[design NOTE]: method calls can not fit in the knot operators system. or `(:foo)` is ambiguous (it can mean call or looser method name)

then looser: assignment / match, can not be wrapped. all assignment operators are of the same precedence and bind from right to left

    ~ = += *= ...

then looser: bind operator `<-`

then looser: logic control structures (can not be redefined, do shortcut):

    not and or xor

then control structures

    if else case ...

## On prefix operators

The prefix operators are: `!`, `-` and `+`

NOTE that `not` is equivalent to `!`

NOTE unlike other languages, in Nabla the `~` symbol is not used for flipping bits, you should use `.flip` instead:

    n.flip

the `.flip` decision is for less ambiguity on `~` matching operators.

since `+` and `-` may be ambiguous as infix or prefix, the defining syntax of prefix operators requires explicit `self`:

    # prefix
    def +self
      ...
    end

    # prefix
    def Foo::Bar.+self
      ...
    end

    def self.+self
      ...
    end

The method names are `!self`, `+self`, `-self`

## Chaining binary operators

    a.< b c   # a < b and b < c
    a.+ b c   # a + b + c
    a.* b c   # a * b * c
    a.&& b c  # a && b && c

NOTE: the methods are defined with arbitrary members, but VM can optimize them.

[design NOTE] since the rules for `<` is different from `+`, so we should not auto translate them into chained by syntax, it is too strange to make many special cases.

# Control structures

`if` conditional statement

    if foo1
      bar1
    else if foo2
      bar2
    else
      bar3
    end

`while` loop

    while foo
      bar
    end

`wend` ... `while` loop

    wend
      bar
    while foo # perform loop body first, then check condition

`next` and `break` terminates current loop.

`make` ... `pick` (see more in [Make Constructor](#make-constructor)

    make
      a <- as.each
      pick a * 2
    end

`case` ... `when` (see more in [Pattern Match](pattern-match.md)) is exhaustive match: if no `else` branch and no valid matching cases, it will raise error.

    case x
    when 1, ...
    when String, ...
    else, ...
    end

`goto` helps getting out of nested control structures. but can not break out of lambda/method/class.

    loop
      ...
      goto some_label
    end
    some_label:

[design NOTE] we don't need mod clauses, just use `,` for one-liners, and it is easier to modify

    if a, b;
    if a, b, else if c, d, else e; end

## Exception handling

`try...when` is a control structure for exception handling

    tries = 0
    try
      if :foo
        throw "foo"
      end
    when "foo"
      tries += 1
      if tries < 10
        :sleep 1.1 ** tries
        redo
      end
    when _
      :puts "unkown error"
    end

`throw` throws an exception (for better debugging, it can be traced and resumed in VM)

`redo` calls the try block again

# Data structures

## Array and hash literals

create an array (compiler may choose list, or HAMT, or array buffer for underlying implementation)

    [a, b, c]

because `,` is the same as new line, the above is equivalent to

    [
      a
      b
      c
    ]

create a hash map

    {
      "a": b
      "c": d
    }

Note that hash is un-ordered

For convenience, for `:` the key is string by default, for `=>` the key is expression by default.

    {
      a: 1
    }

Is the same as

    {
      "a" => 1
    }

To get a member from array or map, use `@` operator:

    arr@3
    m@k

And there is `@=` operator:

    arr@3 = 2

NOTE: we use the `@` operator, so looking for subscript looks cleaner, and not conflict with object creating syntax `A[foo, bar]`, `A{foo: foo, bar: bar}`

A shorthand to make maps with local variables:

    {
      a
      b
      c
    }

is equivalent to:

    {
      a: a
      b: b
      c: c
    }

## Alternative collections

### Assoc array

assoc-array (map-like structure backed by array)

    AssocArray{
      a: a
      b: c
    }

it can store repeated values

    foo = AssocArray{
      "a": 1
      "a": 2
    }
    foo.add "a", 3

assoc-array is aware of insertion order

    foo@'a' # 3

assoc-array can be used as a multiple array.

    foo.values_for 'a' # [1, 2, 3]

Iterating will go through all stored values. The multi-array feature makes it suitable for storing cookies, request params, and http headers.

And it also has `@=` and `:delete!` methods to update or remove existing values under a key.

When elem number is very large (hundreds or so), assoc-array may become slow for random access, but methods like iterating is still very fast.

### Set

Set (array-like structure backed by map)

    Set[a, b]

### Dict

Dict has little memory for large string dictionary, but unordered

    Dict{ ... }

### Rbtree

rb tree, find is by default binary search

    RbTree{ ... }

can be used as priority queue

### Pattern matching collections

Left and right must match the type exactly.

If the right pattern is `[]`, will call `.to_l` of left.
If the right pattern is `{}`, will call `.to_h` of left. (`Set:to_h` will expose the internal hash map, and the values are `true`)

[design NOTE] list is not really useful: slicing, deleting, or concatenating 2 immutable lists still require O(n) operations
RRB tree may be a good option for list impl.

[design NOTE] there is no need for a linked hash map, an immutable one is very slow. For mutable impl, redis may be a better choice.

## Splat operator for generating new arrays

To unroll an sequence-like data structure, use `*`
To unroll a map-like data structure, use `**`

    [a, *b, c]
    {"a": a, **b, "c": d}

NOTE it can also be used in patterns

It can also be used in method calling:

    \(:f)[a, *b, c]

## Data types (structs)

[design NOTE]: adding ivars to core struct types is not allowed (so allocations and copying are easier), however, it should be easy to delegate methods to them.

`struct` declares struct type, fields list can be type checked

    struct Foo[
      a as $/r \d+/
      b as Integer
      c as String
      d, e, f # 3 fields without type checker, note that `,` has the same meaning with new line
    ]

It can also be declared in map-style:

    struct Foo{
      a: x
      b: y
    }
    foo = Foo{a: 3, b: 4}
    foo.x # 3
    foo.y # 4

in a `struct` type, you can add data members from other struct types, and fields with the same names will overlap the previous one.

    # `Foo` inclucdes members from `Bar`
    struct Foo[
      include Bar
      x
      y
    ]

NOTE in above code: `struct Bar` must be defined before including into `Foo`.
NOTE we use `include`, not `*` because `*` is used to match variable sized members.

Although we suggest using snake cased name as member, but you are allowed to to define a capital cased one, just use something like `Bar as _`.

No behavior will be included, only data members.

Some notes on `struct` vs `class`.

- `struct` can not be opened after definition. but `class` can.
- Under `struct` we can not define methods.
- `struct` generates default getter and setters, and they are `final`.

When a struct is defined, there are ways to create an object

    Foo[1, 2, 3]   # ordered, new with operator []
    Foo{a: 1, b: 2, c: 3, d: 4} # looks like a hash, but much light weight
                                # no operator `{}`: it is a built-in syntax
    Foo[*some_array]
    Foo{*some_map}
    Foo[-> a, ...;]

There is also the prototype-update way:

    foo = Foo{a: 2, b: 4}
    bar = foo{"a": 3, "b.c": 4} # overwrites members

Structs are where the prototype chain end.

    foo = Foo
    bar = foo{"a": 3, "b.c": 4} # calls Foo's constructor

When a field ends with `?`, it is converted and stored in boolean

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

[design NOTE]: only boolean converter is available, but we can use lambda's as converter.

[design NOTE]: if we allow methods defined under `struct`, then the difference from `class` is unclear (let `struct` mutate members? but how about more members?)

[design NOTE]: the nabla object serialize format with class support:

    {"foo": Date{"year": 1995, "month": 12, "day": 31}}

It is pure text, a bit readable, and can be parsed (with only the value rules, no other operations allowed).

## Delegate

[design NOTE] it is weird to design a syntax for delegate... so make it a macro method instead

    class Foo
      :delegate Bar {on: 'bar', only: ['x', 'y', 'z']}
    end

If we delegate every method like this:

    class Foo
      :delegate Bar {on: 'bar'}
    end

It snapshots method searches on Bar and define them all on the class.

## Struct with variable initializers

The splat matcher can be used to match arbitrary number of members

    struct Foo[
      x
      *xs as Array # other construct args are put into xs
    ]

    foo = Foo[1, 2, 3, 4]
    foo.x  # 1
    foo.xs # [2, 3, 4]

    # but the map-style constructor must use this form instead:
    foo = Foo{x: 1, xs: [2, 3, 4]}

The double splat matcher

    struct Foo[
      x
      **xs as Map
    ]

    foo = Foo{x: 1, y: 2, z: 3, w: 4}
    foo.x  # 1
    foo.xs # {y: 2, z: 3, w: 4}

    # then array-style constructor must use this form instead:
    foo = Foo[1, {y: 2, z: 3, w: 4}]

matchers can not be used together

    struct Foo[
      *xs
      **ys # error: mixed matchers
    ]

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

# Behavior types (class, include, scope)

a struct type is inherently a behavior type, but there can be behavior types that are not struct types

    class Foo
      include Bar # note: you must define Bar first
    end

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

[TODO] do we add syntax for `import Behavior` so methods in the object can be limited?

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
    foo.bar! 1 $ .baz! 2
    foo.x # 1
    foo.y # 2

## Class with struct

`struct` defines a new data type and a class with the same name, and adds final attribute accessors. But when `include` the class defined by `struct`, the attribute accessors are not included.

    struct Foo[foo]

    class Bar
      include Foo
      def bar
        :foo # error, method not defined
      end
    end

We may define the behavior before defining the data, an example:

    class Foo
      def double_foo
        :foo (+) :foo
      end
    end

    struct Foo[foo]

## Defining method

    def foo
      ...
    end

to define method on an object (TODO consider whether `def obj.foo` makes sense)

    def Obj.foo
      ...
    end

mutator methods end with `!` or `=`

    def my_mutate!
      self.b = 3 # good, changes self
      :b = 3 # warning, changes self while not putting self on the left
    end

    def v= x
      self.mutate!
    end

note: `!` acts as a method modifier. if there are 2 methods in the same name but one ends with `!` while the other doesn't, the later defined one can override the first defined one.

note: there is a limit to `!` and `=` terminated methods: it must not change the data type of the original object.

note: if you don't want to change `self`, use other left-values:

    def foo
      a = self
      a.mutate! # doesn't change self
    end

predicate methods end with `?` and return values are always converted to `true` or `false`

    def good? x
      x
    end

To mention a method, we can use `Klass:method`, but it is not valid syntax (just for simplicity of reflection API).

To make a method only visible to local file, can add a `local` prefix

    local def foo
      ...
    end

`local` methods are always `final`.

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

## Object path changing

Consider this expression:

    a.b@c.d = 1

It changes local variable `a`, and is equivalent to something like:

    a1 = a.b@c
    a1.d = 1
    a2 = a.b
    a2@c = a1
    a.b = a2

This may remind you the reference changing in mutable languages, or the lens library in Haskell :)

What if I do not want to change it?

    x = a.b@c
    x.d = 1 # a is not changed

The rule is simple: only the variable on the very left (leftest value) is changed.

We may define a chain macro with `:=` to DRY code:

    x := a.b.c
    x.d = 3 # equivalent to a.b.c.d = 3

Note: Bang methods can only be used in the tail of the chain.

    a.b.c.d!     # OK
    a.b.c!.d = 3 # syntax error, meaning is ambiguous

Note: The chain can be optimized by compiler

## Special methods

    def hash

    def eql?

[design NOTE]: if we use, `def_hash`, `def_eql` may introduce too many keywords and syntaces. though the other way introduces more special rules.

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

[design NOTE]: there is no arbitrary arity, the syntax is harder to fit in.

## Matching args

see [Pattern Match](pattern-match.md)

## Method calling

method definition is the boundary of lexical scoping for local vars

`:` lookup passes object, then goes to `Kernel` (it's meta class includes `self`), so methods on `Object` is much fewer.

    foo.bar
    :bar
    ::bar

The chain operator `$` can chain lines together

    foo.bar baz xip $
    .lower baz xip $
    .continue baz xip

composed

    foo.bar :baz xip

not composed

    foo.bar (:baz) xip

[design NOTE]: do not support `(.meth)`, because then we may induce `(:meth)`, and it is ambiguous

[design NOTE]: keyword `self` instead of `@`, to free the usage of `@`

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

[design NOTE] this is the same method search mechanism as in Ruby, so all own-defined methods can easily be put in a map for less search steps

## The final modifier

a `final` method can not be modified, even in the inherited class.

    final def foo
      ...
    end

    def foo;   # error
    undef foo # error

[design NOTE] we should not do `final class`, this will lead to bad-to-explain semantics. for example: do we freeze every method in the beginning or end of `final class` block? should we inline included modules?

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

[design NOTE]: the modifiers are not methods? `:final`, `:include`, `:scope` makes compiler optimizations harder, and not as easy to write either. And `include`, `scope` all require implicit receiver: the scoped class, making them methods is a bit complex.

# Lambda

Can capture local vars and change them

    -> x
      x + 2
    end
    -> x y, x + y;

Recall note: comma (`,`) means "new line"

Lambdas with side effects dies when it goes out of scope, only a pure lambda can live long, `Lambda#snapshot` generates a pure lambda (TODO more decent example to address the use):

    x = 0
    l = -> v
      x = v
    end
    l[1]
    x # 1
    l.snapshot!
    l[2]
    x # 1

## `\` syntax for quick lambdas

`\` followed by an operator is a lambda. The following 2 lambdas are equivalent:

    -> x y, x + y;
    \+

To compute factorial for example:

    (1..4).reduce \* # 24

`\` followed by `()` can generate quick lambdas for more general forms:

    \(4 /)         # -> x, 4 / x;
    \(+ 3)         # -> x, x + 3;
    \(*)           # -> x y, x * y;
    \(.present? 3) # -> x, x.present? 3;
    \(:foo)        # :method('foo') # arity depends of method foo
    \(:foo 3)      # (:method('foo').curry 1)[3]

[design NOTE] if we use placeholder lambdas as in scala, then too many meanings are put onto `_`, while saving very few typings.

## Currying

    l = -> x y
      x + y
    end
    l.curry!
    l[1][2]

## Variable scoping

constants follow `class` scoping, while variables follow lexical rules and are separated by `def`s

    A = 3
    a = 3
    def foo
      A # 3
      a # error
    end

[design NOTE] even constant folding with literals is not viable in a dynamic language.
Consider we may have a "commit current constant" operation, which forbids all the constants from reloading then they become real constants and we can start constant folding?
But we still can change name mangling of a constant symbol after that. Think this code:

    A = 3
    class B
      def foo, A;
    end

    # then after a while
    class B
      A = 5
    end

So we can cache constants, but folding is not allowed. --- should we force all constant references to be concrete? no, the syntax is bad for initialization-heavy code. Constant caching

    A = 3
    class B
      def foo
        A   # can cache but not fixed cache
        ::A # can cache and make final method lookup fixed without class tests
      end
    end

Constants have a `source` meta connected to it, when reloading a source, all constants defined by it is undefined and all constant / final caches are cleared. -- so `final` method just skips a little bit overhead of class testing?

## Back arrows

back arrows generates nested lambda

    make
      e <- a.each
      f <- b.each
      <- c.each
    end

is equivalent to

    a.each -> e
      b.each -> f
        c.each ->
        end
      end
    end

it is just an opposite writing of `->` and the following lines before `end` are brace nested into the blocks.

back arrows can be used inside any syntax structures with `end` or `when` delimiter, the following are all valid syntax:

    class Foo
      e <- a.each

      scope foo
        e <- a.each
      end
    end

    try
      e <- a.each
    when _
      e <- a.each
    end

    ->
      e <- a.each
    end

    def foo
      e <- a.each
    end

    if foo
      e <- a.each
    end

back arrows can be considered as monad of the universal sum type. they can be tail-call optimized.

## Breaking out of a `each` call

`break`, `next` and `goto` can break out lambdas

    array.each -> e
      if e == foo
        break
      end
    end

    ax.each -> a
      bx.each -> b
        if :foo a b, goto outside;
      end
    end
    outside:

[impl NOTE]: it will be a special type of yielding?

## Matching args

See [Pattern Match](pattern-match.md)

# Import: specifying code dependencies

    import foo/bar     # require code from another file, under load path, from first to last
    import ./foo/bar   # search starts in folder containing current source
    import /foo/bar    # search starts in root folder
    import c://foo/bar # search starts in drive C
    import ~/foo/bar   # search starts in home folder

The load path is specified at startup and can not be changed over time.

`import` only works in top of a file (except comments).

## Dynamic loading

See [Dynamic Loading](dynamic-loading.md).
