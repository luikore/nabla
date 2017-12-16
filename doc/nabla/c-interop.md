With C Interoperability we can make interfacing C programs much easier.

Floats and Integers can be auto casted from and to corresponding C types, but type mismatch / overflow will raise an exception

    obj = $|c|
      #pragma nb_export duo
      int duo(int i) {
        return i * 2;
      }
    obj.duo 3

And a `void` function returns `nil`.

A string will be casted to bytes, copied by default, and `\0` terminated at the end

    obj = $|c|
      #pragma nb_export duo
      void duo(size_t len, char* bytes) {
        // changing the content of bytes is OK, but do not realloc it.
      }
    obj.duo $ 'foo'.bytesize $ 'foo'

To not-copy the string, maybe we can design an internal function. But it is not essential.

Struct casting should be declared in C code, and returning struct will be mapped back:

    struct Foo
      bar as Integer
      baz as Integer
    end
    obj = $|c|
      #pragma nb_import Foo
      #pragma nb_export func
      struct Foo func(struct Foo foo) {
        return (struct Foo) {
          .bar = foo.bar + 1,
          .baz = foo.baz - 1
        }
      }
    obj.func foo

To take a pointer as parameter, use the `Pointer` struct:

    obj = $|c|
      #pragma nb_import Foo as Foo2
      #pragma nb_export func
      struct Foo2 func(struct Foo2* foo) {
        return (struct Foo) {
          .bar = foo->bar + 1,
          .baz = foo->baz - 1
        }
      }
    foo2 = obj.func $ Pointer[foo]

If member of a struct is another struct type, it will also be declared automatically.
But if the member is not concrete-typed, compiler reports error on struct typing.

If there is already a struct declaration in C code, and we want it back out:

    $|c|
      #pragma nb_export Foo as Foo3
      ...

If there is already a struct declaration in C but we want to map it... do it yourself!

A lambda can also be passed as a function pointer when its type is certain and can be deduced in compile time.

And it returns `struct{ result, void* }`

    F = -> [x as Integer, y as Integer] -> Integer, x * 2
    $|c|
      #pragma nb_import F
      #pragma nb_export func

      # TODO need several APIs for error handling
      extern void* nb_err(void*);

      void func(F f) {
        struct{int result, void* err} res = f(3);
        if (res.err) {
          printf("%d", res.result);
        } else {
          ...
        }
      }

A method can be passed as a function pointer, if its type is certain and can be deduced in compile time.

    class A
      final def foo[a as B, b as C] -> D
        ...
      end
    end
    a = A[] # can be deduced if A, B, C, D are all known here

If a method or lambda can not be determined in compile time, we can use a `typedef` to wrap it:

    typedef X[A, B] -> C
    X[m]

### Static typing

`typedef` and `struct` bind typing information to constants, so they can be recognized in the syntax constructs.

And compiler tries best to deduce types from:

- code instantiation, e.g. `foo = Foo[]`
- type check, e.g. `foo as Foo`
- final method calls, e.g. `foo = :bar baz` in which `:bar` is a typed final method with return type specified
