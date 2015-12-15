#include "cons.h"

typedef struct {
  ValHeader h;
  Val head;
  Val tail;
} Cons;

#define QWORDS_CONS ((sizeof(Cons) + 7) / 8)

void _cons_destruct(void* p) {
  Cons* cons = p;
  RELEASE(cons->head);
  RELEASE(cons->tail);
}

void nb_cons_init_module() {
  klass_def_internal(KLASS_CONS, val_strlit_new_c("Cons"));
  klass_set_destruct_func(KLASS_CONS, _cons_destruct);
  // klass_def_method(KLASS_CONS, val_strlit_new_c("=="), 1, _cons_eq, true);
  // klass_def_method(KLASS_CONS, val_strlit_new_c("hash"), 0, _cons_hash, true);
}

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

Val nb_cons_list(int32_t argc, Val* argv) {
  Val l = VAL_NIL;
  for (int i = argc - 1; i >= 0; i--) {
    l = nb_cons_new(argv[i], l);
  }
  return l;
}

Val nb_cons_alist(void* arena, int32_t argc, Val* argv) {
  Val l = VAL_NIL;
  for (int i = argc - 1; i >= 0; i--) {
    l = nb_cons_anew(arena, argv[i], l);
  }
  return l;
}
