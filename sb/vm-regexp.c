// regexp compile and VM

#include "compile.h"
#include <adt/utils/mut-array.h>
#include <adt/utils/utf-8.h>
#include "vm-regexp-op-codes.h"
#include <ctype.h>

// Nonrecursive backtracking VM as described in https://swtch.com/~rsc/regexp/regexp2.html
// NOTE Thompson/PikeVM can't handle possessive matching / backref elegantly.
//      However we can use some simple trick to reduce pushed threads.

#pragma mark ### exec

typedef struct {
  uint16_t* pc;
  const char* s;
  int32_t captures[20];
} Thread;

MUT_ARRAY_DECL(Threads, Thread);

#define CAPTURE_CAP sizeof(int32_t) * 20

#define SWAP(x, y) do {\
    void* tmp = x;\
    x = y;\
    y = tmp;\
  } while (0)

static void _add_thread(struct Threads* threads, uint16_t* from_pc, const char* s, int32_t* from_captures) {
  Thread tmp = {.pc = from_pc, .s = s};
  memcpy(tmp.captures, from_captures, CAPTURE_CAP);

  Threads.push(threads, tmp);
}

static bool _is_word_char(int c) {
  return isalnum(c) || c == '_';
}

// prereq: s can scan back and forth
static bool _is_word_boundary(const char* s, const char* init_s, const char* end_s) {
  if (s == init_s || s == end_s) {
    return true;
  }

  int size = (int)(end_s - s);
  int next_char = utf_8_scan(s, &size);
  if (next_char < 0) {
    return false;
  }

  int back_size = (int)(s - init_s);
  int prev_char = utf_8_scan_back(s, &back_size);
  if (prev_char < 0) {
    return false;
  }

  return (_is_word_char(prev_char) && !_is_word_char(next_char)) ||
         (!_is_word_char(prev_char) && _is_word_char(next_char));
}

