# Struct, method and lambda are syntax sugars to closure

The closure trinity:

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

In storage, a method closure would look like:

    struct Method {
      Val self;
      Val k1;
      Val k2;
      Code proc;
    };
    struct MethodMeta {
      Hash metadata;
      Hash defaults;
    };

A lambda closure looks exactly the same:

    struct Lambda {
      Val self;
      Val k1;
      Val k2;
      Code proc;
    };

But in proc code, there is upval bytecode which uses `self` for real variable get/set.
