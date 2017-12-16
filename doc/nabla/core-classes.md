# Object Introspection

Methods on objects

    obj.class
    obj.hash_code
    obj.==
    obj.match?    # default impl: alias of ==
    obj.inspect
    obj.respond_to?

    a.present?
    a.presence
    a.blank?

    obj.methods
    obj.object_id
    obj.:m[*args]

    # following methods make new objects with new methods
    obj.def! m lambda
    obj.undef! m
    obj.extend! module

## Kernel methods

Some basic shortcuts that can be called with `:` invocation

    Kernel.gets
    Kernel.puts
    Kernel.sleep
    Kernel.delegate
    Kernel.exit
    Kernel.rand

And top level defined methods are added to `Kernel`.

## Runtime introspection

    Nabla.require
    Nabla.load
    Nabla.reload
    Nabla.infix

    # todo: prof, debug, tracing... will be libraries but attached under Nabla

    Nabla.languages

## Process

Also can have signal handlers

    Process.daemon
    Process.at_exit
    Process.exit
    Process.pid
    Process.kill
    Process.trap

## IO

    IO.readline
    IO.putc
    IO.puts
    IO.print
    IO.open
    ...

## File system and path

    File.cd
    File.cp
    File.entries
    File.glob
    File.mktmp
    File.mktmp_dir
    File.dirname
    File.basename
    File.absolute_path
    File.join

## Numeric

    Integer
    Float
    Rational
    Complex

## Others

    Regexp
    Range
