#include "cons.h"

typedef struct {
  ValHeader h;
  Val head;
  Val tail;
} Cons;

#define QWORDS_CONS ((sizeof(Cons) + 7) / 8)

bool _cons_eq(Val lhs, Val rhs) {
  if (lhs == VAL_NIL && rhs == VAL_NIL) {
    return true;
  }
  if (VAL_KLASS(lhs) != KLASS_CONS || VAL_KLASS(rhs) != KLASS_CONS) {
    return false;
  }
  return val_eq(nb_cons_head(lhs), nb_cons_head(rhs)) &&
    _cons_eq(nb_cons_tail(lhs), nb_cons_tail(rhs));
}

uint64_t _cons_hash(Val obj) {
  if (obj == VAL_NIL) {
    return KLASS_CONS_SALT;
  }
  uint64_t hash = val_hash(nb_cons_head(obj));
  return hash ^ _cons_hash(nb_cons_tail(obj));
}

void _cons_destruct(void* p) {
  Cons* cons = p;
  RELEASE(cons->head);
  RELEASE(cons->tail);
}

void nb_cons_init_module() {
  klass_def_internal(KLASS_CONS, val_strlit_new_c("Cons"));
  klass_set_destruct_func(KLASS_CONS, _cons_destruct);
  klass_set_hash_func(KLASS_CONS, _cons_hash);
  klass_set_eq_func(KLASS_CONS, _cons_eq);
}

Val nb_cons_new(Val head, Val tail) {
  Cons* node = val_alloc(KLASS_CONS, sizeof(Cons));
  node->head = head;
  node->tail = tail;
  return (Val)node;
}

Val nb_cons_new_rev(Val init, Val last) {
  if (init == VAL_NIL) {
    return nb_cons_new(last, VAL_NIL);
  }
  Val head = nb_cons_head(init);
  Val tail = nb_cons_tail(init);
  return nb_cons_new(head, tail);
}

Val nb_cons_anew(void* arena, Val head, Val tail) {
  Cons* node = val_arena_alloc(arena, KLASS_CONS, QWORDS_CONS);
  node->head = head;
  node->tail = tail;
  return (Val)node;
}

Val nb_cons_anew_rev(void* arena, Val init, Val last) {
  if (init == VAL_NIL) {
    return nb_cons_anew(arena, last, VAL_NIL);
  }
  Val head = nb_cons_head(init);
  Val tail = nb_cons_tail(init);
  tail = nb_cons_anew_rev(arena, tail, last);
  return nb_cons_anew(arena, head, tail);
}

Val nb_cons_reverse(Val list) {
  Val res = VAL_NIL;
  for (Val curr = list; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    res = nb_cons_new(nb_cons_head(curr), res);
  }
  return res;
}

Val nb_cons_areverse(void* arena, Val list) {
  Val res = VAL_NIL;
  for (Val curr = list; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    res = nb_cons_anew(arena, nb_cons_head(curr), res);
  }
  return res;
}

Val nb_cons_head(Val vnode) {
  assert(VAL_KLASS(vnode) == KLASS_CONS);
  Cons* node = (Cons*)vnode;
  return node->head;
}

Val nb_cons_tail(Val vnode) {
  assert(VAL_KLASS(vnode) == KLASS_CONS);
  Cons* node = (Cons*)vnode;
  return node->tail;
}

Val nb_cons_list(int32_t argc, Val* argv) {
  Val l = VAL_NIL;
  for (int i = 0; i < argc; i++) {
    l = nb_cons_new(argv[i], l);
  }
  return l;
}

Val nb_cons_alist(void* arena, int32_t argc, Val* argv) {
  Val l = VAL_NIL;
  for (int i = 0; i < argc; i++) {
    l = nb_cons_anew(arena, argv[i], l);
  }
  return l;
}
