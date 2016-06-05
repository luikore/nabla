**NOTE** This language is still in a design stage and working in progress

# The Nabla Programming Language

A value-immutable, functional, dynamic language.

- Minimalist and natural syntax, not only write less, but also **edit** less
- Organize code with object-oriented design concepts
- Functional with pattern matching, lens, applicative `for`, ...
- Customizable syntax and semantics, can use existing code and libraries in other languages
- Concurrency proc based on π-calculus
- With a parsing toolkit which employs visible pushdown automata, PEG, and earley parsers

# Build

Required system: 64bits.

Required build tools:

- GCC or Clang
- GNU Make
- grep
- ls

For building in windows, you can install [MSYS2](msys2.github.io) and use pacman to install the requirements.

    make

# Copying

- Nabla & CCUT: BSDL
- SipHash: vendor/siphash/README.md
- TinyCThread: vendor/tinycthread/README.txt
