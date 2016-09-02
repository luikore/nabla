# Object Introspection

Methods on objects

    obj.class
    obj.hash_code
    obj.==
    obj.match?    # default impl: alias of ==
    obj.inspect
    obj.respond_to?
    obj.call m args

    a.present?
    a.presence
    a.blank?

    obj.methods
    obj.object_id
    obj.method m

    # following methods make new objects with new methods
    obj.def! m lambda
    obj.undef! m
    obj.extend! module

methods can be undefined with `undef` or replaced (just def one with the same name again)

to finalize methods to forbid re-def (raise error on re-def):

    final def foo
      ...

to forbid include or re-open of a class

    final class Foo
      ...

if a child class re-def the method, it will raise an error

## Runtime introspection

(NOTE) Kernel extends itself and methods on it can be called with `:`, so introspection methods need to be defined under other components.

    Kernel::Syntax
    Kernel::Runtime (prof, debug, tracing...)
