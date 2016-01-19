// regexp compile and VM

#include "compile.h"
#include <adt/utils/mut-array.h>

// PikeVM as described in https://swtch.com/~rsc/regexp/regexp2.html
enum OpCodes {
  // op    // args                 // description
  CHAR,    // c:int32_t            // match a char
  MATCH,   //                      // found a match
  JMP,     // offset:int32_t       // unconditional jump
  FORK,    // x:int32_t, y:int32_t // fork execution
  SAVE,    // i:int16_t            // save current position to captures[i]
  AHEAD,   // offset:int32_t       // invoke lookahead code starting from offset
  N_AHEAD, // offset:int32_t       // invoke negative lookahead code starting from offset
  END,     //                      // terminate opcode
  OP_CODES_SIZE
};

typedef struct {
  uint16_t op;
  int32_t arg1;
} __attribute__((packed)) Arg32;

typedef struct {
  uint16_t op;
  int32_t arg1, arg2;
} __attribute__((packed)) Arg3232;

typedef struct {
  uint16_t op;
  int16_t arg1;
} __attribute__((packed)) Arg16;

#define DECODE(ty, pc) ({ty res = *((ty*)pc); pc = (uint16_t*)((ty*)pc + 1); res;})

typedef struct {
  uint16_t* pc;
  int32_t saved[20];
} Thread;

MUT_ARRAY_DECL(Threads, Thread);

Val sb_vm_regexp_compile(struct Iseq* iseq, void* arena, Val patterns_dict, Val node) {
  return VAL_NIL;
}

#define CAPTURE_CAP sizeof(int32_t) * 20

#define SWAP(x, y) do {\
    void* tmp = x;\
    x = y;\
    y = tmp;\
  } while (0)

static void _add_thread(struct Threads* threads, uint16_t* from_pc, int32_t* from_saved) {
  // TODO we may hash mask to reduce memcmp?
  Thread tmp = {.pc = from_pc};
  memcpy(tmp.saved, from_saved, CAPTURE_CAP);
  for (int k = 0; k < Threads.size(threads); k++) {
    if (memcmp(Threads.at(threads, k), &tmp, sizeof(Thread)) == 0) {
      return;
    }
  }
  Threads.push(threads, tmp);
}

// NOTE captures[0] stores the size of current capture
bool sb_vm_regexp_exec(uint16_t* pc, int64_t size, const char* str, int32_t* captures) {
  struct Threads ts[2];
  struct Threads* ct = ts;
  struct Threads* nt = ts + 1;
  Threads.init(ct, 10);
  Threads.init(nt, 10);

  _add_thread(ct, pc, captures);
  for (int j = 0; j <= size; j++) {
    for (int i = 0; i < Threads.size(ct); i++) {
      Thread* t = Threads.at(ct, i);
      switch (*t->pc) {
        case CHAR: {
          if (j == size) {
            break;
          }
          int c = DECODE(Arg32, t->pc).arg1;
          if (str[j] != c) {
            break;
          }
          _add_thread(nt, t->pc, t->saved);
          break;
        }

        case MATCH: {
          memcpy(captures, t->saved, CAPTURE_CAP);
          captures[1] = j;
          Threads.cleanup(ct);
          Threads.cleanup(nt);
          return true;
        }

        case JMP: {
          int offset = DECODE(Arg32, t->pc).arg1;
          _add_thread(ct, t->pc + offset, t->saved);
          break;
        }

        case FORK: {
          Arg3232 offsets = DECODE(Arg3232, t->pc);
          _add_thread(ct, t->pc + offsets.arg1, t->saved);
          _add_thread(ct, t->pc + offsets.arg2, t->saved);
          break;
        }

        case SAVE: {
          int16_t save_pos = DECODE(Arg16, t->pc).arg1;
          if (t->saved[0] < save_pos) {
            t->saved[0] = save_pos;
          }
          t->saved[save_pos] = j;
          _add_thread(ct, t->pc, t->saved);
          break;
        }

        case AHEAD: {
          int32_t offset = DECODE(Arg32, t->pc).arg1;
          bool matched = sb_vm_regexp_exec(t->pc + offset, size - j, str + j, t->saved);
          if (matched) {
            _add_thread(ct, t->pc, t->saved);
          }
        }

        case N_AHEAD: {
          int32_t offset = DECODE(Arg32, t->pc).arg1;
          bool matched = sb_vm_regexp_exec(t->pc + offset, size - j, str + j, t->saved);
          if (!matched) {
            _add_thread(ct, t->pc, t->saved);
          }
        }

        case END: {
          // todo error, should never reach here
        }
      }
    }
    SWAP(ct, nt);
    nt->size = 0;
  }

  Threads.cleanup(ct);
  Threads.cleanup(nt);
  return false;
}

Val sb_vm_regexp_from_string(struct Iseq* iseq, void* arena, Val s) {
  return VAL_NIL;
}
