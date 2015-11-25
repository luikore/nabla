# macros

First simple usage is syntax sugar for visiting environment variables or match results.

    $file # sugar for $(file)
    $dir  # sugar for $(dir)
    $1
    $2
    $args # sugar for $(args)
    $(args)[1]
    $(args 1)

[design NOTE]: if we use `$lit[content]`, then we don't know if it is `$(lit)[content]`.

from other namespace (NOTE this initiates a constant search at compile time, so you must ensure the constants are already defined before loading file (or do we delay the compilation the first time the method is called?))

    Foo::$bar
    Foo::Bar::$(bar ...)
    ::$bar

with inline code

    $(bar ... )

The delimiters can be

    ()
    []
    {}
    <>
    ''
    ""
    ``
    ||
    //

Dangling with block code

    $<<bar
      ...
    Foo::$<<bar
      ...

[design NOTE] to support dynamic src? `{src: ...}$Foo::bar` is bad practice, if one macro syntax can not be checked at compile time, just use normal library calls instead.

[design NOTE] compile options before `$foo` should not be dynamic evaluated value, it should be part of the language:

    $<<c
      #pragma -L... -I... -fPIC
      foo() {
        ...
      }

if need some run-time values

    $<<c_inline.exec {env: ..., args: ...}
      int main() {
        ...
      }

To make the following mixed use comfortable, literals are designed to be noun-like, so inputs are put to the left side instead of the right side.

    :puts $file $line $1

Use macros from other namespace

    # macros search Net::HTTP first, then search Net, then search top level
    using Net::HTTP
      ...
    end

[design NOTE]: since literals are noun-like, if we need to inject some run-time variable as options, define a method on it and call. if we use a prefix notation to inject options like `{opt: 3}$foo`, then namespaced syntax require a weird tweak: `3Foo::$bar` -> `3$Foo::bar`, and doesn't help much: we don't need dynamic syntax, and most options must be determined at compile time.

[design NOTE] some places are cleaner and easier to do static analysis if design a syntax instead of macro. So instead of

    "code"$require
    FooModule$include

We should use

    require "code"
    include FooModule

## multiple dangling blocks

    :puts $<<foo $<<bar
      ... # foo
    ---
      ... # bar

[design NOTE] not implemented for the first release of nabla

## define syntax

    def $foo src compile_context
      :check_interp compile_context src

      # do the parsing, return a lambda at last
      -> exec_context
        ...
      end

we may attach something to the top of the lexical scope context

## define digit macro

for `$1`

    def $? digit
      ...

### the context object

for runtime retrospection

    context.file
    context.line
    context.locals
    context.set_local # should not let it set local, it violates lexical check
    context.get_const
    context.self

use `context.locals` if the lang need simple interpolation lookup. `#{}` is recommended but not forced.

the language should be able to expose several component parsers as library, so the parser can even interpolate complex expressions.

`src` processor should be abel to invoke part of the language parsers.

