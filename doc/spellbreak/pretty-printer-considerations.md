Inline comments and significant spaces can easily mess the parser. But we still need them when making a code formatter, or, pretty printer.

To hide and show comments and significant spaces, we can attach these nodes to a prev ST node in the same line.
If there is no such line, we put the comment into token stream as a separate comment line (may need it when generating document or doing annotation processor).

    :attach_token "comment.inline" 1 # this works the same way as :token in syntax highlighter

All the "hidding inline comment" work is done by the lexer so parser should require too much thoughts for comments.
