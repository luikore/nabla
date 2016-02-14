enum OpCodes {
  // op    // args                 // description
  CHAR,    // c:int32_t            // match a char
  RANGE,   // f:int32_t, t:int32_t // match a char range
  MATCH,   //                      // found a match
  JMP,     // offset:int32_t       // unconditional jump
  FORK,    // x:int32_t, y:int32_t // fork execution
  SAVE,    // i:int16_t            // save current position to captures[i]
  ATOMIC,  // offset:int32_t       // match atomic group, can also be used for possesive matching
  AHEAD,   // offset:int32_t       // invoke following lookahead code, if matched, goto offset
  N_AHEAD, // offset:int32_t       // invoke following negative lookahead code, if not matched, goto offset
  ANCHOR_BOL,
  ANCHOR_EOL,
  ANCHOR_WBOUND,
  ANCHOR_N_WBOUND,
  ANCHOR_BOS,
  ANCHOR_N_BOS,
  ANCHOR_EOS,
  ANCHOR_N_EOS,
  END,     //                      // terminate opcode
  OP_CODES_SIZE
};

static const char* op_code_names[] = {
  [CHAR] = "char",
  [RANGE] = "range",
  [MATCH] = "match",
  [JMP] = "jmp",
  [FORK] = "fork",
  [SAVE] = "save",
  [ATOMIC] = "atomic",
  [AHEAD] = "ahead",
  [N_AHEAD] = "n_ahead",
  [ANCHOR_BOL] = "anchor_bol",
  [ANCHOR_EOL] = "anchor_eol",
  [ANCHOR_WBOUND] = "anchor_wbound",
  [ANCHOR_N_WBOUND] = "anchor_n_wbound",
  [ANCHOR_BOS] = "anchor_bos",
  [ANCHOR_N_BOS] = "anchor_n_bos",
  [ANCHOR_EOS] = "anchor_eos",
  [ANCHOR_N_EOS] = "anchor_n_eos",
  [END] = "end"
};