static bool _is_hex_char(int c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static bool _is_space_char(int c) {
  return c == '\n' || c == '\r' || c == ' ' || c == '\t';
}

static bool _is_digit_char(int c) {
  return (c >= '0' && c <= '9');
}

// NOTE
// captures[0] stores max index of captures
// captures[1] stores $0.size
static bool _exec(uint16_t* init_pc, int64_t size, const char* init_s, int32_t* captures) {
  struct Threads ts;
  Threads.init(&ts, 10);
  const char* s_end = init_s + size;

# define CHECK_END if (t->s == s_end) goto thread_dead
# define ADVANCE_CHAR do {\
    int scanned = s_end - t->s;\
    utf_8_scan(t->s, &scanned);\
    t->s += scanned;\
  } while (0)

  _add_thread(&ts, init_pc, init_s, captures);
  while (ts.size) {
    Thread* t = Threads.at(&ts, ts.size - 1);
    ts.size--;
    uint16_t* pc = t->pc;
    for (;;) {
      switch (*pc) {
        case CHAR: {
          CHECK_END;
          int c = DECODE(Arg32, pc).arg1;
          int scanned = s_end - t->s;
          int u8_char = utf_8_scan(t->s, &scanned);
          if (u8_char == c) {
            t->s += scanned;
            continue;
          } else {
            goto thread_dead;
          }
        }

        // assume set size >= 1
        case SET: {
          CHECK_END;
          int32_t size = DECODE(Arg32, pc).arg1;
          int scanned = s_end - t->s;
          int u8_char = utf_8_scan(t->s, &scanned);
          bool matched = false;
          for (int i = 0; i < size; i++) {
            if (u8_char == DECODE(int32_t, pc)) {
              matched = true;
              break;
            }
          }
          if (matched) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case MATCH: {
          memcpy(captures, t->captures, CAPTURE_CAP);
          captures[1] = (t->s - init_s);
          Threads.cleanup(&ts);
          return true;
        }

        case DIE: {
          goto thread_dead;
        }

        case JMP: {
          int offset = DECODE(Arg32, pc).arg1;
          pc = init_pc + offset;
          continue;
        }

        case JIF_RANGE: {
          CHECK_END;
          Arg323232 payload = DECODE(Arg323232, pc);
          int scanned = s_end - t->s;
          int u8_char = utf_8_scan(t->s, &scanned);
          if (u8_char >= payload.arg1 && u8_char <= payload.arg2) {
            t->s += scanned;
            pc = init_pc + payload.arg3;
          }
          continue;
        }

        case FORK: {
          Arg3232 offsets = DECODE(Arg3232, pc);
          pc = init_pc + offsets.arg1;
          _add_thread(&ts, init_pc + offsets.arg2, t->s, t->captures);
          continue;
        }

        case SAVE: {
          int16_t save_pos = DECODE(Arg16, pc).arg1;
          if (t->captures[0] < save_pos) {
            t->captures[0] = save_pos;
          }
          t->captures[save_pos] = (t->s - init_s);
          continue;
        }

        case ATOMIC: {
          int32_t offset = DECODE(Arg32, pc).arg1;
          bool matched = _exec(pc, s_end - t->s, t->s, t->captures);
          if (matched) {
            pc += offset;
            t->s += t->captures[1];
            continue;
          } else {
            goto thread_dead;
          }
        }

        case AHEAD: {
          int32_t offset = DECODE(Arg32, pc).arg1;
          bool matched = _exec(pc, s_end - t->s, t->s, t->captures);
          if (matched) {
            pc += offset;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case N_AHEAD: {
          int32_t offset = DECODE(Arg32, pc).arg1;
          bool matched = _exec(pc, s_end - t->s, t->s, t->captures);
          if (matched) {
            goto thread_dead;
          } else {
            pc += offset;
            continue;
          }
        }

        case ANCHOR_BOL: {
          if (t->s == init_s || (t->s != s_end && t->s[-1] == '\n' && t->s[0] != '\n')) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_EOL: {
          if (t->s == s_end || (t->s != init_s && t->s[-1] != '\n' && t->s[0] == '\n')) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_WBOUND: {
          if (_is_word_boundary(t->s, init_s, s_end)) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_N_WBOUND: {
          if (!_is_word_boundary(t->s, init_s, s_end)) {
            goto thread_dead;
          } else {
            continue;
          }
        }

        case ANCHOR_BOS: {
          if (t->s == init_s) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_N_BOS: {
          if (t->s == init_s) {
            goto thread_dead;
          } else {
            continue;
          }
        }

        case ANCHOR_EOS: {
          if (t->s == s_end) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_N_EOS: {
          if (t->s == s_end) {
            goto thread_dead;
          } else {
            continue;
          }
        }

        case CG_ANY: {
          CHECK_END;
          ADVANCE_CHAR;
          pc++;
          continue;
        }

        case CG_D: {
          CHECK_END;
          if (_is_digit_char(t->s[0])) {
            t->s++; // always 1 byte
            pc++;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case CG_N_D: {
          CHECK_END;
          if (_is_digit_char(t->s[0])) {
            goto thread_dead;
          } else {
            ADVANCE_CHAR;
            pc++;
            continue;
          }
        }

        case CG_W: {
          CHECK_END;
          int scanned = s_end - t->s;
          int ch = utf_8_scan(t->s, &scanned);
          if (_is_word_char(ch)) {
            t->s += scanned;
            pc++;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case CG_N_W: {
          CHECK_END;
          int scanned = s_end - t->s;
          int ch = utf_8_scan(t->s, &scanned);
          if (_is_word_char(ch)) {
            goto thread_dead;
          } else {
            t->s += scanned;
            pc++;
            continue;
          }
        }

        case CG_H: {
          CHECK_END;
          if (_is_hex_char(t->s[0])) {
            t->s++; // always 1 byte
            pc++;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case CG_N_H: {
          CHECK_END;
          if (_is_hex_char(t->s[0])) {
            goto thread_dead;
          } else {
            ADVANCE_CHAR;
            pc++;
            continue;
          }
        }

        case CG_S: {
          CHECK_END;
          if (_is_space_char(t->s[0])) {
            t->s++; // always 1 byte
            pc++;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case CG_N_S: {
          CHECK_END;
          if (_is_space_char(t->s[0])) {
            goto thread_dead;
          } else {
            ADVANCE_CHAR;
            pc++;
            continue;
          }
        }

        case END: {
          // todo error, should never reach here
        }
      }
    }
    thread_dead:;
  }

no_match:

  Threads.cleanup(&ts);
  return false;

# undef CHECK_END
# undef ADVANCE_CHAR
}

bool sb_vm_regexp_exec(uint16_t* init_pc, int64_t size, const char* str, int32_t* captures) {
  memset(captures, 0, CAPTURE_CAP);
  captures[0] = 1; // 0 & 1 are preserved

  return _exec(init_pc, size, str, captures);
}
