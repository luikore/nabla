#include "token.h"
#include "string.h"
#include <ccut.h>

void token_suite() {
  ccut_test("token new") {
    val_begin_check_memory();

    Val node = nb_token_new_c(nb_string_new_literal_c("terminal"), "foo", VAL_NIL);
    assert_str_eq("foo", nb_token_loc(node)->s);

    Val token_content = nb_token_to_s(node);
    Val foo = nb_string_new_c("foo");
    assert_true(val_eq(foo, token_content), "content should be foo");

    val_end_check_memory();
  }
}
