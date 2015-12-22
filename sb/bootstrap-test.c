#include "sb.h"
#include <ccut.h>

static void* arena;

static void _setup() {
  val_begin_check_memory();
  arena = val_arena_new();
}

static void _teadown() {
  val_arena_delete(arena);
  val_end_check_memory();
}

void bootstrap_suite() {
  ccut_test("bootstrap ast") {
    _setup();

    Val node = sb_bootstrap_ast(arena, sb_klass());

    _teadown();
  }

  ccut_test("bootstrap compile") {
    _setup();

    sb_init_module();

    _teadown();
  }
}
