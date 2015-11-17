ccut = Clean C Unit Testing

## Compiling

After cloning the code

    make install # install files in /opt/local

Then add these cflags for your tests

    -std=c11 -lccut

If you are testing C++, just

    -lccut

## Example

your_suite.c

```c
#include "ccut.h"

void your_suite() {

  ccut_test(foo1) {
    assert_true(2 == 2, "waze");
  }

  ccut_test(foo2) {
    assert_true(1 == 2, "no way");
  }

  ccut_test(bar) {
    pending;
  }

  ccut_test(baz) {
    assert_eq(1, 1);
    fail;
  }

}
```

test_runner.c

```c
#include "ccut.h"

void your_suite();
int main (int argc, char const *argv[]) {
  ccut_run_suite(your_suite);
  ccut_print_stats();
  return 0;
}
```

Add ccut.h and ccut.c, then compile and run

```sh
cc std=c11 your_suite.c test_runner.c ccut.c && ./a.out
```

![](https://raw.github.com/luikore/ccut/master/screenshot.png)

## Caveats

Require C11 for type generic macros.

Every suite is a function of state machine, tests are run in the definition order.

Code outside the `ccut_test(...){ ... }` blocks will be executed n+1 times, where n is the number of tests (`before_each`/`after_each` are todos).

To compare pointers (data that failed assertions will be printed in hex format), you must hand-cast the pointer to `void*` first.

The header `"ccut.h"` should be added lastly, because some common headers may also define a macro named `"test"`.

## Usage

- `ccut_test(test_name) { ... }` - define a test (must be put inside a void function)
- `ccut_run_suite(your_suite)` - run a test suite
- `ccut_print_stats()` - print test stats

## Assertions

- `assert_true(actual, message)` - if `actual`, then success, else show message and terminate current test
- `assert_eq(expected, actual)` - assert equal for integers or pointers
- `assert_str_eq(expected, actual)` - assert 2 nul-terminated strings are equal
- `assert_ull_eq(expected, actual)` - assert equal for unsined integer data (NOTE that `assert_eq` may overflow given two large uint64 integers)
- `assert_mem_eq(expected, actual, bytes_size)` - memory equality test
- `assert_eps_eq(expected, actual, eps)` - assert 2 numbers differ no more than the absolute value of eps

## Negative assertions

- `assert_false` - assert the expression to be considered false
- `assert_neq` - assert not equal
- `assert_str_neq` - assert 2 nul-terminated strings are not equal
- `assert_ull_neq` - assert not equal for unsined integer data
- `assert_mem_neq` - assert contents of 2 memory regions are not the same
- `assert_eps_neq` - assert 2 numbers differ more than eps

## Utilities

- `ccut_print_trace_on(signal)` print stack trace when a certain signal is generated

NOTE: you also need `-g -rdynamic` for the compiler to ensure the correct symbols.

Example assertion failure program (bad_assert.c):

```c
#include <ccut.h>
#include <assert.h>

void bad_assert_function() {
  assert(0 == 1);
}

int main(int argc, char const *argv[]) {
  ccut_print_trace_on(SIGABRT);
  bad_assert_function(NULL);
  return 0;
}
```

Compile and run:

```sh
$ cc -g -rdynamic -lccut bad_assert.c -o bad_assert
$ ./bad_assert
```

And you get the backtrace of function names -- but the pitfault of `ccut_print_trace_on` is it only contains function names and human-unreadable offsets, and the cross-platform way to find the file and lines is just hard. You can just just use a debugger by `lldb ./bad_assert` or `gdb ./bad_assert`, then `run` and `bt` for more infomation.

## Testing

The following command will test ccut on C and C++.

    make test

## License

In-file BSD 3-clause.
