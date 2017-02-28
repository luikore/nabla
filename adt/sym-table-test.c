#include "sym-table.h"
#include <ccut.h>

void sym_table_suite() {
  ccut_test("id incremental") {
    val_begin_check_memory();
    NbSymTable* table = nb_sym_table_new();
    uint64_t i1;
    nb_sym_table_get_set(table, strlen("hello"), "hello", &i1);
    uint64_t i2;
    nb_sym_table_get_set(table, strlen("world"), "world", &i2);
    assert_eq(1, i2 - i1);

    nb_sym_table_delete(table);
    val_end_check_memory();
  }

  ccut_test("ids are identical for equivalent strings") {
    val_begin_check_memory();
    NbSymTable* table = nb_sym_table_new();
    uint64_t i1;
    nb_sym_table_get_set(table, strlen("hello"), "hello", &i1);
    nb_sym_table_get_set(table, strlen("world"), "world", NULL);
    char hello2[] = "hello";
    uint64_t i2;
    nb_sym_table_get_set(table, strlen(hello2), hello2, &i2);
    assert_eq(i1, i2);
    assert_eq(2, nb_sym_table_size(table));

    nb_sym_table_delete(table);
    val_end_check_memory();
  }

  ccut_test("reverse get") {
    val_begin_check_memory();
    NbSymTable* table = nb_sym_table_new();
    uint64_t i;
    nb_sym_table_get_set(table, 4, "char", &i);

    size_t sz;
    char* ptr;
    assert_true(nb_sym_table_reverse_get(table, &sz, &ptr, i), "should contain the key");
    assert_eq(4, sz);
    assert_mem_eq("char", ptr, 4);

    nb_sym_table_delete(table);
    val_end_check_memory();
  }
}
