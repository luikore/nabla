#include <ccut.h>

void node_suite();
void vm_callback_suite();
void bootstrap_suite();

int main (int argc, char const *argv[]) {
  // ccut_trap_asserts();
  ccut_run_suite(node_suite);
  ccut_run_suite(vm_callback_suite);
  ccut_run_suite(bootstrap_suite);
  ccut_print_stats();
  return 0;
}
