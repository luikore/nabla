#include "sb.h"
#include <stdlib.h>
#include <adt/utils/dbg.h>
#include <adt/dict.h>

static uint32_t _hex_to_uint(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - ('a' + 10);
  }
  return c - ('A' + 10);
}

static int32_t _scan_u8(int size, const char* signed_s) {
  static const unsigned char leading_masks[] = {0, 0b0, 0b11000000, 0b11100000, 0b11110000, 0b11111000, 0b11111100};
  static const unsigned char masks[] = {0,  0b01111111, 0b00111111, 0b00011111, 0b00001111, 0b00000111, 0b00000011};

  const unsigned char* s = (const unsigned char*)signed_s;
  if ((leading_masks[size] & s[0]) != leading_masks[size]) {
    fatal_err("invalid u8: '%.*s':%d", size, signed_s, size);
  }

  int32_t c = (int32_t)(s[0] & masks[size]);
  for (size_t i = 1; i < size; i++) {
    c = (c << 6) | (s[i] & 0b00111111);
  }
  return c;
}

static void _append_u8(char* signed_s, int64_t* pos, int32_t c) {
  unsigned char* s = (unsigned char*)signed_s;
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

#pragma mark ## actions

static Val char_escape_sp(Spellbreak* ctx, Val capture_i) {
  int i = VAL_TO_INT(capture_i);
  char c = ctx->curr[CAPTURE_BEGIN(ctx, i)];
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

static Val char_hex(Spellbreak* ctx, Val capture_i) {
  int to = CAPTURE_END(ctx, capture_i);

  int64_t r = 0;
  for (int i = 0; i < to; i++) {
    r *= 16;
    r += _hex_to_uint(ctx->curr[i]);
  }
  return VAL_FROM_INT(r);
}

static Val char_no_escape(Spellbreak* ctx, Val capture_i) {
  int i = VAL_TO_INT(capture_i);
  int size = CAPTURE_END(ctx, i) - CAPTURE_BEGIN(ctx, i);
  const char* s = ctx->curr + CAPTURE_BEGIN(ctx, i);
  int32_t c = _scan_u8(size, s);
  return VAL_FROM_INT(c);
}

static Val concat_char(Spellbreak* ctx, Val left_s, Val right_c) {
  int c = VAL_TO_INT(right_c);
  // TODO refcount and transient optimize
  const char* from = nb_string_ptr(left_s);
  size_t size = nb_string_byte_size(left_s);
  Val res_s = nb_string_new_transient(size);
  char* to = (char*)nb_string_ptr(res_s);
  memcpy(to, from, size);
  to[size] = c;
  return res_s;
}

// invoke parser defined in peg_dict, set result in context_stack
// NOTE ending current context is managed in lex vm
static Val parse(Spellbreak* sb) {
  size_t context_stack_size = ContextStack.size(&sb->context_stack);
  assert(context_stack_size);
  ContextEntry* ce = ContextStack.at(&sb->context_stack, context_stack_size);
  Val s = VAL_FROM_STR(ce->name_str);
  int token_pos = ce->token_pos;

  Val parser;
  if (!nb_dict_find(sb->peg_dict, nb_string_ptr(s), nb_string_byte_size(s), &parser)) {
    val_throw(nb_string_new_literal_c("bad context name str"));
  }
  int token_size = TokenStream.size(&sb->token_stream) - ce->token_pos;
  Token* token_start = TokenStream.at(&sb->token_stream, ce->token_pos);
  ValPair pair = sb_vm_peg_exec((void*)parser, sb->arena, token_size, token_start);
  if (pair.snd != VAL_UNDEF) {
    val_throw(pair.snd);
  }

  return pair.fst;
}

// TODO support bignum
static Val parse_int(Spellbreak* ctx, Val capture_i) {
  int group = VAL_TO_INT(capture_i);
  const char* s = ctx->curr + CAPTURE_BEGIN(ctx, group);
  char* end;
  int size = CAPTURE_END(ctx, group) - CAPTURE_BEGIN(ctx, group);
  int64_t i = strtoll(s, &end, 10);
  if ((const char*)end - s != 0) {
    // TODO just terminate the parser
    fatal_err("conversion to string failed for '%.*s', parsed size = %ld", size, s, (const char*)end - s);
  }
  return VAL_FROM_INT(i);
}

static Val style(Spellbreak* ctx, int32_t argc, Val* argv) {
  // todo
  return VAL_NIL;
}

static Val token(Spellbreak* ctx, int32_t argc, Val* argv) {
  Val name = argv[0];
  int group = ((argc > 1) ? VAL_FROM_INT(argv[1]) : 0);
  Val val = (argc == 3 ? argv[2] : VAL_UNDEF);

  Token tok = {
    .pos = ctx->curr - ctx->s,
    .size = CAPTURE_END(ctx, group) - CAPTURE_BEGIN(ctx, group),
    .v = val
  };
  TokenStream.push(&ctx->token_stream, tok);

  return VAL_NIL;
}

static Val yield(Spellbreak* ctx, Val obj) {
  
  return VAL_NIL;
}

static Val tail(Spellbreak* ctx, Val list) {
  return nb_cons_tail(list);
}

#pragma mark ## api

static uint32_t spellbreak_klass;

static void _define_node(const char* name, int argc, const char** argv) {
  NbStructField fields[argc];
  for (int i = 0; i < argc; i++) {
    fields[i] = (NbStructField){.matcher = VAL_UNDEF, .field_id = val_strlit_new_c(argv[i])};
  }
  nb_struct_def(nb_string_new_literal_c(name), spellbreak_klass, argc, fields);
}

static void _sb_destruct(void* p) {
  Spellbreak* sb = p;
  Vals.cleanup(&sb->stack);
  ContextStack.cleanup(&sb->context_stack);
  Vals.cleanup(&sb->vars);
  TokenStream.cleanup(&sb->token_stream);
}

#define METHOD(k, func, argc) klass_def_method(k, val_strlit_new_c(#func), argc, func, true)
#define METHOD2(k, func, min_argc, max_argc) klass_def_method2(k, val_strlit_new_c(#func), min_argc, max_argc, (ValMethodFunc2)func, true)
#define STR(v) nb_string_new_literal_c(v)

void sb_init_module(void) {
  spellbreak_klass = sb_new_syntax(STR("Spellbreak"));

# define DEF_NODE(type, ...) _define_node(#type, sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*), (const char*[]){__VA_ARGS__})
# include "sb-klasses.inc"
# undef DEF_NODE

  METHOD(spellbreak_klass, char_escape_sp, 1);
  METHOD(spellbreak_klass, char_hex, 1);
  METHOD(spellbreak_klass, char_no_escape, 1);
  METHOD(spellbreak_klass, concat_char, 2);
  METHOD(spellbreak_klass, parse_int, 1);
}

void sb_bootstrap() {
  void* arena = val_arena_new();
  Val ast = sb_bootstrap_ast(arena, spellbreak_klass);
  sb_syntax_compile(arena, spellbreak_klass, ast);
  val_arena_delete(arena);
}

uint32_t sb_klass() {
  return spellbreak_klass;
}

uint32_t sb_new_syntax(uint32_t name_str) {
  uint32_t klass = klass_ensure(name_str, klass_ensure(STR("Lang"), 0));
  SpellbreakMData* mdata = malloc(sizeof(SpellbreakMData));
  mdata->compiled = false;
  klass_set_data(klass, mdata);
  klass_set_destruct_func(klass, _sb_destruct);

  METHOD(klass, parse, 1);
  METHOD(klass, yield, 1);
  METHOD(klass, tail, 1);
  METHOD2(klass, style, 1, 2);
  METHOD2(klass, token, 1, 3);

  klass_set_unsafe(klass);
  return klass;
}

#undef METHOD2
#undef METHOD

Spellbreak* sb_new(uint32_t syntax_klass) {
  SpellbreakMData* mdata = klass_get_data(syntax_klass);

  if (!mdata->compiled) {
    val_throw(STR("klass not compiled"));
  }

  Spellbreak* s = val_alloc(syntax_klass, sizeof(Spellbreak));

  s->lex_dict = mdata->lex_dict;
  s->peg_dict = mdata->peg_dict;

  Vals.init(&s->stack, 10);
  ContextStack.init(&s->context_stack, 5);
  TokenStream.init(&s->token_stream, 20);
  Vals.init(&s->vars, mdata->vars_size);

  for (int i = 0; i < mdata->vars_size; i++) {
    Vals.at(&s->vars, i)[0] = VAL_NIL;
  }

  return s;
}

void sb_reset(Spellbreak* s) {
  s->stack.size = 0;
  s->context_stack.size = 0;
  s->token_stream.size = 0;
  for (int i = 0; i < Vals.size(&s->vars); i++) {
    Vals.at(&s->vars, i)[0] = VAL_NIL;
  }
  s->curr = s->s;
}

Spellbreak* sb_new_sb() {
  return sb_new(spellbreak_klass);
}

Val sb_parse(Spellbreak* s, const char* src, int64_t size) {
  s->curr = s->s = src;
  s->size = size;
  s->arena = val_arena_new();

  ValPair pair = sb_vm_lex_exec(s);
  if (pair.snd != VAL_UNDEF) {
    val_throw(pair.snd);
  }
  return pair.fst;
}

void sb_syntax_compile(void* arena, Val ast, uint32_t target_klass) {
  CompileCtx ctx = {
    .lex_dict = nb_dict_new(),
    .peg_dict = nb_dict_new(),
    .patterns_dict = nb_dict_new(),
    .vars_dict = nb_dict_new(),
    .arena = arena,
    .ast = ast
  };
  Val err = sb_compile_main(&ctx);

  if (err != VAL_UNDEF) {
    RELEASE(ctx.lex_dict);
    RELEASE(ctx.peg_dict);
    RELEASE(ctx.patterns_dict);
    RELEASE(ctx.vars_dict);
    val_throw(err);
  }

  SpellbreakMData* mdata = klass_get_data(target_klass);
  mdata->lex_dict = ctx.lex_dict;
  mdata->peg_dict = ctx.peg_dict;
  mdata->vars_size = nb_dict_size(ctx.vars_dict);
  mdata->compiled = true;
  RELEASE(ctx.patterns_dict);
  RELEASE(ctx.vars_dict);
}
