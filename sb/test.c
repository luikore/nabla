#include <ccut.h>

void bootstrap_suite();
void vm_regexp_suite();

int main (int argc, char const *argv[]) {
  // ccut_trap_asserts();
  ccut_run_suite(bootstrap_suite);
  ccut_run_suite(vm_regexp_suite);
  ccut_print_stats();
  return 0;
}
