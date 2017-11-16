PEG syntax runnning on a semi-structured token stream

The entrance rule must be the same as bounded context name.

    peg Main = [
      Main : .... {...} / ... {...};
    ]

    peg String = [
      String : ....;
    ]

    peg Regexp = [
      Regexp : ....;
    ]

The block `{ ... }` is the evaluator registered on that specific rule branch.

### Lookahead predicates

    &some_rule # expect look ahead
    !some_rule # negative look ahead

[design NOTE] Looking ahead regexp is not allowed, but we can use regexp lookahead in lexer to generate distinguished tokens.

[TODO] since we have tokens that can distinguish things, remove lookahead?

### Left recursion

There are several left recursion extension proposals for PEG, for example, this one:

http://www.inf.puc-rio.br/~roberto/docs/sblp2012.pdf

The impl requires a stack when starting to `inc` parse a rule
https://github.com/pegjs/pegjs/issues/231#issuecomment-54756322

We don't do left recursion, focus just on left-associative interpretation of the rule

    A : B C { bc } /* op D { } # $1 = acc, $2 = op, $3 = right

Same for destructive right-associative meaning

    A : B C { bc } /* op D { } # $1 = left, $2 = op, $3 = acc

There is also `/+` and `/?`, and they are transformed the similar way as `/*` does

    A : B /* C  ==>  A : B C*
    A : B /+ C  ==>  A : B C+

Further, we may introduce a dynamic operator system.

[design NOTE] parser combinators `<<` and `>>` is not implemented, since we can not omit them in prettyprinting. We have extracting mechanism to simplify the work.

[design NOTE] we don't need right associative rules, since we can do this and it is right associative:

    A : B A / B

### Branches without action

A default action is used, which rewrites the ST node name (with context and branch info) into the rule name (without context and branch info) and returns the ST node.

### Building AST nodes in rule callbacks

It can just take the value associated on the token. By default the associated value is a string literal, but you can customize it by setting value in `:token` call in lexer.

    some.token { $1 }

To build a node, use node creating syntax

    { Foo[attr1, attr2, attr3] }

Or a list building syntax

    { [$1, $2, $3] }

Splat building list

    { [*$1, $2] }

Or recursively

    { [$1, Foo[$2, [$3, $4]]] }

Node has to be declared with `struct` instruction or you will get "class not found error"

    struct Foo[foo, bar, baz]
    peg Bar[
      Bar: foo { Foo[attr1, attr2, attr3] }
    ]

Function calls and variable visits are forbidden in syntax action since they must be pure. (TODO in future we may thunk function calls?)

An empty brace ignores the captures and returns nil instead

    { }

### Rule with epsilon branch (nullable rule)

It is OK to append an rule with epsilon branch, but then this rule can not be used in left-most position.

For example, rule `Foo` is nullable:

    Foo: foo / EPSILON { }

But then the following rule will raise compile error:

    Bar: bar* Foo # an EPSILON is possible to be put in left-most position

TODO: loosen this limitation?

The special rule `EPSILON` is useful in simplifying rules when you want to ignore something and build a list. For example, consider you want to ignore the end of lines and return a list of statements inside a pair of parens:

    Parened: paren.left Stmts paren.right { $2 }
    Stmts: Stmt Stmts { [$1, *$2] }
         / eol Stmts { $2 }
         / EPSILON { [] }

### Reverse finding matched item

Sometimes a rule contains too many items, and it is hard to find the number. Counting from right may help.

    $-1 # the rightmost item
    $-2 # the item on the left of the rightmost item

### Operator table

    var<> $t  # operator table, configured with outside code
    lex Main = [
      $t
    ]
    peg Main = [
      A : B /$t # apply operator table to B
    ]

config will be like

    $t.op $/r +/ {assoc: 'left', prec: 20, atleast: 0} \acc op e, ...

### Precedence

All branch operators `/`, `/$`, `/*`, ... are of the same precedence, which is lower than the space joining items into a sequence.

### Invoking rules from other parsers

    peg Main = [
      Main : ... {...} / OtherLang.Main;
    ]

### Prettyprint function

For code formatter. The function has no effect on real compiler.

    :pp

### On ST and AST

ST is the syntax tree that has 1:1 correlation to the parser definition. ST can be used in formatting code (pretty printing).

The language definition should be able to ouput ST or AST, for different purposes.

the `*`, `+`, `?` quantifiers return node arrays, may use `:at` function to extract members from it.

### Traverse Order

Is defined by the eval function of each node.

### Compiled byte code

Each rule is expanded to simple 3-layered inlined structure of `Or -> Seq -> Many`

    Main:
      push Main1
      match-token # if match, push arg stack, else pop pos-stack and goto Main1
      match-star  # if match, push arg stack, else pop pos-stack and goto Main1
      node
    Main1:
      push Main2
      call String # manipulate rule stack
      ...

Inside each context, we have input (token-length * rules) slots for cache.

### Todo

To embed earley parser rules?
