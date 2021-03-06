- function names in lower snake case: `foo_bar`
- type names start with capital, in camel case: `FooBar`
- macros in upper snake case: `FOO_BAR`
- Exposed functions should be mostly prefixed with `val_`, `nb_`, or `nb_`
- `*_alloc` and `*_free` functions don't do internal constructor and destructor
- `*_new` and `*_delete` functions invoke internal constructor and destructor
- file names separated with dash (`-`)
- use int instead of `size_t` and assert > 0 (why not use `size_t` and `-Wconversion`?, because val integer is represented in signed int, this way can result in less conversions)
- API interfaces should use fixed length `int32_t`, `int64_t`, ... instead of platform-individual `int`

# malloc and free

A: It is totally OK to `free(NULL)`, so the recommended way (from http://stackoverflow.com/a/27451303)

    char *p = malloc(BUFSIZ);
    char *q = malloc(BUFSIZ);
    char *r = malloc(BUFSIZ);
    if (p && q && r) {
      // ...
    }
    free(p);
    free(q);
    free(r);

B: fail malloc failure directly

    mallopt(M_CHECK_ACTION, 1); // abort program
