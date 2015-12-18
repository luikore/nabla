# on contextual thinking

Adding layers can simplify the job, but may slow down the lexing speed. The best compiler inlines layers.

The core idea is contextual thinking when composing the lexer.

When you need to parse a rather complex language, you can break it into several contexts like `NormalCode`, `String`, `FloatingNumber`, etc.

In each context, we first find out when we need to get out of it, then add other regular expressions to match :tokens.

To find out when to get into or out of a context, a look-ahead pattern is very useful.

Contexts are good for separation of concern of open-close constructs (for example, a string `"foo"` is opened with a `"`, and is closed by a `"`. We can call a `String` context at the first `"` then return at the second `"`).

# principle: lexer is just a lexer

It only handles strings. All the type converting jobs should be part of the compiler.

# principle: be tolerent to errors

Don't have to create a context for every syntax rule.

# trick: consecutive patterns

It may help building a more strict syntax checker, for example, we know that after some keywords, the :tokens may appear are very limited:

    lex Main [
      /let\b/ {
        :token "kw"
      } /\w+/ {
        :token "name"
      } /\ *(=)/ {
        :token "op.eq" 1
      }
    ]

It eagerly highlights the tokens that has already been input.

It can report expected pattern and token, helping fix the code.

Precision and flexibility, it has both.

# lexing on ambiguous syntax

In Perl/Ruby/Javascript, the `/` can be considered div operator or beginning of regexp. Only a parser is capable to handle it properly.

### For javascript:

http://stackoverflow.com/questions/5519596/when-parsing-javascript-what-determines-the-meaning-of-a-slash

There is one solution in old standard:

http://www-archive.mozilla.org/js/language/js20-2002-04/rationale/syntax.html#regular-expressions

Regardless of the previous :token, `//` is interpreted as the beginning of a comment.

It is div operator if `/` or `/=` and previous :token is noun-like:

    ]
    Identifier   Number   RegularExpression   String
    class   false   null   private   protected   public   super   this   true
    get   include   set

    ++   --   )   }   ----- See NOTE below

Else, it is regexp if previous :token is infix-like or begin-like or verb-like:

    !   !=   !==   #   %   %=   &   &&   &&=   &=   (   *   *=   +   +=   ,   -   -=   ->
    .   ..   ...   /   /=   :   ::   ;   <   <<   <<=   <=   =   ==   ===   >   >=   >>   >>=   >>>   >>>=
    ?   @   [   ^   ^=   ^^   ^^=   {   |   |=   ||   ||=   ~
    abstract   break   case   catch   const   continue   debugger   default   delete   do   else   enum
    export   extends   final   finally   for   function   goto   if   implements   import   in   instanceof
    interface   is   namespace   native   new   package  return   static   switch   synchronized
    throw   throws   transient   try   typeof   use   var   volatile   while   with

Else it is a div operator.

*pitfall 1* (don't have this problem in ruby)

The only controversial choices are `)` and `}`. A `/` after either a `)` or `}` :token can be either a division symbol (if the `)` or `}` closes a subexpression or an object literal `)` or a regular expression :token (if the `)` or `}` closes a preceding statement or an `if`, `while`, or `for` expression). Having `/` be interpreted as a RegularExpression in expressions such as `(x+y)/2` would be problematic, so it is interpreted as a division operator after `)` or `}`.

    if (true) /a/g ---> regexp
    (x+y)/2        ---> div
    {}/a/g         ---> regexp
    +{}/a/g        ---> div

for `++` and `--`, they may end an PostfixExpression or start an UnaryExpression.

*Solution*: we may make `if (...)` and `(?!+-~){}` contexts, and see if the next :token is `/`, if yes, we can set a state `$expect_regexp = true`.

    /(?<=...) \// {
      # other tokens with prev checker...
    }
    /(?<=\)|\}|\+\+|\-\-) \// {
      :token (if $expect_regexp "regexp-begin" "op-div")
      $expect_regexp = false
    }

*pitfall 2* (don't have this problem in Ruby, always recognized as Regexp in Ruby)

the code below is interpreted as div operator:

    hi = 1
    g = 1
    a = 1

    /hi/g

If one wants to place a regular expression literal at the very beginning of an expression statement, it’s best to put the regular expression in parentheses.

*Solution*: look-backward to last non-empty line.

# dealing with inline-comments and spaces

Attach them to the token before it?
