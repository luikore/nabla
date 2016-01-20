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
static uint16_t complex_reg[] = {
  SAVE, 2,
  /*2*/ CHAR, 'a', 0,
  FORK, 2, 0, 10, 0,
  /*10*/ SAVE, 3,
  SAVE, 4,
  /*14*/ CHAR, 'b', 0,
  FORK, 14, 0, 22, 0,
  /*22*/ SAVE, 5,
  MATCH
};

#define MATCH_REG(reg_ty)\
  memset(captures, 0, sizeof(captures));\
  res = sb_vm_regexp_exec(reg_ty, strlen(src), src, captures)

void vm_regexp_suite() {
  ccut_test("vm_regexp_exec simple regexp") {
    int32_t captures[20];
    const char* src;
    bool res;

    src = "ab";
    MATCH_REG(simple_reg);
    assert_eq(true, res);

    src = "";
    MATCH_REG(simple_reg);
    assert_eq(false, res);
  }

  ccut_test("vm_regexp_exec complex regexp") {
    int32_t captures[20];
    const char* src;
    bool res;

    src = "aaab";
    MATCH_REG(complex_reg);
    assert_eq(true, res);

    src = "abb";
    MATCH_REG(complex_reg);
    assert_eq(true, res);
  }
}
