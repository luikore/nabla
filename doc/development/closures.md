# Quarternity

Struct, method and lambda are syntax sugars to closure.

The closure quarternity:

- The `struct` syntax returns `self`, initializing builds data on heap
- The `def` syntax defines local hidden slots, and returns result by default calling code block. when calling, builds data on stack and invokes the code block.
- The `->` syntax also defines local hidden slots, details are initialized by syntax context, and returns result by default calling code block. when constructing, allocates in heap but code block not invoked.
  - pure lambda: no captures.
  - impure lambda: captures are "upvals", when local scope dies, put in detached mode and raise error when calling.
  - snapshotting converts impure lambda to pure lambda

But closure is not exposed to user.

Let's use a pseudo syntax to denote the closure:

    closure{k1: v1, k2: v2}
      ...
    end

Then struct is equivalent to:

    Foo = closure{k1: v1, k2: v2}
      self
    end

A method is equivalent to:

    :foo = closure{self: implicit, k1: v1, k2: v2}
      ...
    end

A lambda is equivalent to:

    k3 = nil
    closure{k1: v1, k2: v2, &k3: k3, &self: self}
      ...
    end

Those references (`&` variables) are upvals, which looks upward.

In storage, a binding of method closure would look like:

    struct Binding {
      # klass field points to metadata object (a klass which inherits Method)
      # and it has proc code defined (no redundant proc pointer in instance, we can optimize it in JIT)
      #
      # NOTE: not "proto" field, because we have more structured metadata
      #
      ValHeader header;
      Val self;
      Val k1;
      Val k2;
      Val local1;
      Val local2;
      ...
    };

A lambda closure (instance) looks nearly the same but allocated on heap:

    struct Lambda {
      # every lambda has its own klass which inherits Lambda
      # a flag will indicate whether this is pure or not.
      # if pure, the interpreter sets the pure flag and interpret `load_up & store_up` as `load & store`
      ValHeader header;

      # self of the enclosing method
      Val self;

      # points to upval, bytecode `load_up & store_up` will use the pointer
      Val local_upval1;
      ...

      Val k1;
      Val k2;
      Val local1;
      Val local2;
    };

A struct closure looks like:

    struct Struct {
      # klass doesn't have a proc
      ValHeader header;
      Val k1;
      Val k2;
    };

All 3 representation can use the same argument constraint mechanism, and error will map to definition position.

A class definition for any of the above:

    struct Klass {
      ValHeader header;
      Val name;
      bool visible; # TODO for lambda & method, the class is not visible? consider if this makes sense or not
      Array<FieldSpec> field_specs;
      Hash<String, int> field_names;
      byte* initializer; # code for parsing input
      byte* proc;        # code for executing output
    };

Runtime organization of classes:

    Map<String, Klass*> classes;
    [(file, [String])] loaded_classes; # so we can bulk unload a list of klasses

# Behavior class

Behavior classes can be mutated in runtime

    foo = def foo[] do
    end

# The initializer language

Consider a nesting initializer:

    struct Foo{
      x
      y
      *z as Bar
    }

Pattern matching struct will use the klass spec. But pattern matching a lambda has to execute the code.
