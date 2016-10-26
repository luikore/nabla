#include "sb.h"
#include <stdlib.h>
#include <adt/utils/dbg.h>
#include <adt/utils/utf-8.h>
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

#pragma mark ## actions

static ValPair char_escape_sp(Spellbreak* ctx, Val capture_i) {
  int i = VAL_TO_INT(capture_i);
  char c = ctx->curr[CAPTURE_BEGIN(ctx, i)];
  switch (c) {
    case 'a': return (ValPair){VAL_FROM_INT('\a'), VAL_NIL};
    case 'b': return (ValPair){VAL_FROM_INT('\b'), VAL_NIL};
    case 'f': return (ValPair){VAL_FROM_INT('\f'), VAL_NIL};
    case 'n': return (ValPair){VAL_FROM_INT('\n'), VAL_NIL};
    case 'r': return (ValPair){VAL_FROM_INT('\r'), VAL_NIL};
    default:
      return (ValPair){VAL_NIL, nb_string_new_literal_c("bad escape char")};
  }
}

static ValPair char_hex(Spellbreak* ctx, Val capture_i) {
  int to = CAPTURE_END(ctx, capture_i);

  int64_t r = 0;
  for (int i = 0; i < to; i++) {
    r *= 16;
    r += _hex_to_uint(ctx->curr[i]);
  }
  return (ValPair){VAL_FROM_INT(r), VAL_NIL};
}

static ValPair char_no_escape(Spellbreak* ctx, Val capture_i) {
  int i = VAL_TO_INT(capture_i);
  int size = CAPTURE_END(ctx, i) - CAPTURE_BEGIN(ctx, i);
  const char* s = ctx->curr + CAPTURE_BEGIN(ctx, i);
  int scanned_size = size;
  int32_t c = utf_8_scan(s, &scanned_size);
  if (c < 0) {
    return (ValPair){VAL_NIL, nb_string_new_f("invalid u8: '%.*s':%d", size, s, size)};
  } else {
    return (ValPair){VAL_FROM_INT(c), VAL_NIL};
  }
}

static ValPair concat_char(Spellbreak* ctx, Val left_s, Val right_c) {
  int c = VAL_TO_INT(right_c);
  // TODO refcount and transient optimize
  const char* from = nb_string_ptr(left_s);
  size_t size = nb_string_byte_size(left_s);
  Val res_s = nb_string_new_transient(size);
  char* to = (char*)nb_string_ptr(res_s);
  memcpy(to, from, size);
  to[size] = c;
  return (ValPair){res_s, VAL_NIL};
}

// invoke peg parser defined in peg_dict, set result in context_stack
// NOTE ending current context is managed in lex vm
// TODO when we support importing external rules, we need to find the right action
static ValPair peg(Spellbreak* sb, Val s) {
  size_t context_stack_size = ContextStack.size(&sb->context_stack);
  assert(context_stack_size);
  ContextEntry* ce = ContextStack.at(&sb->context_stack, context_stack_size);
  int token_pos = ce->token_pos;

  Val parser;
  if (!nb_dict_find(sb->context_dict, nb_string_ptr(s), nb_string_byte_size(s), &parser)) {
    return (ValPair){VAL_NIL, nb_string_new_literal_c("bad context name str")};
  }
  int token_size = TokenStream.size(&sb->token_stream) - ce->token_pos;
  Token* token_start = TokenStream.at(&sb->token_stream, ce->token_pos);
  ValPair pair = sb_vm_peg_exec((void*)parser, token_size, token_start);
  return pair;
}

// TODO support bignum
static ValPair parse_int(Spellbreak* ctx, Val capture_i) {
  int group = VAL_TO_INT(capture_i);
  const char* s = ctx->curr + CAPTURE_BEGIN(ctx, group);
  char* end;
  int size = CAPTURE_END(ctx, group) - CAPTURE_BEGIN(ctx, group);
  int64_t i = strtoll(s, &end, 10);
  if ((const char*)end - s != 0) {
    Val err = nb_string_new_f("conversion to string failed for '%.*s', parsed size = %ld", size, s, (const char*)end - s);
    return (ValPair){VAL_NIL, err};
  }
  return (ValPair){VAL_FROM_INT(i), VAL_NIL};
}

