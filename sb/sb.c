#include "spellbreak.h"
#include <stdlib.h>
#include <adt/utils/dbg.h>
#include <adt/dict.h>
#include <adt/box.h>

static uint32_t _hex_to_uint(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - ('a' + 10);
  }
  return c - ('A' + 10);
}

static void _append_u8(char* s, int64_t* pos, int32_t c) {
# define MASK_C(rshift) (((c >> rshift) & 0b00111111) | 0b10000000)
  if (c < 0x80) {
    s[*pos++] = c;
  } else if (c < 0x0800) {
    s[*pos++] = (0b11000000 | (c >> 6));
    s[*pos++] = MASK_C(0);
  } else if (c < 0x10000) {
    s[*pos++] = (0b11100000 | (c >> 12));
    s[*pos++] = MASK_C(6);
    s[*pos++] = MASK_C(0);
  } else if (c < 0x200000) {
    s[*pos++] = (0b11110000 | (c >> 18));
    s[*pos++] = MASK_C(12);
    s[*pos++] = MASK_C(6);
    s[*pos++] = MASK_C(0);
  } else if (c < 0x4000000) {
    s[*pos++] = (0b11111000 | (c >> 24));
    s[*pos++] = MASK_C(18);
    s[*pos++] = MASK_C(12);
    s[*pos++] = MASK_C(6);
    s[*pos++] = MASK_C(0);
  } else {
    s[*pos++] = (0b11111100 | (c >> 30));
    s[*pos++] = MASK_C(24);
    s[*pos++] = MASK_C(18);
    s[*pos++] = MASK_C(12);
    s[*pos++] = MASK_C(6);
    s[*pos++] = MASK_C(0);
  }
# undef MASK_C
}

static Val _char_hex(Ctx* ctx, Val v_tok, Val v_from, Val v_to) {
  TokenNode* tok = (TokenNode*)v_tok;
  const char* s = tok->loc.s;
  int64_t from = VAL_TO_INT(v_from);
  int64_t to = VAL_TO_INT(v_to);
  if (to < 0) {
    to = tok->loc.size + 1 + to;
  }
  assert(to > from);

  int64_t r = 0;
  for (int i = from; i < to; i++) {
    r *= 16;
    r += _hex_to_uint(s[i]);
  }
  return VAL_FROM_INT(r);
}

static Val _char_escape_sp(Ctx* ctx, Val tok, Val v_index) {
  int i = VAL_TO_INT(v_index);
  char c = ((TokenNode*)tok)->loc.s[i];
  switch (c) {
    case 'a': return VAL_FROM_INT('\a');
    case 'b': return VAL_FROM_INT('\b');
    case 'f': return VAL_FROM_INT('\f');
    case 'n': return VAL_FROM_INT('\n');
    case 'r':
    default:
      return VAL_FROM_INT('\r');
  }
}

#pragma mark ## actions

static Val char_escape_sp_1(Ctx* ctx, Val tok) {
  return _char_escape_sp(ctx, tok, VAL_FROM_INT(0));
}

static Val char_hex_1(Ctx* ctx, Val tok) {
  return _char_hex(ctx, tok, VAL_FROM_INT(0), VAL_FROM_INT(-1));
}

// TODO utf-8 char
static Val char_no_escape_1(Ctx* ctx, Val v_tok) {
  TokenNode* tok = (TokenNode*)v_tok;
  return VAL_FROM_INT(tok->loc.s[0]);
}

static Val compile_spellbreak_1(Ctx* ctx, Val tree) {
  return (Val)nb_spellbreak_compile_main(ctx->meta, ctx->arena, tree);
}

static Val concat_char_2(Ctx* ctx, Val left_s, Val right_c) {
  int c = VAL_TO_INT(right_c);
  // TODO refcount and transient optimize
  const char* from = nb_string_ptr(left_s);
  size_t size = nb_string_bytesize(left_s);
  Val res_s = nb_string_new_transient(size);
  char* to = (char*)nb_string_ptr(res_s);
  memcpy(to, from, size);
  to[size] = c;
  return res_s;
}

