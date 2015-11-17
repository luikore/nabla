# Comments

From `#` to the end of the line.

# Predefine some common pattern: `pattern`

Many rules may share some common parts. To reduce duplicated code, we can alias some regexps with `pattern`, then reference the aliases in rule definitions.

    pattern @foo = /regexp/
    pattern @bar = "string"

## Global variables (declare in top level)

Decl:

    var $name

Global vars are only available in lex blocks.

NOTE: Global var also marks all units in the file not reusable

## Local variables (declare inside lex block)

The lexer can store and make use of several variables, you need to declare them in the begin callback.

Variables represent current lexing state, and point to immutable values. For editor, this lexing state is stored alongside with history.

    var name

Local variables are only available to current lex.

# Begin callbacks

`begin` callback is invoked before parsing

    begin /some-pattern/ {
      # init vars
      # and copied for incremental parsing save points
      var some_var = "foo"
      var some_array = [3, "foo"]

      # config lexer behaviors
      :token_class "tok" ".some-klass"
      :fold_context "SomeContext"
      :fold_pair "tok-beg" "tok-end"
    }

Only top level language can use begin without pattern.

NOTE `begin` callback can contain no pattern, just for var initialization 
NOTE `begin` callbacks should be put on top and it is not matched in the lexing loop.

# End callbacks

    end /some-pattern/ {
        ...
    }

if `/some-pattern/` matches, the callback code is executed and return to the parent state.

NOTE There can be multiple `begin` or `end` callbacks.
NOTE `end` callbacks are sensitive to matching order
NOTE `end` callback must contain a pattern

[design NOTE] if we use `:return` in code and remove `end` instead, the meaning of `:return` is not consistent. for the ordering freedom of `:return`, we can use lookahead to achieve the same goal.

[design NOTE] Should we use parallel hooks? No, we can use look-ahead/after groups instead.

# Lexing Contexts: `lex`

All lexing rules are defined under a context beginning with keyword `lex`. First character in a context name must be capitalized. The entrance context is `Main`.

    lex Main = [
      # rules here, end callback is optional
    ]
    lex Foo = [
      # rules here, end callback is required
    ]

We may also like to re-use some rules from an existing partial context, partial context names begin with a `*` for example:

    lex *Bar = [
      # rules here, :return action is forbidden
    ]
    lex Foo = [
      # rules before
      *Bar
      # rules after
    ]

NOTE: you shall not use repeated context names.

Contexts may set a default regexp flag (TODO)

    lex Foo(?i) = [
      # ...
    ]

To use context names from other languages (TODO)

    *C:Foo

# Types of State Values

- nil: `nil`, default to 
- string: `"foo"`
- matched string: `$2`, which inherits string, but with `:begin` and `:end` methods
- boolean: `true`, `false`
- integer: `123`, `-1`

more types are todos:

- color?
- ...

# Regexp

quantifier meanings

    +   ?   *   greedy
    ++  ?+  *+  possessive
    +?  ??  *?  reluctant

Many aspects are the same as Onigmo except (todo elaborate it):

interpolate predefined pattern, note that groups in the pattern are ignored

    {@pat}

interpolate global predefined pattern (NOTE it should not be a `\p{}`, since it may consist of more than 1 chars)

    {@Graphene}

interpolate predefined char class (unicode char classes)

    \p{Han}

to achieve the back-reference effect, we can also store a state variable and reference it.

    lex Normal = [
      /<<-(\w+)/ { heredoc = $1, :call 'heredoc' }
    ]

    lex Heredoc = [
      { :match heredoc, :return }
      /./ { ..... }
    ]