static ValPair style(Spellbreak* ctx, int32_t argc, Val* argv) {
  // todo
  return (ValPair){VAL_NIL, VAL_NIL};
}

static ValPair token(Spellbreak* ctx, int32_t argc, Val* argv) {
  Val name = argv[0];
  int group = ((argc > 1) ? VAL_FROM_INT(argv[1]) : 0);
  Val val = (argc == 3 ? argv[2] : VAL_UNDEF);

  Token tok = {
    .pos = ctx->curr - ctx->s,
    .size = CAPTURE_END(ctx, group) - CAPTURE_BEGIN(ctx, group),
    .v = val,
    .ty = VAL_TO_STR(name)
  };
  TokenStream.push(&ctx->token_stream, tok);

  return (ValPair){VAL_NIL, VAL_NIL};
}

static ValPair yield(Spellbreak* ctx, Val obj) {
  // todo
  return (ValPair){VAL_NIL, VAL_NIL};
}

static ValPair tail(Spellbreak* ctx, Val list) {
  return (ValPair){nb_cons_tail(list), VAL_NIL};
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
  TokenStream.cleanup(&sb->token_stream);
}

#define METHOD(k, func, argc) klass_def_method(k, val_strlit_new_c(#func), argc, func, true)
#define METHOD2(k, func, min_argc, max_argc) klass_def_method_v(k, val_strlit_new_c(#func), min_argc, max_argc, (ValMethodFuncV)func, true)
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
  int32_t gen = val_gens_new_gen();
  val_gens_set_current(gen);

  Val ast = sb_bootstrap_ast(spellbreak_klass);
  sb_syntax_compile(spellbreak_klass, ast);

  val_gens_set_current(gen - 1);
  val_gens_drop();
}

uint32_t sb_klass() {
  return spellbreak_klass;
}

uint32_t sb_new_syntax(uint32_t name_str) {
  uint32_t klass = klass_def(name_str, klass_def(STR("Lang"), 0));
  SpellbreakMData* mdata = malloc(sizeof(SpellbreakMData));
  mdata->compiled = false;
  klass_set_data(klass, mdata);
  klass_set_destruct_func(klass, _sb_destruct);

  METHOD(klass, peg, 1);
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

  s->context_dict = mdata->context_dict;

  Vals.init(&s->stack, mdata->vars_size + 10);
  ContextStack.init(&s->context_stack, 5);
  TokenStream.init(&s->token_stream, 20);

  for (int i = 0; i < mdata->vars_size; i++) {
    Vals.at(&s->stack, i)[0] = VAL_NIL;
  }

  return s;
}

void sb_reset(Spellbreak* s) {
  s->stack.size = 0;
  s->context_stack.size = 0;
  s->token_stream.size = 0;
  for (int i = 0; i < Vals.size(&s->stack); i++) {
    Vals.at(&s->stack, i)[0] = VAL_NIL;
  }
  s->curr = s->s;
}

Spellbreak* sb_new_sb() {
  return sb_new(spellbreak_klass);
}

Val sb_parse(Spellbreak* s, const char* src, int64_t size) {
  s->curr = s->s = src;
  s->size = size;

  ValPair pair = sb_vm_lex_exec(s);
  if (pair.snd != VAL_UNDEF) {
    val_throw(pair.snd);
  }
  return pair.fst;
}

void sb_syntax_compile(Val ast, uint32_t target_klass) {
  CompileCtx ctx = {
    .context_dict = nb_dict_new(),
    .patterns_dict = nb_dict_new(),
    .vars_table = nb_sym_table_new(),
    .ast = ast
  };
  Iseq.init(&ctx.iseq, 30);
  Val err = sb_compile_main(&ctx);

  if (err != VAL_UNDEF) {
    RELEASE(ctx.context_dict);
    RELEASE(ctx.patterns_dict);
    nb_sym_table_delete(ctx.vars_table);
    val_throw(err);
  }

  SpellbreakMData* mdata = klass_get_data(target_klass);
  mdata->context_dict = ctx.context_dict;
  mdata->vars_size = nb_sym_table_size(ctx.vars_table)
  mdata->compiled = true;
  RELEASE(ctx.patterns_dict);
  nb_sym_table_delete(ctx.vars_table); // TODO need to keep it for debugging
}
