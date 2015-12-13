#include "cons.h"

typedef struct {
  ValHeader h;
  Val head;
  Val tail;
} Cons;

#define QWORDS_CONS ((sizeof(Cons) + 7) / 8)

Val nb_cons_new(Val head, Val tail) {
  Cons* node = val_alloc(KLASS_CONS, sizeof(Cons));
  node->head = head;
  node->tail = tail;
  return (Val)node;
}

Val nb_cons_anew(void* arena, Val head, Val tail) {
  Cons* node = val_arena_alloc(arena, KLASS_CONS, sizeof(Cons));
  node->head = head;
  node->tail = tail;
  return (Val)node;
}

Val nb_cons_reverse(Val list) {
  Val res = VAL_NIL;
  for (Val curr = list; curr != VAL_NIL; curr = nb_cons_tail(curr)) {
    res = nb_cons_new(nb_cons_head(curr), res);
  }
  return res;
}

Val nb_cons_arena_reverse(void* arena, Val list) {
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