## some built-in literals

    $(env path)   # env var (ignores case)
    $env          # map-like
    $(global foo) # global var (note: delimiter has some limit due to perl-style extended syntax)
    $before_match
    $after_match
    $0
    $1
    $2
    $args
    $dir
    $file
    $line
    $col
    $(reg \d+)         # regexp
    $(str hello #{you}) # interpolates
    $(quote hello #{you}) # do not interpolate

string example

    $<<str
      hello #{you}

    $<<quote
      hello #{a} world

## custom syntax usage ideas and examples

### Regexp

    $r/a/

    $<<r
      ['"`/]

### Workflow engine

    $<<workflow
      ...
      yield 'state1' # this state can be saved
      ...
      yield 'state2'

### Data formats

Using the `#` interpolate syntax by default

    $<<json
      {"foo": x, "bar": #{y}}

    $<<yaml
      x: 3
      y: 4

    $<<csv
      a,b,c
      1,2,3

Depending on what we want

    String::$<<json
      ...

    Map::$<<json
      ...

    AssocArray::$<<json
      ...

### String processing

    $<<awk << "some file"
      ...src

    $<<tr << "something"
      12345-7
      asdfc-e

### Shell

    $<<sh
      ...

### Makefile syntax for build tool

### Constraint programming

    c = $<<constraint
      def sibling x y
        :parent_child z x and :parent_child z y
      end
      def parent_child x y
        :father_child x y or :mother_child x y
      end
      :mother_child "trude" "sally"
      :father_child "tom" "sally"
      :father_child "tom" "erica"
      :father_child "mike" "tom"

    c.query $<<predicate
      :sibling "sally" "erica" #=> true

pure methods can be captured as predicates

    def zero_sum x y
      x + y = 0
    c = $<<constraint
      :zero_sum x y
      y == 3
    c.query(x)

### Array programming

### C ext

C inline

    $<<c_inline.exec {include_paths: ..., lib_paths: ..., libs: ..., target: 'exe'}
      #include <stdio.h>
      main() {
          printf("hello world\n");
      }

C ext ffi (with default header and linking)

    $<<c.call "printf" "hello world"
      #include <stdio.h>

### Comprehension

NOTE: We have for syntax so no need comprehension in custom syntax

### Sequence inferring

step increment seq

    $(seq 1, 5, ..., 41)

step decrement seq

    $(seq 9, 8, ...)

product seq

    $(seq 1, 2, 4, ...)

fibonacci seq

    $(seq 1, 2, 3, 5, ...)

### Assertion

    $<<assert
      ...

### SQL

    x = 3
    $(select foo.a, foo.b from foos foo where foo.a > #{x})

SQL segment: `with`

lambda args:

    where = -> x, $(with where foo.a > #{x});
    $(select foo.a with #{where.call 3})

sql args (TODO modify to use PG `with` syntax):

    where = $(with (x) begin where foo.a > x end);
    $(select foo.a with #{where}(foo.b))

### IO open with protocols

    $(open ~/.vimrc).read
    $(open http://example.com).post

### HTTP header

    Net::HTTP::$<<header
      foo: #{x}
      bar: #{y}
      baz: #{z}

### Symbolic computation

Do not shrink symbols

Add `3x` syntax

Add syntax: `int[] <exp>`, `dif[] <exp>`, `sum[]`, `lim[]`, `product[]` and matrix

    $<<sym
      :factor 3x**2 + 2x + 1

    ${sym int[x] x (+) :cos x }
    ${sym int[x=1..4] x (+) :cos x }
    ${sym lim[x->INF] 1/x }

Note - for seamless integration with numeric integration, `$sym` can reference lambdas, and args are set as `$1`, `$2`, ...

    f = :method 'f'
    ${sym int[$1=1..4] f }

Greek names will be displayed in the original form

The expression only evaluates when output is required.

Note - the syntax in `$sym` is limited to calls, operators and constants.

## language composability considerations

We recommend `#{}` for interpolating, if not possible, `@{}`, `${}`, `{{}}` ... can be chosen

For the parsing interface, user only need to specify interpolation start and end symbol.

# annotations

Annotation macros are effective for compile-time AST transformation, an annotation starts with `@` and takes a whole line

    @attr_foo bar
    def baz
      ...
    end

Annotation can also be scoped within constant namespace

    Foo::@attr_foo bar
    def baz
      ...
    end

[design NOTE] args in annotation must be constants, if you need run-time AST transformation, just use normal methods.

## custom annotation

    def @foo
      ...

NOTE: difference between the definition of infix `@` method:

    def @ that
      ...

## ast transform examples

### optimize

The options are the same as command line compile options only without the leading `--`

    @attr_optimize $[w mutable-array static-dispatch static-type inline lazy]
    def f
      a = []
      3.times do i
        a.push! i
      end
    end

NOTE `lazy` means the optimization is invoked the first time the method is invoked instead of doing it at once, so some methods defined afterwards can be located.

### maybe

    @maybe 'a'
    def foo a
      x = a.b.c.d
    end

### Weak typing

treats everything as string, treat `0` and `""` as `false` in conditions, consider `nil` as `""`

    @weak 's'
    def foo s
      if s == 23
        ...
      if something.size
        ...
    end

### Attribute Unification

treats `a.b` as `a["b"]`

    @attr_unify
    def foo
      h = {"a": 3, "b": 4}
      h.a # use h["a"] if method `a` is not defined
      h.b = 5
    end

### Web server Actions

    @GET '/index.:format'
    def index key1 key2 # gets arg by name
      String ~ key1
      {a: {b: x}} ~ key2
      ...
    end

    @PUT '/update.json'
    def update
      ...
    end

[NOTE] We may lose some compile-time advantages for syntax simplicity...

# custom operators

to add new infix operators

    Kernel::Lang.add_operator "^_^" {precedence: 5}

then it can be used in `def`, `.` or `:` calling

    class M
      def ^_^ other
        ...

operator naming rule: `{Symbol}({Word}|{Symbol})*{Symbol}`

# custom comment processors

Default comment processor only do some minimal processing.

To define some other document based comment processor for more powerful document:

    # markdown comment processor
    Kernel::Lang.def_comment_processor "md" -> src obj
      ... return a comment obj or nil for non-doc identities
    end

    #<<md
      **foo**

      - bar
      - baz

comment object

    comment_obj.to_term # to ansi terminal (colorful)
    comment_obj.to_html # to html
    comment_obj.assoc "A::B::foo"

you can use `assoc` method to document macro methods

NOTE for cross reference, a doc page contains simplified links which can be re-written by js, so no need to consider too much on it now.

# nesting

If the macro preprocessor is an update to the main syntax, we may use some additional tricks

    $<<foo
      $1 # we may make all macro search under Foo first, then the top level
      $<<bar
        ...
