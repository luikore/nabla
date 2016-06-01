#include "gens.h"
#include <ccut.h>

void gens_suite() {
  ccut_test("gen 0: heap") {
    Gens* g = nb_gens_new_gens();

    void* ptr = nb_gens_malloc(g, 10);
    assert_neq(NULL, ptr);
    ptr = nb_gens_realloc(g, ptr, 10, 20);
    nb_gens_free(g, ptr);

    nb_gens_delete_gens(g);
  }

  ccut_test("gen -1: checked") {
    Gens* g = nb_gens_new_gens();
    nb_gens_set_current(g, -1);

    void* ptr = nb_gens_malloc(g, 10);
    assert_neq(NULL, ptr);
    ptr = nb_gens_realloc(g, ptr, 10, 20);
    nb_gens_free(g, ptr);

    nb_gens_delete_gens(g);
  }

  ccut_test("gen >0: arena") {
    Gens* g = nb_gens_new_gens();

    // todo

    nb_gens_delete_gens(g);
  }
}
