Pdlex is lexing with pushdown automata.

# Benefits

- One rule definition for language implementation and syntax highlighting.
- JIT code, fast.
- Can parse files with mixed encodings.

# How to Use

Basic usage

```c
Pdlex* lex = nb_new();
nb_set_rules(lex, rule_str, rule_size);
nb_parse(lex, str, size);
nb_delete(lex);
```

We can feed in data continuously and feed data to `nb_parse` for many times.

To reset states:

```c
nb_reset(lex);
```

Visit lexer fields for current status (they can be saved or modified)

```c
lex->state // a hash of {state: stack_or_var}
lex->line
lex->col
lex->error // set if some error happens
```

# Instructions and Rules

See defining-rules.md

# Methodology

See methodology.md for more details about designing lexer in the "pushdown" way.

# How It Works

It's all dynamic.

- Read the rule definition (a string in memory).
- JIT parser with the rules.
- Parse with the parser.
