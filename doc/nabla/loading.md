## Require: specifying code dependencies

    require foo/bar     # require code from another file, under load path, from first to last
    require ./foo/bar   # search starts in folder containing current source
    require /foo/bar    # search starts in root folder
    require c://foo/bar # search starts in drive C
    require ~/foo/bar   # search starts in home folder

If there is space or comma in the path, quote it

    require "foo bar, baz/good"
    require "foo \x23 bar"

The load path is specified at startup and can not be changed over time.

So every call is easily determined.

It also accepts glob patterns

    require ./test-*

`require` doesn't work inside methods, and parameters of `require`

[design NOTE] `require` is not a method call to better address static dependencies and works for analysis.

## Dynamic loading

To support dynamic loading:

    loaded_objects = [
      Nabla.load 'foo/bar'
      Nabla.load './foo/bar'
      Nabla.load 'foo/bar' {paths: ['baz', 'xip']}
    ]

Dynamic loading returns a load marker, it contains:

- All the dependencies of it (via `require`), if we issue a reload again, the files will be re-loaded by topological sorting order.
- All the constants and methods it overwrites, if load this file again, we need to delete the constants, and restore the methods first.

To issue a reload:

    Nabla.reload loaded_objects

Problems of dynamic reloading:

- Inlined or cached final methods / constants: bytecode has to remember the dependency (or the hierarchical version) and deoptimize if needed.
- Objects of an older struct: runtime must not delete existing fields in a loaded struct, new fields should be appended to the exitsing struct class.

## Reloading

Only code loaded by `Nabla.require` and `Nabla.load` can be reloaded.

## Resolving constant name conflict: require under namespace

Loader will raise an error when it meets name conflict.

We can use this:

    class Foo
      require foo/bar
    end

Then all constants and all dependencies in the module will be encapsulated inside namespace `Foo`.

## Code reloading mechanism

Code reloading use cases:

1. re-run some source code: use `load`, but constants can not be re-defined, but it is OK to assign constant if the new value equals to the old.
2. for rapid development and see the effect after changing code

for case 2, use `history_reload`.

File is the unit for env loading organization. Each history item is a tuple of (path, absolute_path, env, constants (with versions)) or an array of such items.

For reloading to work properly, the syntax element after `require` must be a string literal (syntax restriction), and recommend only use top level `require`.

`history_reload` checks a given list (list not limited to literal), and reload all files after the earliest entry. But it has a limit: only files loaded after current unit can be reloaded.
