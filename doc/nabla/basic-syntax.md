# Literals

## integer numbers

underlines in between digits will be dropped.

    1_234_567

oct numbers have a `0o` prefix, so that number parsing won't make mistakes on numbers that start with `0`

    0o123

## float numbers

hex numbers with scientific notions

    0xFFp3 == (0xFF * 2 ** 3).to_f

## (literal) suffix flags

rational suffix

    3/4r

imaginary suffix

    2i

[design NOTE]: since we don't have `/.../` regexp, there is no regexp suffix flags (o x u i g m etc), just use `(?i)` and `(?m)`

## strings

single quoted, the only escapes are `'` and `\`

    'a\'string\\'

double quoted, can contain escapes, hex, unicode

    "a\n\x33\u{2345}"

and strings can take multiple lines

    '
    oh
    yes
    '

the spaces are significant

## ranges

    a .. b  # b inclusive
    a ... b # b exclusive

since `..` and `...` possess very high precedence (only lower than `.`), adding paren around it is very common pattern (see below for operator precedence)

    a.b (...) c + 1

## other direct literals

    true false nil

## constants and variables

names starting with upper case is constant, they can not be re-assigned

    A = 3
    A = 4  # error
    A = 3  # OK, no conflict and no effect (this design makes `load` a bit easier)

names starting with lower case is variable, they can be re-assigned

    a = 3
    a = 4 # not shadow naming

Variables better be named in snake case. Examples:

    foo
    bar3
    foo_bar
    fooBar # still OK, but not recommended as code style

The local environment is mutable, local vars can be changed inside a block (sub), but such kind of sub can not be turned into a lambda

    a = 0
    sub1 = do
      b = 3
    end
    sub2 = do
      a = 3
    end
    Lambda[sub1] # good, sub1 doesn't change local variables outside
    Lambda[sub2] # error, sub2 has side effect on local variables

If you need to store blocks for futher use, it is better for the method to require lambda instead. When a local scope ends, the reference count to the associated blocks are checked -- if more than 1, it raises an error.

Can use `var` to force declare variable instead of search up captures

    a = 0
    do
      var a = 3
      a # 3
    end
    a # 0

[design NOTE] Just check the type is better, error message is easier to understand than "block can not be converted to lambda because it contains mut-assignment in line ...".

## anonymous value `_`

if it is "left value" which lies on the left of assignment or matching-assignment, then it has no effect. (see also pattern matching)

    [_, x] = [3, 4]
    x as _ ~ 4
    x as _.f ~ 4 # syntax error
    _.f ~ 4 # syntax error
    x._ ~ 4 # syntax error

if it is a "right value", then it represents value of expression in the context. (good for repl)

    3 + 4
    _ * 5

# Misc

## comments

line comments

    # foo
    # bar

block comment

    #<<
      foo
      bar
      baz

block comment with preprocessor

    #<<html
      foo
      bar
      baz

    #<<markdown
      foo
      bar
      baz

    #<<asciidoc
      foo
      bar
      baz

## comma, new line, semicolon and `end`

`,` means new line

    [a,
      b, c]

      if foo, bar;

  `;` means `end`

# Operators

## operator precedence

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

## on prefix operators

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

## chaining binary operators

    a.< b c   # a < b and b < c
    a.+ b c   # a + b + c
    a.* b c   # a * b * c
    a.&& b c  # a && b && c

NOTE: the methods are defined with arbitrary members, but VM can optimize them.

[design NOTE] since the rules for `<` is different from `+`, so we should not auto translate them into chained by syntax, it is too strange to make many special cases.

# Control structures

if cond

    if foo1
      bar1
    else if foo2
      bar2
    else
      bar3
    end

while loop

    while foo
      bar
    end

wend loop

    wend
      bar
    while foo # perform loop body first, then check condition

for (see below for for section)

    for
      a <- as.each
      select a * 2
    end

`case` ... `when` (see pattern-match)

[design NOTE] we don't need mod clauses, just use `,` for one-liners, and it is easier to modify

    if a, b;
    if a, b, else if c, d, else e; end

# Data structures

## array and hash literals

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

## alternative collections

### assoc array

assoc-array (backed by array)

    AssocArray[
      a: a
      b: c
    ]

it can store repeated values

    foo = AssocArray[
      "a": 1
      "a": 2
    ]
    foo.add "a", 3

assoc-array is aware of insertion order

    foo['a'] # 3

assoc-array can be used as a multiple array.

    foo.values_for 'a' # [1, 2, 3]

Iterating will go through all stored values. The multi-array feature makes it suitable for storing cookies, request params, and http headers.

And it also has `@=` and `:delete!` methods to update or remove existing values under a key.

When elem number is very large (hundreds or so), assoc-array may become slow for random access, but methods like iterating is still very fast.

### set

Set (backed by map)

    Set{a, b}

### dict

dict has little memory for large string dictionary, but unordered

    Dict{ ... }

### rbtree

rb tree, find is by default binary search

    RbTree{ ... }

can be used as priority queue

### pattern matching collections

Left and right must match the type exactly.

If the right pattern is `[]`, will call `.to_l` of left.
If the right pattern is `{}`, will call `.to_h` of left. (`Set:to_h` will expose the internal hash map, and the values are `true`)

### design notes

[design NOTE] list is not really useful: slicing, deleting, or concatenating 2 immutable lists still require O(n) operations
RRB tree may be a good option for list impl.

[design NOTE] there is no need for a linked hash map, an immutable one is very slow. For mutable impl, redis may be a better choice.

## splat operator for generating new arrays

To unroll an seq-like data structure, use `*`
To unroll a map-like data structure, use `**`

    [a, *b, c]
    {"a": a, **b, "c": d}

NOTE it can also be used in patterns

## data types (custom records)

[design NOTE]: adding ivars to core data types is not allowed (so allocations and copying are easier), however, it should be easy to delegate methods to them.

`data` declares data type, fields list can be type checked

    data Foo
      $r/\d+/ ~ a
      Integer ~ b
      String ~ c
      d, e, f # 3 fields without type checker, note that `,` has the same meaning with new line
    end

in a `data` type, you can inherit other data types, and fields with the same names will overlap the previous one.

    data Foo < Bar # think "<" as "⊂"

but inheritance is limited only in data fields, no behavior will be inherited, and only single-inheritance allowed.

`data` can not be opened after definition. but `class` can.

`data` defines default getter and setters, and they are `final`.

ways to new an object

    Foo[1, 2, 3]   # ordered, new with operator []
    Foo{"a": 1, "b": 2, "c": 3, "d": 4} # looks like a hash, but much light weight
                                        # no operator `{}`: it is a built-in syntax
    Foo[*some_array]
    Foo{*some_map}
    Foo[-> a, ...;]

    # the prototype way:
    bar = foo{"a": 3, "b.c": 4} # overwrites members

When a field ends with `?`, it is converted and stored in boolean

    data Foo
      a?
      b?
    end

    foo = Foo{1, nil}
    foo.a?    # true
    foo.b?    # false
    foo.b = 1 # Foo{true, true}

[design NOTE]: ADT's sum type doesn't fit in dynamic languages because values are summed type of all types. And in OO language, sum type is polymorphism.

under `data` we can not define methods

[design NOTE]: if we allow methods defined under `data`, then the difference from `class` is unclear (let `data` mutate members? but how about more members?)

[design NOTE]: the nabla object serialize format with class support:

    {"foo": Date{"year": 1995, "month": 12, "day": 31}}

It is pure text, a bit readable, and can be parsed (with only the value rules, no other operations allowed).

## `for` constructor

It is as expressive as (or more) applicative do or list comprehensions.

For example:

    sums = for Array
      a <- as.each
      [b as Integer, *_] <- bs.each_slice 4
      select a * b
    end

`for` may also be used in other data types

    for Point
      x <- coords.each
      select x
    end

    for Foo
      [k, v] <- kvs.each
      select k => v
    end

since this form is not very pleasing:

    do
      ...
    end.call

we can `for` without `select` instead

    for
      ...
    end

example with IO:

    for
      h <- IO.open 'f' 'w'
      line <- a.read_line
      if /foo/.match? line
        h.write line
      end
    end

or simplify with pattern match:

    for
      h <- IO.open 'f' 'w'
      [line as /foo/] <- a.read_line
      h.write line
    end

example with try (`Object[o]` yields `o`, and `Object[]` yields `nil`):

    for Object
      a <- b.try
      select a.foo
    end

but there is no "state monad", since we only build result in last phase

[NOTE] we may make many things applicatives...

the "if" applicative

    for Object
      <- :if foo
      select bar
    end

the "while" applicative

    for Object
      <- :while ->, foo;
      select bar # only the first works
    end

    for Array
      <- :while ->, foo;
      select bar # all elements are collected into the array
    end

[NOTE] we don't have elvis operator `?.`, it makes syntax hard to recognize when combined with question mark method names. but we can specify syntax for this kind of visits, see custom-syntax for more information.

[TODO] it is like fmap ... should we use `map` instead? or simpler, `for` ?

[impl NOTE]:

We add an implicit continuation arg in lambda calling, with it we can aggregate data into the primary stack.

See also https://ghc.haskell.org/trac/ghc/wiki/ApplicativeDo for applicative do notation

[design NOTE]:

- It is in essential a monad
- In the block is just normal nabla code, with just one addition: `select elem` or `select k => v` (NOTE we should disable the syntax `select k: v` since in this case k is usually string)
- if we use `collect Array[...]` syntax, then it becomes a bit ambiguous with `Array[...]`, and we know the languages between
- it is so more like a control syntax instead of a constructor syntax
- the syntax is a relieve for lambdas not being able to change outer variables
- if simplify `x <- xs.each` to `x <- xs`? (calling `bind` method). but then it this expression is ambiguous: `x <- xs.foo bar`.

[TODO] in monad comprehension there can also be `take` and `group by`
https://ghc.haskell.org/trac/ghc/wiki/MonadComprehensions

# Behavior types (class)

a data type is inherently a behavior type, but there can be behavior types that are not data types

    class Foo
      include Bar
    end

the name being included must be class

there is only one kind of inheritance via `include`, methods defined in `include` module is looked up hierachically

to inline the methods defined in Bar

    class Foo
      include *Bar
    end

the code above is a macro `include`, all methods searchable in `Bar` are inlined and may overwrite defined methods in `Foo`. if `Bar` is re-opened afterwards, `Foo` will not be affected at all. This is useful when you need to make sure methods defined in `Bar` are searched first.

NOTE: be careful when using it, if some source file has `include *`, it is not reloadable. (in future we must develop some a bit more complex mechanism for reloading them, but only raise error when we meet `final` modifiers)

`class` can be re-opened, but `data` can not.

classes can also be scoped

    class Foo
      scope :bar
        include Bar
      end
    end

scoping is a way to separate concerns. `scope` in fact creates a new class and provide ways

    data Foo
      a
    end
    class Foo
      def a; # no way: can not overwrite final method
      scope :foo
        def a; # ok, since scope is an implicit sub data type
      end
    end

The goodness: one data type can re-use other data-types methods.

and can delegate methods

    class Foo
      delegate :bar as Bar # delegate on bar, can search methods on bar (will do a class check of Bar)
    end

we can also use splat macro on `delegate` (TODO and it has the same reload problem as with `include *`)

    class Foo
      delegate :bar as *Bar
    end

delegate is like `include`, and `class_of?` checks both `include` and `delegate`. for specific checks, use `class_include?` and `class_delegate?`.

how about if we delegate only a part of the methods? -- we don't

    class Foo
      ['foo', 'bar', 'baz'].each -> x
        :def x -> args
          :bar.send "#{x}" args
        end
      end
    end

the order of behavior type and data type can be switched

    class Foo
      ...
    end
    data Foo
      ...
    end

[TODO] do we add syntax for `import Behavior` so methods in the object can be limited?

## defining method

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

[design NOTE] We should not impl `local def`, it can be quite complex if we impl dynamic method calling.

## object path changing

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

## special methods

    def hash

    def eql?

[design NOTE]: if we use, `def_hash`, `def_eql` may introduce too many keywords and syntaces. though the other way introduces more special rules.

[style guide]: `.build` is a convention for customized constructors. For example

    def Foo.build a
      Foo[a * 3, 4]
    end

## default params

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

## default params can be put in reverse order

    def foo a=3 b=4 c
      ...
    end

Arity is still the same as normal order (`1..3` in the above example). `:method('foo').reverse_default_args?` is `true`

[design NOTE]: there is no arbitrary arity, the syntax is harder to fit in.

## matching args

see pattern-match

## method calling

method is the boundary of lexical scoping for local vars

`:` lookup passes object, then goes to `Kernel` (it `extend self`), so methods on `Object` is much fewer.

    foo.bar
    :bar
    ::bar

chain: right-value `_` represents the last expression value in the same block

    foo.bar baz xip
    _.lower baz xip
    _.continue baz xip

composed

    foo.bar :baz xip

not composed

    foo.bar (:baz) xip

[design NOTE]: do not support `(.meth)`, because then we may induce `(:meth)`, and it is ambiguous

[design NOTE]: keyword `self` instead of `@`, to free the usage of `@`

## method search rule

search latest defined, then latest included

    class A
      include M1
      def foo
        ...
      end
      include M2
      def foo
        ... super # search first foo, then M2, then M1
      end
    end

## the final modifier

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

but note the exception: macro `include *` inlines all the method defs including `final` declarations!

    class Child2
      include *Parent
      def foo; # bad
    end

you may use `include *` for the purpose of freezing methods, but be careful, `include *` also makes the file not-reloadable.

NOTE: it is mainly used for certain optimizations to work, but not recommended to be used everywhere. source files with `final` can not be reloaded!

NOTE: there is no modifiers like `private` and `protected` -- we can always use `scope` and `delegate` to reuse code while keeping concerns separated. and they are not easy to test. for some cases of private/protected methods, we can also use lambdas.

[design NOTE]: the modifiers are not methods? `:final`, `:include`, `:delegate`, `:scope` makes compiler optimizations harder, and not as easy to write either.

# Lambda and subroutines

## lambda

can capture local vars but can not change it

    -> x
      x + 2
    end
    -> x y, x + y;

recall note: comma (`,`) means "new line"

quick lambdas (`->` followed by bracket)

    ->(4 /)
    ->(+ 3)
    ->(.present? 3)
    ->(4 + )
    ->(*)

[design NOTE]: we can not remove the `()`, because it may give ambiguity on `-> *args`, and with `()` it is more feasible. And `()` is more unified.

curry

    -> x y
      x + y
    end
    l = _.curry
    l[1][2]

for lambda

## subroutines (todo clean it)

a subroutine can modify captured variables, and can use `next` and `break`. but the life of a block ends with a local scope.

similar to lambda, but start with `do`, for example

    arr.each do e
      :print e
    end

in lambdas, `return` ends the control flow, and captures are snapshot and immutable

in subroutines, `return` ends the control flow of wrapping lambda or method, and can change local vars outside subroutine
in subroutines, `break`/`next` can control iterations, but in lambdas, they are forbidden if not wrapped in subroutine.
in subroutines, the arity is not enforced.

subroutine's life ends with the call receiving subroutine or local var (`some_local = do, ...;`), if ref_count > 1 at it's end of life, it raises an error.

iteration methods can use either subroutines or lambdas.

more usages

    s = do
      ...
    ;
    s.call # return value is pair of ['break', 3], ['ret', 4], ['next', 5], ...

[impl NOTE] `break` and `next` in if/while are compiled differently than subroutines.

NOTE nesting lambda and subroutine

    a = 1
    ->
      do, a = 2;
      _.call
      a # 2
    end
    _.call
    a # 1

`a` in lambda capture is changed, but outside of lambda, `a` is not changed.
compiler should generate warning: assigning local variable inside closure has no effect for the outside (lower warning level just warns the subroutine-in-lambda change, higher warning level warns lambda change)

thought C-API of subroutines

    // use return value to control the flow, C-ext usually use this
    int cb(Val e, Val* udata) {
      if (...) return ibreak;
    }
    arr_iter(arr, undef, cb);

    // iter with code obj, used in lang impl
    // in byte code, `break`, `return` will create control flow yields,
    // and the loop captures it, cleans up resources, then re-throw a `return` -- yes, every method body captures the `return`
    arr_iter_code(arr, undef, code);

## on non-local jumps (todo clean it)

`break`, `next`, `return` goes one jump buf chain

for every method that requires resource cleanup, adds a jump buf chain

todo: make a setjmp/longjmp prototype for impl of coroutines [see](http://en.wikipedia.org/wiki/Setjmp.h)

`do` starts a block, inside the block, variables can be altered
`->` starts a lambda, lambda just snapshots outside variables

simple rule: break / next are only allowed inside a block, so

    while xx
      ...
    end

is in fact:

    while xx do
      ...
    end

blocks hold reference count (just for check) but it dies when containing block terminates

blocks are more common than lambdas, but only lambdas works when we need to store them (register callbacks for example)

    :register some_obj -> x y
      ...

NOTE block is mutable object so it errors instead of giving segfault. mutable objects are immutable pointer with reference count, and a pointer to mutable memory

## shadow naming and variable scoping

shadow naming means the variable is in fact a different one than the original variable

    a = 4
    do
      a = 5 # not shadow naming, changes local var
    end
    ->
      a = 5 # shadow naming, does not change local var
    end

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

## back arrows

back arrows generates nested lambda

    do
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

there is no back arrow for do-block (we recommend immutable ways, when we really need to change local vars inside block, then use `do` directly)

## matching args

see pattern-match

## namespaces

Every class also acts as a namespace

    class Foo
      class Baz
        Bar # searches Bar inside Baz, if not, search under Foo, if not, search top namespace
            # if not found: raise error - constant 'Bar' not found under 'Foo::Baz'
      end
    end

Change constant/macro searching namespace

    using Foo::Bar::Baz
      ... # constants and macros are searched from Foo::Bar::Baz instead
    end
