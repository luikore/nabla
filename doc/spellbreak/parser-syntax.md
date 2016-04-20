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

Lookahead regexps (this may add consulting calls to lexer, should not recommended it?)

    &@pattern_name
    !@pattern_name

### Left recursion

http://www.inf.puc-rio.br/~roberto/docs/sblp2012.pdf

The above impl requires a stack when starting to `inc` parse a rule
https://github.com/pegjs/pegjs/issues/231#issuecomment-54756322

We may just fall back to focus just on left-associative meaning of the rule

    A : B C { bc } >* op D { } # $1 = acc, $2 = op, $3 = right

Same for destructive right-associative meaning

    A : B C { bc } <* op D { } # $1 = left, $2 = op, $3 = acc

There is also `>+` and `<+` but no `>?` nor `<?`, and they are transformed the same way as `>*` does

    A : B >* C  ==>  A : B C*
    A : B >+ C  ==>  A : B C+

The angled bracket can be viewed as the grouping parenthesis.

NOTE `foldl` is similar to left assoc, `foldr` similar to right assoc

Further, we may introduce a dynamic operator system.

[design NOTE] parser combinators `<<` and `>>` is not implemented, since we can not omit them in prettyprinting. We have extracting mechanism to simplify the work.

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

Function calls and variable visits are forbidden in syntax action.

An empty brace ignores the rule and doesn't return anything

    { }

### Extracting

We may append a `!` to return the node as the action result. This is called "extracting".

Example:

    Foo : Xip Bar! / Baz { Foo[$1] }
    # is equivalent to
    Foo : Xip Bar { $2 } / Baz { Foo[$1] }

Only one `!` is allowed before every branch, and it can not be mixed with `?` or `*`.

[design NOTE]: Further, we may inline some rules and optimize out the branch ST node.

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

All branch operators `/`, `/$`, `>*`, ... are of the same precedence, which is lower than the space joining items into a sequence.

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
