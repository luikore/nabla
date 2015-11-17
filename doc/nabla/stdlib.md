half of the following candidates should not be in stdlib

    gmp powered precision (sad that openssl too slow) is built-in
    bignum

    ds
      set
      tree_map
      dict
      hash variants (ignore case)
      deque and channel
      graph chasing algorithm to detect ring

    crypto
      rot47
      openssl

    net
      socket
      http
      ftp
      ssh
      socks

    lang
      sh
      awk
      sed
      c
      makefile
      asm (gnu syntax)
      asm (masm syntax)
      sql
      tcl/tk
      yacc
      peg
      spellbreak - parser entries to use
      pointfree

    math
      matrix (with array programing dsl)
      math symbols operators
      number_field_compiler

    data
      json
      yaml
      html with google's parser
      pack
      bitstring
      asn.1 -> use openssl

    date

    encoding

    debug

    prof

note: i may consider `$unsigned` as a trait checker, but this kind of job can always be replaced by a method
