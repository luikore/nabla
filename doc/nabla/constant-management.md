# Changing content of a constant

Option one: re-define methods on classes.

Option two: expose a box -- get and set are serialized so we still won't have circular references.

    Foo = Nabla.box Bar[]

    Foo.get # Bar[]
    Foo.set Baz[]
    Foo.get # Bar[]

# Resolving constant name conflict

Loader will raise an error when it meets name conflict.

We can use this:

    local import foo/bar

All constants defined during loading will be local to current file
