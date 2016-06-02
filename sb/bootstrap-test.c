#include "sb.h"
#include <ccut.h>

void bootstrap_suite() {
  ccut_test("bootstrap ast") {
    int32_t gen = val_gens_new_gen();
    val_gens_set_current(gen);

    sb_init_module();
    Val node = sb_bootstrap_ast(sb_klass());

    val_gens_set_current(0);
    val_gens_drop();
  }
}
