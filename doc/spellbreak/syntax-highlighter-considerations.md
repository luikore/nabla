Pdlex is supposed to be used as a lexer for a language parser, and as a lexer for syntax highlighting.

Lexical styling is mainly done via the side effects of the `token` action. But if syntax highliter requires styling beyond semantic tokens, use `:style` action and the effect can be overlapped with tokens.

Paren matching is done via the `begin.` and `end.` pairs. If `token` has a `begin.` prefix, it will be matched with the `end.` one in the same prefix. Editor action for paren completion/matching can also take advantage of `begin.` actions.

Editor symbol table listing and folding can also be defined for `begin.` and `end.` tokens.

DO NOT do processing like decoding in lexer, it is not required in a syntax highlighter.

---

In the means of editor grammar, we need to add these functions (begin-block only):

- instruction `token_class "token" ".styling.class"` to connect token and styling (token names are hierarchical and separated by dashes. styles are just selectors from a css definition)
- instruction `fold_context "some-context"` folds a context
- instruction `fold "token-begin" "token-end"`
    NOTE: to fold indentation based syntax, may can use fake token
    NOTE: usually their name matches, but there is no limit to that, we also allow to add folding for unmatched ones like `"token"` and `"nekot"`
- vim-like `foldexpr` can be implement as a supplementary way for folding, see http://learnvimscriptthehardway.stevelosh.com/chapters/49.html
- we can include a context with `*SomeLanguage:SomeContext` ? (TODO consider more about it with action syntax)

- action `:style` to add styling for a segment while not affecting the code of tokens
- action `:symbol` to generate an entry in the function-list (this can use a markdown format or org-mode?)
    TODO: if we make lexer definition more fine-grained, it is not easy to find out which tokens are symbols, maybe a separate regexp?

---

Notes for syntax-highliter reparsing:

- lookahead patterns should be length-limited

---

[Thoughts] Should we forbid greedy operators because their results can not be cached? Or we just suggest to keep regexps short.
