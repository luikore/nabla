## Loading code from other files

[design NOTE] not a method call, since we need to do code reloading analysis

    require 'foo'   # from load paths
    require './foo' # search from same dir

    load 'foo'           # force loading a file
    history_reload 'foo' # force loading a file and every file after it

## code reloading mechanism

Code reloading use cases:

1. re-run some source code: use `load`, but constants can not be re-defined, but it is OK to assign constant if the new value equals to the old.
2. for rapid development and see the effect after changing code

for case 2, use `history_reload`.

File is the unit for env loading organization. Each history item is a tuple of (path, absolute_path, env, constants (with versions)) or an array of such items.

For reloading to work properly, the syntax element after `require` must be a string literal (syntax restriction), and recommend only use top level `require`.

`history_reload` checks a given list (list not limited to literal), and reload all files after the earliest entry. But it has a limit: only files loaded after current unit can be reloaded.
