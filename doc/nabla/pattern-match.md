Pattern match is an intuitive way to reduce many boilerplate code.

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
    when [Hash, \(== 3), \(.present?)]
      ...
    when _
      ...

They call `obj.match concrete` on individuals, which doesn't raise

the default `Object.match` uses `==`

NOTE

* prog can warn dead match routines

## Using `as` for splitting var and pattern

    case a
    when [head as Head, *_]
      ...

examples:

    a       -- var without match
    self.a  -- var without match
    Const   -- type
    1.2     -- type
    'asd'   -- type
    -> x where x.zero?, x * 2; -- type and convert
    [] {}   -- type (recursive splat open)

by default, locals and ivars are vars, other expressions are patterns. if need to change default positions, add `as`. the lhs of `as` is used as vars, and the rhs of `as` is used as types/patterns.

disambig examples:

    _ as $(x a 3)  -- literal as pattern
    _ as a         -- var as pattern
    _ as self.a    -- call result as pattern
    Const as _     -- const as left value
    a.b as _       -- calls a.b =

## `where` can be added after arglist for more constraints

    matcher = -> foo where foo > 4;

## The `.match` interface (TODO refine considerations)

Calling `.match` returns match data

For lambdas `mdata @ 0` is the return value

    mdata = (case -> v, when v > 4, v * 2;).match 5
    mdata @ 0 # 10

For struct `mdata @ 0` is the source

    Foo.match foo
    mdata @ 0 # foo

For `[]` and `{}` patterns the expression is expanded and no mdata.

For regexp `mdata` contains captures.

[TODO consideration] make head-match API for parser?

    mdata, rest = lambda.head_match stream

Then bootstrap parser should be written in nabla

    Foo = -> [Bar[v]], ...; /
          -> [Bar[v]], ...;

This form can be used for tree transform too.

## Matching and mass-assign

    <match-expr> =~ <expr>

Returns true when matched. finally the `match` methods of the objects on the rhs of `as` are called.

    [a, b as Int] =~ arr

case of const:

    Foo = 3          # assign
    Foo.match 3      # matching returning matchdata, there's no match-expr, so no assignments (what about regexp?)
    Foo =~ 3
    _ as Foo =~ 3    # matching (can raise)
    Foo as Int =~ 3  # matching and assign (can raise)

## Matching error

both `=~` and `=` works as assignment operator, but:

- `=` throws error when not match
- `=~` is as it looks like, doesn't care so much and only returns `false` when not match

## Matching a map

example (use `*rest_var` to match the rest)

    {"a": a as Hash, "*": b as 3, *rest: (-> x, x.size < 3)} =~ {'a': {}, '*': 3, 'c': 12}

function params do NOT match exprs, they allow default assignments

    def f x y z=3 opts={a: 3, b: 4}
      Integer = a
      Object = b
    end

to make a matching expression with default values, use the full-matching style

    def f{"x": x as Hash default nil, "y": y as Int default 3}
    end

to abbreviate left-value hash match spec, use a variable on the left side of the `:`

    {a: Foo} # equivalent to {"a": a as Foo}

[impl NOTE] compiler and doc-generator should be able to extract the first several lines of matchers for further use

## Matching a set

We can match a set to another set

    Set[a, b, 3] =~ Set[1, 2, 3]

But the order for `a`, `b` are not garanteed

## Matching function inputs

An empty `case` clause can replace arguments.

We can match against args when using `case def`

    def foo case
    when [a as Integer, b as 3]
      ...
    when [x, *xs]
      ...
    else # if no else branch, raises argument error
      ...
    end

When method is defined with matching args, the arity is `-1`

Lambdas and subroutines may also be defined with case

    -> case
    when [x, *xs]
      ...
    end

The back arrow is also powered with matching syntax, which makes this syntax more powerful

    make
      [x as \(.even?)] <- xs.each
      [y as \(.odd?)] <- ys.each
      pick x * y
    end

## Matching scope arguments

For scopes, it is the same as matching method arguments and they are defined as methods on the scoped object

    scope foo case
    when [x, *xs]
      def bar
        :xs
      end
    end

The `case` syntax also works for back arrows

    case <- arr.each
    when [x as \(> 4), y as \(< 3)]
      ...
    end

## Logic operations on matcher

The following methods are defined on lambda:

- `|` logic or
- `&` logic and
- `.neg` logic negate

We can them use like this:

    foo = -> x, x > 4;
    bar = -> x, x < 10;
    baz = \(.even?)
    x =~ (foo | bar) & baz.neg

## On recursive matching

With lambda referencing self. example matching integer array:

    # but be careful since matching arguments requires additional brackets
    foo = -> case
    when [x]
    when [[]], true
    when [[Integer, *_ as foo]], true
    end
    _ as foo = [1, 2, 3] # matches

With struct referencing self

    struct Foo[
      parent as \(.nil?) | Foo
    ]
