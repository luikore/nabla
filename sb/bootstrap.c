#include "sb.h"

#define STR(s) nb_string_new_literal_c(s)

static uint32_t FIND_K(const char* klass, uint32_t parent) {
  uint32_t k = klass_find(STR(klass), parent);
  if (k) {
    return k;
  } else {
    klass_debug();
    static const char* template = "klass %s not found";
    int size = strlen(template) + strlen(klass);
    char buf[size];
    size = sprintf(buf, template, klass);
    val_throw(nb_string_new_literal(size, buf));
  }
}

// XXX sizeof((Val[]){__VA_ARGS__}) can explode clang's memory
#define NODE(type, argc, ...) nb_struct_anew(arena, FIND_K(#type, namespace), argc, (Val[]){__VA_ARGS__})

// #define TOKEN(type_name, content, val) nb_token_new_c(STR(type_name), content, val)

#define LIST(...) nb_cons_alist(arena, sizeof((Val[]){__VA_ARGS__}) / sizeof(Val), (Val[]){__VA_ARGS__})

Val sb_bootstrap_ast(void* arena, uint32_t namespace) {
# include "bootstrap.inc"
}