[impl NOTE] interpolations of vars and captures are not handled by regexp engine, it is very hard to make an efficient yet simple engine with backreference (further reading on engine with better backref: http://ect.bell-labs.com/who/knamjoshi/papers/robustness-infocom10.pdf --- modified Thompson's VM and use liveness analysis to detect maybe-too-complex cases in compile time).

[NOTE] for syntax simplicity, only interpolating var is allowed, no interpolating methods. to achieve the effect, pre-compute the value you want to put:

    { :push v $2, v_top = $2 }
    { v_top = :pop v }

set ignore case flag

    (?i)

set case sensitive flag

    (?I)

set lang agnostic ignore case flag (todo)

    (?i:German)

set encoding flag (the default is utf-8, only utf and iso-8859-1 are available) (todo)

    (?e:utf-8)
    (?e:utf-8-normalize) # use normalization form canonical composition (NFC) in matching regexp
                         # (but matched string is not normalized), useful for editing legacy text
                         # in this case, `.` matches a grapheme instead of a char

set multiline flag (default), in this case, `.` matches everything (todo)

    (?m)

set non-multiline flag, in this case, `.` matches `[^\n]` (todo)

    (?M)

anchors:

- `^` beginning of line
- `$` end of line
- `\b` word boundary
- `\B` not word boundary
- `\a` beginning of file
- `\A` not beginning of file
- `\z` end of file
- `\Z` not end of file

misc notes:

- `.` can match `\n` or `\r`.
- `\s`, `\d`, `\w` ... are not affected by unicode chars, for matching all kinds of word chars beyond ASCII word chars, use `\p{Word}` instead.

special chars:

- `\t`
- `\n`
- `\r`
- `\f`

hex char

- `\xAB`

unicode char with 4 digits

- `\uABCD`

unicode char with digits more or less than 4

- `\u{ABCD1234}`

special char groups:

- `\s` space
- `\S` non space
- `\d` digit
- `\D` non digit
- `\w` word
- `\W` non word
- `\h` hex digit (case insensitive)
- `\H` non hex digit

[Design NOTE] On lookaheads
NOTE that we still need to look ahead for definitive yielding tokens even without supporting lookahead.
For parser, we have the whole input data and loop until a real match, so no need to worry lookahead.
For syntax highlighter, we loop visible text for building the DOM AST (then use dom diff to color).

# Rules

Each rule is in this form (multiple patterns will be recognized as capture groups):

    /pattern1/ "pattern2" /pattern3/ {
      # actions
    }

The code inside `{` ... `}` is the actions to perform after the match.

Actions are in a simple lisp-like language.

For example, the code below invokes an action named `:token`, which may produce a token for the lexer, or highlight for a text editor with predefined styles.

    lex String = [
      /\\\h\h/ { :token "escape" $0 }
    ]

[design NOTE] Similar to QUEX a `=>` to simplify the case for `:token "escape" $0` ? but it makes the syntax more complex and not very consistent, plus not flexible for ordering `:return`s, so no need to add it.

In actions, you can:

ref capture group

    $1 $2

ref whole text

    $0

use or set values for var

    x
    x = true
    x = false

## Consecutive patterns

The rule is matched with the first pattern, and then expects the second, and then expects the third, ...

If any of the patterns not match, will report error with expectations.

    /pattern1/ { action 1 } "pattern2" { action 2 } /pattern3/ { action 3 }

## The `:token` Action

It emits a token for parser, or query a matching style for the string segment. It takes 2 parameters. The first is required token type name, which will be used to classify the whole matching string. The second is optional for the segment of matching group. For example,

    "true" { :token "const-true" }

This rule matches string `true`, and generates a token named `const-true`.

Another example,

    /(alias)\ +(\w+)\ +/ {
      :token "keyword-alias" $1
      :token "alias-name" $2
    }

This rule generates 2 tokens: `keyword-alias` and `alias-name` with corresponding matching string (and matching positions), while ignoring the arbitrary lengthed spaces in between.

### On token naming

Tokens is best to be named in top-down categorized form -- for classification convenience and syntax highlight styling.

Assume we have a token `"grandparent-parent-child"`, the styles defined on `"granparent"` will be available on `"granparent-parent"` and styles on `"granparent-parent"` is available on `"granparent-parent-child"`.

## Dynamic match action

It can match a stored or computed value to the input flow, if failed, pass the block on to next regexp or block

    :match :close_paren :top paren_stack

## The `:style` Action

First usage is like `:token` action, but is used only for syntax highlighter. It has no effect in compiler lexer.

For dynamic styling with CSS

    /\#\h+/ {
      :style 'color' $0
      :style 'background' (:contrast_color $0)
    }

NOTE: under only very few conditions the `:style` action should be used. Many meaningless tokens are required in the AST for code re-formatting to work.

See more in syntax-highlighter-considerations.md

## Context Switch Actions: `:call`, `:return` and `:call_block`

It is easier to disambiguating tokens if divide your lexer into several major contexts. Especially for lexing strings which has several escape rules in them. To call another context

    :call "MyStringContext"

In that context you can return to caller by

    :return

A special action to invoke lexer for block, anchors like `^`, `$` will apply to the unindented text.

    :call_block <indent-string-for-the-block> <context-name>

NOTE: we should not use indentations instead of a special action, it will create many limits for the definition of the embeded language.

## Lists: `:cons`, `[]`, `:head`, `:tail`

