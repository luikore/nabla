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

[design NOTE]

> Since we don't have `/.../` regexp, there is no regexp suffix flags (o x u i g m etc), just use `(?i)` and `(?m)`

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

[design NOTE]

> Method calls can not fit in the knot operators system. or `(:foo)` is ambiguous (it can mean call or looser method name)

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

[design NOTE]

> Since the rules for `<` is different from `+`, so we should not auto translate them into chained by syntax, it is too strange to make many special cases.

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

[design NOTE]

> We don't need mod clauses, just use `,` for one-liners, and it is easier to modify
>
>     if a, b;
>     if a, b, else if c, d, else e; end

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

[design NOTE]

> List is not really useful: slicing, deleting, or concatenating 2 immutable lists still require O(n) operations
> RRB tree may be a good option for list impl.

[design NOTE]

> There is no need for a linked hash map, an immutable one is very slow. For mutable impl, redis may be a better choice.

## Splat operator for generating new arrays

To unroll the rest of the data structure, use `*`

    [a, *b, c]
    {"a": a, *b, "c": d}

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

## Method calling

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

[design NOTE]

> Do not support `(.meth)`, because then we may induce `(:meth)`, and it is ambiguous

[design NOTE]

> Keyword `self` instead of `@`, to free the usage of `@`

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
    \(:foo)        # :\foo # arity depends of method foo
    \(:foo 3)      # (:\foo.curry 1)[3]

[design NOTE]

> If we use placeholder lambdas as in scala, then too many meanings are put onto `_`, while saving very few typings.

## Currying

    l = -> x y
      x + y
    end
    l.curry!
    l[1][2]

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

Back arrows can be used inside any syntax structures with `end` or `when` delimiter, the following are all valid syntax:

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
