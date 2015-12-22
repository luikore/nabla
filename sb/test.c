#include <ccut.h>

void bootstrap_suite();

int main (int argc, char const *argv[]) {
  // ccut_trap_asserts();
  ccut_run_suite(bootstrap_suite);
  ccut_print_stats();
  return 0;
}
