#include "token.h"
#include "string.h"
#include <ccut.h>

void token_suite() {
  ccut_test("token new") {
    val_begin_check_memory();

    Val node = nb_token_new_c(nb_string_new_literal_c("terminal"), "foo");
    assert_str_eq("foo", nb_token_loc(node)->s);

    val_end_check_memory();
  }
}
