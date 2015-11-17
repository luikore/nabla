# validations

- should not define contexts with name-clashes
- should not interpolate non-exist capture group
- should not use an undefined capture group (backref should not match it)
- should not use an undefined var name
- should not mutual include
- build context switching graph (a pop action generates arrows pointing back to all incoming contexts), and check orphan contexts.
- for the ability of stream parsing, look ahead/backward patterns should be length-limited, the invalidation range will be expanded by the length. (see also syntax-highlighter-considerations.md)

# warnings

- (in a same context) detect dead regexps that never can be matched and warn
- warn patterns like `\w+a` can not match `aa`
- warn atomic group if it is no use

optional warnings

- escapes of no use
- groups that is not captured

# utils for development

- list all possible tokens
- show context switching graph
- tool to convert CFG to pdlex
