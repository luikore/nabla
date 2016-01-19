#include <ccut.h>
#include "sb.h"

// TODO opcode parser
enum OpCodes {
  // op    // args                 // description
  CHAR,    // c:int32_t            // match a char
  MATCH,   //                      // found a match
  JMP,     // offset:int32_t       // unconditional jump
  FORK,    // x:int32_t, y:int32_t // fork execution
  SAVE,    // i:int16_t            // save current position to captures[i]
  AHEAD,   // offset:int32_t       // invoke lookahead code starting from offset
  N_AHEAD, // offset:int32_t       // invoke negative lookahead code starting from offset
  END,     //                      // terminate opcode
  OP_CODES_SIZE
};

// ab
static uint16_t simple_reg[] = {
  CHAR, 'a', 0, // little endian
  CHAR, 'b', 0,
  MATCH
};

// (a+)(b+)

void vm_regexp_suite() {
  ccut_test("vm_regexp_exec") {
    int32_t captures[20];
    const char* src = "ab";
    bool res = sb_vm_regexp_exec(simple_reg, strlen(src), src, captures);
    assert_eq(true, res);
  }
}
