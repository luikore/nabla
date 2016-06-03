#include <ccut.h>
#include <adt/val.h>

void bootstrap_suite();
void vm_regexp_suite();
void vm_peg_suite();
void vm_lex_suite();

int main (int argc, char const *argv[]) {
  val_trap_backtrace(argv[0]);
  ccut_run_suite(bootstrap_suite);
  ccut_run_suite(vm_regexp_suite);
  ccut_run_suite(vm_peg_suite);
  ccut_run_suite(vm_lex_suite);
  ccut_print_stats();
  return 0;
}
