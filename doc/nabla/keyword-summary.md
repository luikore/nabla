~35 comparing with other languages (basic type names are excluded):

    lang  keywords
    C#     ~90
    C++    ~85
    Java   ~45
    Golang ~25
    C      ~25

## keywords summary

the following words can not be used as variables, but can be used as method names.

declarative (12)

    data class using
    include extend delegate scope # has implicit receiver
    def undef final
    var # force declaration of a new variable, useful inside `do` blocks

type checker

    as

logic shortcut operator

    not and or xor

control flow (12)

    if else case when ensure fall_through
    while wend do end for select

jumps must be inside `while`/`wend`/`do` (can we jump inside lambda?)

    next break

jump out of nearest `def` or lambda:

    return

syntax sugars

    redo

special literals

    true false nil

self/this

    self

dumb variable

    _

last same name method invocation

    super # calls last method defined in this class

require/load

    require load history_reload
