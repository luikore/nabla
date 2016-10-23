## Dynamic loading

[design NOTE] `import` is not a method call to better address static dependencies and works for analysis.

To support dynamic loading:

    loaded_objects = [
      Nabla.load 'foo/bar'
      Nabla.load './foo/bar'
      Nabla.load 'foo/bar' {paths: ['baz', 'xip']}
    ]

Dynamic loading returns a load marker, it contains:

- All the dependencies of it (via `import`), if we issue a reload again, the files will be re-loaded by topological sorting order.
- All the constants and methods it overwrites, if load this file again, we need to delete the constants, and restore the methods first.

To issue a reload:

    Nabla.reload loaded_objects

Problems of dynamic reloading:

- Inlined or cached final methods / constants: bytecode has to remember the dependency (or the hierarchical version) and deoptimize if needed.
- Objects of an older struct: runtime must not delete existing fields in a loaded struct, new fields should be appended to the exitsing struct class.

## Production-ready hot code replacement

It is even harder than development-mode dynamic loading, we can use other mechanisms to switch deployment so not consider it now.

The main problems:

- There may be 2 versions of code for an eternal running proc, and it is hard to decide when to replace it.
- Some proc is started by a module, tracking them is even harder.
