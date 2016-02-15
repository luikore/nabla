// NOTE: to reduce runtime state size,
//       ignore case / language / encoding flags are all set in compile time
enum OpCodes {
  // op        // args                 // description
  CHAR,        // c:int32_t            // match a char
  CHAR2,       // a:int32_t, b:int32_t // matches either a or b, can optimize ignore case matchs
  MATCH,       //                      // found a match
  DIE,         //                      // no match
  JMP,         // offset:int32_t       // unconditional jump
  JIF_RANGE,   // f:int32_t, t:int32_t, offset:int32 // jump to offset if matches f..t, continue if not match
  FORK,        // x:int32_t, y:int32_t // fork execution
  SAVE,        // i:int16_t            // save current position to captures[i]
  ATOMIC,      // offset:int32_t       // match atomic group, can also be used for possesive matching
  AHEAD,       // offset:int32_t       // invoke following lookahead code, if matched, goto offset
  N_AHEAD,     // offset:int32_t       // invoke following negative lookahead code, if not matched, goto offset
  ANCHOR_BOL,                          //
  ANCHOR_EOL,                          //
  ANCHOR_WBOUND,                       //
  ANCHOR_N_WBOUND,                     //
  ANCHOR_BOS,                          //
  ANCHOR_N_BOS,                        //
  ANCHOR_EOS,                          //
  ANCHOR_N_EOS,                        //
  CG_ANY,                              // char group '.' (NOTE predefined char groups don't need to respect language)
  CG_D,                                // char group '\d'
  CG_N_D,                              // char group '\D'
  CG_W,                                // char group '\w'
  CG_N_W,                              // char group '\W'
  CG_H,                                // char group '\h', hex digit, always ignore case
  CG_N_H,                              // char group '\H', non hex digit
  CG_S,                                // char group '\s'
  CG_N_S,                              // char group '\S'
  END,         //                      // terminate opcode
  OP_CODES_SIZE
};

static const char* op_code_names[] = {
  [CHAR] = "char",
  [CHAR2] = "char2",
  [MATCH] = "match",
  [DIE] = "die",
  [JMP] = "jmp",
  [JIF_RANGE] = "jif_range",
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
  [CG_ANY] = "cg_any",
  [CG_D] = "cg_d",
  [CG_N_D] = "cg_n_d",
  [CG_W] = "cg_w",
  [CG_N_W] = "cg_n_w",
  [CG_H] = "cg_h",
  [CG_N_H] = "cg_n_h",
  [CG_S] = "cg_s",
  [CG_N_S] = "cg_n_s",
  [END] = "end"
};
