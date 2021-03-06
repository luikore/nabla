#include "token.h"
#include "utils/str.h"
#include "string.h"

typedef struct {
  ValHeader h;
  Val name;
  NbTokenLoc loc;
} Token;

#define QWORDS_TOKEN ((sizeof(Token) + 7) / 8)

static bool _token_eq(Val l, Val r) {
  if (VAL_KLASS(r) != KLASS_TOKEN) {
    return false;
  }
  Token* tl = (Token*)l;
  Token* tr = (Token*)r;
  return val_eq(tl->name, tr->name) &&
         val_eq(tl->loc.v, tr->loc.v) &&
         str_compare(tl->loc.size, tl->loc.s, tr->loc.size, tr->loc.s) == 0;
}

static uint64_t _token_hash(Val vt) {
  Token* t = (Token*)vt;
  uint64_t h = val_hash(t->name) ^ KLASS_TOKEN_SALT;
  if (t->loc.size) {
    h ^= val_hash_mem(t->loc.s, t->loc.size);
  }
  h ^= val_hash(t->loc.v);
  return h;
}

static void _token_destruct(void* ptr) {
  Token* t = ptr;
  RELEASE(t->loc.v);
}

void nb_token_init_module() {
  klass_def_internal(KLASS_TOKEN, val_strlit_new_c("Token"));
  klass_set_destruct_func(KLASS_TOKEN, _token_destruct);
  klass_set_eq_func(KLASS_TOKEN, _token_eq);
  klass_set_hash_func(KLASS_TOKEN, _token_hash);
}

Val nb_token_new(Val name, NbTokenLoc loc) {
  Token* t = val_alloc(KLASS_TOKEN, sizeof(Token));
  t->loc = loc;
  return (Val)t;
}

Val nb_token_new_c(Val name, const char* content, Val v) {
  NbTokenLoc loc = {.s = content, .size = strlen(content), .v = v};
  return nb_token_new(name, loc);
}

NbTokenLoc* nb_token_loc(Val tok) {
  assert(VAL_KLASS(tok) == KLASS_TOKEN);
  return &((Token*)tok)->loc;
}

Val nb_token_to_s(Val v) {
  assert(VAL_KLASS(v) == KLASS_TOKEN);
  Token* tok = (Token*)v;
  return nb_string_new(tok->loc.size, tok->loc.s);
}
