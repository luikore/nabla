Stages:

## 1. lex -> semi-structured node

Token stream and reduced value is packed in context node.

Regexp match struct structure:

    regexp_code
    str
    len
    pos
    captures
    threads

Lexer meta struct structure:

    node_meta # for creating node_arena
    variables # with immutable default values, only for lexer use
    match_evaluators[]
    parser_metas {context_type: parser_meta}
    regexp_metas {context_type: regexp_code}

Lexer struct structure:

    lexer_meta
    node_meta
    node_arena
    ctx          # user passed struct for parser invocation
    input_stream # and current pos
    context_stack
    variables

Lexer loops matching on input string with current context.

When meet a `match-c` in regexp vm, invokes the function in address with captures, and return a token value.
(TODO library mode: if the evaluator is nabla, invoke `match` with instruction address)

In a `return` action, lexer pops top stream, but do not parse it yet. If there is no parser defined on that context, inline the popped stream into its parent.

## 2. peg parse tokens array

The parse VM may share some same instructions with lex VM, but it is parsed top-down, still no need to re-use code between them.

And the parse VM has an internal stack and memoize table, maybe should not reuse it with the nabla language.

For every context, there is a specific parser env created, but they share the same parser meta.

Node meta struct structure:

    evaluators # organized by round: [round_1_evaluators, round_2_evalators, ...]
               # if some node is not evaluated in a round, put NULL in the table
               # optimize: if some node's closure is not evaluated in a round, put id function address in the table
    attr and eval info for each kind of node

Parser meta struct structure:

    context_type
    code
    memoize_lookup
    node_lookup

Parser struct structure:

    parser_meta
    node_arena # for tokens and nodes
    st_arena   # for intermediate ST nodes (thunk params for AST)
    token_stream
    call_stack # rule expand stack
    memoize_table # array of type_id*pos(pos in token) to value
    bytecode_generator # emit to a peephole optimizer
    ctx

The parser is started by traversing the root token stream (root context node).

Some redundant st nodes may be generated when backtracking.

## 3. evaluator environment

Assume actions are idempotent. All context-sensitive operations must be operated in lexing.

## token stream

It is a dynamic array of:

    int id = token_id
    int pos
    int len
    int line

Or parsed node:

    int id = context_id # contexts are also registered as tokens
    int pos
    Node* node

When invoking parser callback, only the tokens being used are allocated as TokenNode.

In the PEG VM, matching external rules searches the context and id, and `match` instruction checks the id.

## memoize table

Rules in one context is numbered from 0.

## node registries

NodeMeta stores every node and token definitions, it provides for a cluster of languages, and can be freeed.

## action function arity

When registering function, can overload by parameter size

    :return    # do not parse, join the token stream with parent
    :return xx # parsed result

But arbitrary arity is not allowed.
