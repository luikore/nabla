# `case` ... `when` syntax

[design NOTE] not `switch ... case` since it is more like functional `case of`.

    case a
    when X
      ...
    when Y
      ...
    when _
      ...

    case [1, 2, 3]
    when [Hash, ->(==).bind 3, ->(.present?)]
      ...
    when _
      ...

They call `obj.match? concrete` on individuals, which doesn't raise

the default `Object.match?` uses `==`

NOTE

* prog can warn dead match routines

## using `as` for splitting var and pattern

    case a
    when [head as Head, *_]
      ...

examples:

    a       -- var without match
    self.a  -- var without match
    Const   -- type
    1.2     -- type
    'asd'   -- type
    ->(.zero?) -- type
    [] {}   -- type (recursive splat open)

by default, locals and ivars are vars, other expressions are patterns. if need to change default positions, add `as`. the lhs of `as` is used as vars, and the rhs of `as` is used as types/patterns.

disambig examples:

    _ as $(x a 3)  -- literal as pattern
    _ as a         -- var as pattern
    _ as self.a    -- call result as pattern
    Const as _     -- const as left value
    a.b as _       -- calls a.b =

## matching and mass-assign

    <match-expr> ~ <expr>

raises error when not match. finally the `match?` methods of the objects on the rhs of `as` are called.

    [a, b as Int] ~ arr

the above code is equivalent to

    # match first, then assign
    if !(Int.match? arr[1])
      throw <match error>
    a = arr[0]
    b = arr[1]

or

    case arr
    when [a, b as Int]
    else
      throw <match error>
    end

case of const:

    Foo = 3          # assign
    Foo.match? 3     # matching returning matchdata, there's no match-expr, so no assignments (what about regexp?)
    Foo ~ 3
    _ as Foo ~ 3     # matching (can raise)
    Foo as Int ~ 3   # matching and assign (can raise)

## matching error

both `~` and `=` works as assignment operator, but:

- `~` throws error when not match
- `=` doesn't throw

## matching Map

example (use `*: rest_var` to match the rest)

    {"a": a as Hash, "*": b as 3, *: rest as (-> x, x.size < 3)} ~ {'a': {}, '*': 3, 'c': 12}

function params are NOT match exprs, they allow default assignments

    def f x y z=3 opts={a: 3, b: 4}
      a ~ Integer
      b ~ Object
    end

[impl NOTE] compiler and doc-generator should be able to extract the first several lines of matchers for further use

## matching Set

We can match a set to another set

    Set{a, b, 3} ~ Set{1, 2, 3}

It is turned into canonical form first

## matching arguments

When args are quoted with brackets, we are matching the arg array, and it means `if match then...`

    def foo [a as Integer, b as 3]
      ...

You may think, how about multiple matches? but nabla does forbid method overloading

    def foo [*xs]
      case xs
      when [a, b]
        ...
      when [a, b, c, *_]
        ...
      end
    end

When method is defined with matching args, the arity is `-1`

Lambdas and subroutines may also be defined with matching args

    -> [x, *xs]
      ...
    end

    do [x, *xs]
      ...
    end

The back arrow is also powered with matching syntax

    for
      [x as ->(.even?)] <- xs.each
      [y as ->(.odd?)] <- ys.each
      select x * y
    end

[design NOTE] can we make mismatch an error?

## Logic operations on matcher

The following methods are defined on lambda:

- `|` logic or
- `&` logic and
- `.neg` logic negate

We can them use like this:

    foo = do x, x > 4;
    bar = do x, x < 10;
    baz = ->(.even?)
    x ~ (foo | bar) & baz.neg

## On recursive matching

With subroutine referencing self. example matching integer array:

    foo = do x
      case x
      when [], true
      when [Integer, *_ as foo], true
      end
    end
    _ as foo ~ [1, 2, 3]

Another way:

    present = do [[Integer, *_ as foo]], true; # but be careful since matching arguments requires additional brackets
    foo = present | []
    _ as foo ~ [1, 2, 3]

With data referencing self

    data Foo
      parent as ->(.nil?) | Foo
    end
