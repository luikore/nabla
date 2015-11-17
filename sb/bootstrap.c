#include "spellbreak.h"

#define NODE(context, type, size, ...) ({\
  nb_node_meta_def_class(ctx->meta, #context, "<"#type, size);\
  nb_syntax_node_new_v(ctx->arena, #context, #type, size, __VA_ARGS__);\
})

#define TOKEN(type_name, content) ({\
  nb_node_meta_def_token(ctx->meta, "."type_name);\
  nb_token_node_new_c(ctx->arena, type_name, content);\
})

#define CONS(e, list) nb_cons_node_new(ctx->arena, e, list)

#define STR(s) nb_string_new_literal_c(s)

Val nb_spellbreak_bootstrap(Ctx* ctx) {
# include "bootstrap.inc"
}
