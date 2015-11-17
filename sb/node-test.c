#include <ccut.h>
#include <adt/string.h>
#include "node.h"

static PdlexNodeMeta* meta;
static PdlexNodeArena* arena;

static void _setup() {
  meta = nb_node_meta_new();
  arena = nb_node_arena_new(meta);
}

static void _def_classes() {
  nb_node_meta_def_token(meta, ".terminal");
  nb_node_meta_def_class(meta, "Main", "<NonTerminal", 2);
}

static void _teardown() {
  nb_node_arena_delete(arena);
  nb_node_meta_delete(meta);
}

static void _assert_aligned(Val node) {
  assert_eq(false, VAL_IS_IMM((Val)node));
}

void node_suite() {
  ccut_test("syntax node new") {
    _setup();
    _def_classes();

    Val node = nb_syntax_node_new_v(arena, "Main", "NonTerminal", 2, (Val)3, (Val)5);
    assert_eq(2, nb_syntax_node_size(node));
    assert_eq(3, (int)((SyntaxNode*)node)->attrs[0]);
    assert_eq(5, (int)((SyntaxNode*)node)->attrs[1]);

    assert_eq(nb_string_new_literal_c("Main"), nb_node_context_name(node));
    assert_eq(nb_string_new_literal_c("<NonTerminal"), nb_node_type_name(node));
    assert_eq(true, nb_syntax_node_is(node, "NonTerminal"));

    _assert_aligned(node);
    node = nb_syntax_node_new_v(arena, "Main", "NonTerminal", 2, (Val)3, (Val)5);
    _assert_aligned(node);

    _teardown();
  }

  ccut_test("token node new") {
    _setup();
    _def_classes();

    Val node = nb_token_node_new_c(arena, "terminal", "foo");
    assert_str_eq("foo", ((TokenNode*)node)->loc.s);
    assert_eq(nb_string_new_literal_c(".terminal"), nb_node_type_name(node))

    _assert_aligned(node);
    node = nb_token_node_new_c(arena, "terminal", "foo");
    _assert_aligned(node);

    _teardown();
  }

  ccut_test("cons node new") {
    _setup();

    Val node;
    node = nb_cons_node_new(arena, VAL_FROM_INT(1), VAL_NIL);
    node = nb_cons_node_new(arena, VAL_FROM_INT(2), node);
    node = nb_cons_node_new(arena, VAL_FROM_INT(3), node);

    node = nb_cons_node_reverse(arena, node);
    _assert_aligned(node);

    int i = 1;
    for (Val curr = node; curr != VAL_NIL; curr = ((ConsNode*)curr)->list, i++) {
      assert_eq(VAL_FROM_INT(i), ((ConsNode*)curr)->e);
    }

    _teardown();
  }

  ccut_test("cons node reverse") {
    _setup();

    Val node = nb_cons_node_new(arena, VAL_TRUE, VAL_NIL);

    _teardown();
  }

  ccut_test("wrapper node new") {
    _setup();

    Val node = nb_wrapper_node_new(arena, VAL_TRUE);
    assert_eq(VAL_TRUE, ((WrapperNode*)node)->val);

    _teardown();
  }

  ccut_test("convert between wrapper and box node") {
    _setup();

    Val imm_obj = nb_node_to_val(nb_val_to_node(arena, VAL_FROM_INT(3)));
    assert_eq(3, VAL_TO_INT(imm_obj));

    Val heap_obj = nb_node_to_val(nb_val_to_node(arena, nb_string_new_c("foo")));
    assert_mem_eq("foo", nb_string_ptr(heap_obj), 3);
    RELEASE(heap_obj);

    _teardown();
  }
}
