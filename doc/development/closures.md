# Quarternity

Struct, method, pure lambda and impure lambda are syntax sugars to closure.

The closure quarternity:

- The `struct` syntax returns `self`, initializing builds data on heap
- The `def` syntax defines local hidden slots, and returns result by default calling code block. when calling, builds data on stack and invokes the code block.
- The `->` syntax also defines local hidden slots, details are initialized by syntax context, and returns result by default calling code block. when constructing, allocates in heap but code block not invoked.
  - pure lambda: no captures.
  - impure lambda: captures are "upvals", when local scope dies, put in detached mode and raise error when calling.
    snapshotting converts impure lambda to pure lambda

But closure is not exposed to user.

Let's use a pseudo syntax to denote the closure:

    closure{k1: v1, k2: v2}
      ...
    end

Then a `struct` is something like:

    Foo = closure{k1: v1, k2: v2}
      self
    end

A method is considered with a true form of:

    :methods@'foo' = closure{self: implicit, k1: v1, k2: v2}
      ...
    end

A lambda is written like (with another pseudo syntax: `&` variables are upvals, which looks upward):

    k3 = nil
    closure{k1: v1, k2: v2, &k3: k3, &self: self}
      ...
    end

In storage, a binding of method closure's C representation:

    struct Binding {
      // klass field points to metadata object (a klass which inherits Method)
      // and it has proc code defined (no redundant proc pointer in instance, we can optimize it in JIT)
      //
      // NOTE: not "proto" field, because we have more structured metadata
      //
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
      // every lambda has its own klass which inherits Lambda
      // a flag will indicate whether this is pure or not.
      // if pure, the interpreter sets the pure flag and interpret `load_up & store_up` as `load & store`
      ValHeader header;

      // self of the enclosing method
      Val self;

      // points to upval, bytecode `load_up & store_up` will use the pointer
      Val local_upval1;
      ...

      Val k1;
      Val k2;
      Val local1;
      Val local2;
    };

An instance of struct looks like:

    struct Struct {
      // klass doesn't have a proc
      ValHeader header;
      Val k1;
      Val k2;
    };

All 3 representation can use the same argument constraint mechanism, and error will map to definition position.

A struct definition for any of the above:

    struct Struct {
      ValHeader header;  // klass = klass_struct
      Val name;
      bool visible;      // TODO for lambda & method, the class is not visible? consider if this makes sense or not
      [FieldSpec] field_specs;
      {String: int} field_names;
      byte* initializer; // a special bytecode for parsing input
      byte* proc;        // code for executing output
      Val (*native_proc)(Val); // native implementation of the proc
      Klass* klass;
    };

Runtime organization of structs:

    {String: Struct*} structs; // we also put native types as structs in it
    [(file, [String])] loaded_structs; // so we can bulk unload a list of struct

# Behavior class

Behavior classes can be mutated in runtime,

    struct Klass {
      ValHeader header;
      {String: Struct*} final_methods;
      {String: Struct*} methods;
      [Class*] includes;
    };

Runtime organization of classes:

    {String: [file, Klass*]} klasses; // note: multiple klass definitions in one file are merged into one klass
    [(file, [String])] loaded_classes;

TODO: it can introduce loop references since a klass is mutable, but the lifecycle of classes are completely determined by loading and unloading.

When a struct is defined or a class is defined, the runtime will lookup the table of structs and classes, and do the connection if needed.

Native types do not have structs, but they are connected to classes.

# Lifecycle of struct

first create klass id with a name.

naming:

- struct: `Foo::Bar::Baz`
- method: `Foo::Bar:method`
- lambda: `Foo::Bar:method\3` the 3rd lambda inside that method. when a method is replaced, all the lambdas inside should be checked for undef.

then create struct with field specs

then set struct proc

# Bootstraping

The spellbreaker klass

# The initializer language

Consider a nesting initializer:

    struct Foo{
      x
      y
      *z as Bar
    }

Pattern matching struct will use the klass spec. But pattern matching a lambda has to execute the code.
