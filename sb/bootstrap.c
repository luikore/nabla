#include "sb.h"

#define STR(s) nb_string_new_literal_c(s)

// XXX sizeof((Val[]){__VA_ARGS__}) can explode clang's memory
#define NODE(type, argc, ...) nb_struct_new(klass_find(STR(#type), namespace), argc, (Val[]){__VA_ARGS__})

// #define TOKEN(type_name, content, val) nb_token_new_c(STR(type_name), content, val)

#define LIST(...) nb_cons_list(sizeof((Val[]){__VA_ARGS__}) / sizeof(Val), (Val[]){__VA_ARGS__})

Val sb_bootstrap_ast(void* arena, uint32_t namespace) {
# include "bootstrap.inc"
}
