#include <ccut.h>

void bootstrap_suite();
void vm_regexp_suite();
void vm_peg_suite();
void vm_lex_suite();

int main (int argc, char const *argv[]) {
  // ccut_trap_asserts();
  ccut_run_suite(bootstrap_suite);
  ccut_run_suite(vm_regexp_suite);
  ccut_run_suite(vm_peg_suite);
  ccut_run_suite(vm_lex_suite);
  ccut_print_stats();
  return 0;
}
