#include "spellbreak.h"
#include <ccut.h>

static Ctx ctx;

static void _setup() {
  val_begin_check_memory();
  ctx.meta = nb_node_meta_new();
  ctx.arena = nb_node_arena_new(ctx.meta);
}

static void _teadown() {
  nb_node_arena_delete(ctx.arena);
  nb_node_meta_delete(ctx.meta);
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
    Spellbreak* spellbreak = nb_spellbreak_compile_main(ctx.meta, ctx.arena, node);
    nb_spellbreak_delete(spellbreak);

    _teadown();
  }
}
