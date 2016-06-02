#include "sb.h"

static uint32_t _guarded_klass_find_c(const char* name, uint32_t namespace) {
  uint32_t k = klass_find_c(name, namespace);
  if (k) {
    return k;
  } else {
    klass_debug();
    static const char* template = "klass %s not found";
    int size = strlen(template) + strlen(name);
    char buf[size];
    size = sprintf(buf, template, name);
    val_throw(nb_string_new_literal(size, buf));
  }
}

#define STR(s) nb_string_new_literal_c(s)

// XXX sizeof((Val[]){__VA_ARGS__}) can explode clang's memory
#define NODE(type, argc, ...) nb_struct_new(_guarded_klass_find_c(#type, namespace), argc, (Val[]){__VA_ARGS__})

// #define TOKEN(type_name, content, val) nb_token_new_c(STR(type_name), content, val)

#define LIST(...) nb_cons_list(sizeof((Val[]){__VA_ARGS__}) / sizeof(Val), (Val[]){__VA_ARGS__})

Val sb_bootstrap_ast(uint32_t namespace) {
# include "bootstrap.inc"
}