The following 2 are the same:

    :cons 1 :cons 2 :cons 3 nil # [1,2,3]
    [1, 2, 3]

To retrieve member or part from the list

    :head [1,2,3] # 1
    :tail [1,2,3] # [2,3]
    :init [1,2,3] # [1,2]
    :last [1,2,3] # 3

## Which context does the token belongs to?

If a token is generated before `:call`, it belongs to the caller context, else it belongs to the callee context

    :token "foo" $1 # belongs to the caller
    :call Bar
    :token "bar" $2 # belongs to the callee

Same in `:return`

    :token "bar" $1 # belongs to the child
    :return
    :token "foo" $2 # belongs to the parent

## Helper Statements for Simple Tasks

error

    :error "message" # throws error and message here

error with expectation

    :expect "var-name" $2 # expect a "var-name" token in the location of $2, may also report the expected rule definition

compute the closing symbol for the arg

    :closing $2

you can use custom statements through api (todo)

## Considerations for More Helpful Errors

todo

## [Design NOTE] Pros and Cons of Action Language

Pros:

- Actions are in a safer sandbox, making optimizations and validations easier.
- Make it possible to define a unified lexer for language and syntax highlighting.
- We can keep all context vars in lexer, to keep the parser context-free.

Cons:

- Actions are limited at early implementations. For example, there's no conditional or cycle statements.
- Need to define actions in host language before using them.

## [Design NOTE] How about a simple lexer with flat state?

An alternative scheme, is to separate the actions and state changing code out, and only handle flat states in lexer. But it has some drawbacks:

- state change is un-checked if defined outside of lexer.
- when changing lexer or state-change code, it is prone to make them un-sync.

## Debug Statements

todo

## Action syntax summary

It is a subset of nabla for bootstrapping, and is translated into C.

When used as a library, it is extended to nabla and ST calling is changed to use ref-counted objects instead. NOTE that each time a new syntax is defined, the whole lexer/paser are re-generated.

Literals

    nil true false 123 -4

Match references (when match is string, the reference is untyped token)

    $1 $2 $-2

Yield variable (parser action only)

    $$

Assignment

    some_var = 3

Node

    NodeClass[foo, bar, baz]

Condition (still an action, but clauses are lazy evaluated)

    if <cond>, <true-expr>, else, <false-expr>, end

Built-in infix operators

    + - * / % ^ & | && || > < >= <= == != @

Built-in prefix operator

    !

Actions

    :token
    :call_block
    :call
    :return
    :error
    :closing
    :contrast_color

String

    :concat
    :size
    :bytesize
    :empty

Array

    :size
    :empty
    :push
    :pop
    :shift
    :unshift
    :nth

Call unification

    :push some_arr 3
    # is equivalent to
    some_arr.push 3

## Dealing with Indentations

#### Way 1: lexer guided virtual token

Define vars that are used in indents

    var $indent

Set the var when we have indent

    /^ +/ {
        if $0.size > $indent.size
            :token 'indent'
        else if $0.size < $indent.size
            :token 'dedent'
        end
        $indent = $0
    }

#### Way 2: `:call_block`

The builtin `:call_block` trims the leading indentation and provide easier indentation-aware parsing.

#### Way 3: parser guided

Since we have location info in lex tokens, we can let parser decide if the locations attached to the tokens satisfy some indentation constraints.

[design NOTE] Do we need some middle-level abstraction for indentation processing? I think the answer is "No". For many languages indentation works slightly different.

## Dealing with Heredocs

Heredoc strings are not easy to parse -- the delimiters match each other in a queued order. Here is an example in Ruby:

    puts <<HELLO, <<WORLD
      hello,
    HELLO
      world!
    WORLD

To lex it without too ugly code, first we need a variable to store match delimiters, and we need the `queue` behavior

    var $here_delimiter
    var $here_delimiter_q

Now match-and-return becomes simple:

    lex Main = [
      begin { $here_delimiter_q = nil }
      /<<(\w+)/ { $here_delimiter_q = :cons $1 $here_delimiter_q, if !$here_delimiter, $here_delimiter = $1, end }
      Heredoc
    ]

    lex Heredoc = [
      begin $here_delimiter { }
      end $here_delimiter { $here_delimiter_q = :tail $here_delimiter_q, $here_delimiter = :head $here_delimiter_q }
    ]

This is not beautiful code, but way better than making one huge regexp with backref and putting all patterns inside it yet not being able to recognize all kinds of heredoc forms (see https://github.com/textmate/ruby.tmbundle/blob/master/Syntaxes/Ruby.plist#L1757-L2331).
