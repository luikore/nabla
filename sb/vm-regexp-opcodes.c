enum OpCodes {
  // op    // args                 // description
  CHAR,    // c:int32_t            // match a char
  RANGE,   // f:int32_t, t:int32_t // match a char range
  MATCH,   //                      // found a match
  JMP,     // offset:int32_t       // unconditional jump
  FORK,    // x:int32_t, y:int32_t // fork execution
  SAVE,    // i:int16_t            // save current position to captures[i]
  ATOMIC,  // offset:int32_t       // match atomic group, can also be used for possesive matching
  AHEAD,   // offset:int32_t       // invoke lookahead code starting from offset
  N_AHEAD, // offset:int32_t       // invoke negative lookahead code starting from offset
  END,     //                      // terminate opcode
  OP_CODES_SIZE
};
