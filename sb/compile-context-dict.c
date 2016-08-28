#include "compile.h"

void sb_compile_context_dict_insert(CompileCtx* ctx, Val name, char kind, int32_t offset) {
  char stack_buf[20];
  char* buf;
  int size = nb_string_byte_size(name);
  if (size + 1 >= 20) {
    buf = malloc(size + 2);
  } else {
    buf = stack_buf;
  }

  buf[0] = kind;
  memcpy(buf + 1, nb_string_ptr(name), size);

  REPLACE(ctx->context_dict, nb_dict_insert(ctx->context_dict, buf, size + 1, VAL_FROM_INT(offset)));

  if (size + 1 >= 20) {
    free(buf);
  }
}

int32_t sb_compile_context_dict_find(Val context_dict, Val name, char kind) {
  char stack_buf[20];
  char* buf;
  int size = nb_string_byte_size(name);
  if (size + 1 >= 20) {
    buf = malloc(size + 2);
  } else {
    buf = stack_buf;
  }

  buf[0] = kind;
  memcpy(buf + 1, nb_string_ptr(name), size);

  Val res;
  int offset;
  bool found = nb_dict_find(context_dict, buf, size + 1, &res);
  if (found) {
    offset = VAL_TO_INT(res);
  } else {
    offset = -1;
  }

  if (size + 1 >= 20) {
    free(buf);
  }
  return offset;
}