static Val cons_2(Ctx* ctx, Val e, Val list) {
  return nb_cons_node_new(ctx->arena, e, list);
}

// pop current tokens and invoke parser
static Val parse_0(Ctx* ctx) {
  // todo
  return VAL_NIL;
}

static Val parse_int_1(Ctx* ctx, Val v_tok) {
  TokenNode* tok = (TokenNode*)v_tok;
  const char* s = tok->loc.s;
  char* end;
  int size = tok->loc.size;
  int64_t i = strtoll(s, &end, 10);
  if ((const char*)end - s != 0) {
    // TODO just terminate the parser
    fatal_err("conversion to string failed for '%.*s', parsed size = %ld", size, s, (const char*)end - s);
  }
  return VAL_FROM_INT(i);
}

static Val return_1(Ctx* ctx, Val v_obj) {
  // todo
  return VAL_NIL;
}

static Val style_2(Ctx* ctx, Val tok, Val v) {
  return VAL_NIL;
}

static Val token_2(Ctx* ctx, Val v_pseudo_tok, Val v_capture) {
  // todo
  return VAL_NIL;
}

static Val token_1(Ctx* ctx, Val v_pseudo_tok) {
  return token_2(ctx, v_pseudo_tok, VAL_FROM_INT(0));
}

static Val yield_1(Ctx* ctx, Val obj) {
  return VAL_NIL;
}

static Val tail_1(Ctx* ctx, Val list) {
  return VAL_NIL;
}

#pragma mark ## api

Spellbreak* nb_spellbreak_new(void) {
  static Val actions = VAL_NIL;
  if (actions == VAL_NIL) {
    actions = nb_dict_new();
#   define AC(name, func) REPLACE(actions, nb_dict_insert(actions, name, strlen(name), nb_box_new((uint64_t)func)))
    AC("char_escape_sp/1", char_escape_sp);
    AC("char_hex/1", char_hex_1);
    AC("char_no_escape/1", char_no_escape_1);
    AC("compile_spellbreak/1", compile_spellbreak_1);
    AC("concat_char/2", concat_char_2);
    AC("cons/2", cons_2);
    AC("parse/0", parse_0);
    AC("parse_int/1", parse_int_1);
    AC("return/1", return_1);
    AC("style/2", style_2);
    AC("token/2", token_2);
    AC("token/1", token_1);
    AC("yield/1", yield_1);
    AC("tail/1", tail_1);
#   undef AC
    val_perm(actions);
  }

  Spellbreak* spellbreak = malloc(sizeof(Spellbreak));
  spellbreak->patterns_dict = nb_dict_new();
  spellbreak->actions_dict = actions;
  spellbreak->vars_dict = nb_dict_new();
  spellbreak->lex_dict = nb_dict_new();
  spellbreak->peg_dict = nb_dict_new();
  return spellbreak;
}

void* nb_spellbreak_find_action(Spellbreak* sb, char* name, int32_t name_size, int8_t arity) {
  char key[name_size + arity + 5];
  int key_size = sprintf(key, "%.*s/%u", name_size, name, arity);
  if (key_size < 0) {
    fatal_err("find action concat string failed");
  }
  Val func;
  if (nb_dict_find(sb->actions_dict, key, key_size, &func)) {
    return (void*)nb_box_get(func);
  } else {
    fatal_err("action not defined: %.*s", key_size, key);
  }
}

void nb_spellbreak_delete(Spellbreak* spellbreak) {
  RELEASE(spellbreak->patterns_dict);
  RELEASE(spellbreak->actions_dict);
  RELEASE(spellbreak->vars_dict);
  RELEASE(spellbreak->lex_dict);
  RELEASE(spellbreak->peg_dict);
  free(spellbreak);
}
