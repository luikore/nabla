#include "sb.h"
#include <ccut.h>

void bootstrap_suite() {
  ccut_test("bootstrap ast") {
    val_begin_check_memory();

    sb_init_module();
    void* arena = val_arena_new();
    Val node = sb_bootstrap_ast(arena, sb_klass());
    val_arena_delete(arena);

    val_end_check_memory();
  }
}
