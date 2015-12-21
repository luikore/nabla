#include "sb.h"
#include <ccut.h>

static Ctx ctx;

static void _setup() {
  val_begin_check_memory();
  ctx.arena = val_arena_new(ctx.meta);
}

static void _teadown() {
  val_arena_delete(ctx.arena);
  val_end_check_memory();
}

void bootstrap_suite() {
  ccut_test("bootstrap ast") {
    _setup();

    Val node = nb_spellbreak_bootstrap(&ctx);

    _teadown();
  }

  ccut_test("bootstrap compile") {
    _setup();

    Val node = nb_spellbreak_bootstrap(&ctx);
    Spellbreak* spellbreak = sb_compile_main(ctx.meta, ctx.arena, node);

    _teadown();
  }
}
