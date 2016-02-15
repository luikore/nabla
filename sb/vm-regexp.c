// regexp compile and VM

#include "compile.h"
#include <adt/utils/mut-array.h>
#include <adt/utils/utf-8.h>
#include "vm-regexp-opcodes.c"
#include <ctype.h>

// Nonrecursive backtracking VM as described in https://swtch.com/~rsc/regexp/regexp2.html
// NOTE Thompson/PikeVM can't handle possessive matching / backref elegantly.
//      However we can use some simple trick to reduce pushed threads.

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
  int32_t arg1, arg2, arg3;
} __attribute__((packed)) Arg323232;

typedef struct {
  uint16_t op;
  int16_t arg1;
} __attribute__((packed)) Arg16;

#define DECODE(ty, pc) ({ty res = *((ty*)pc); pc = (uint16_t*)((ty*)pc + 1); res;})

#define ENCODE(iseq, ty, data) do {\
  uint16_t args[sizeof(ty) / sizeof(uint16_t)];\
  ((ty*)args)[0] = data;\
  for (int _i = 0; _i < (sizeof(ty) / sizeof(uint16_t)); _i++) {\
    Iseq.push(iseq, args[_i]);\
  }\
} while (0)

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
  // TODO use \p{Word}
  return isalnum(c) || c == '_';
}

static bool _is_hex_char(int c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static bool _is_space_char(int c) {
  return c == '\n' || c == '\r' || c == ' ' || c == '\t';
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

        case CHAR2: {
          // TODO
          goto thread_dead;
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

        // TODO scan unicode char
        case ANCHOR_WBOUND: {
          if (t->s == init_s || t->s == s_end ||
              (_is_word_char(t->s[-1]) && !_is_word_char(t->s[0])) ||
              (!_is_word_char(t->s[-1]) && _is_word_char(t->s[0]))) {
            continue;
          } else {
            goto thread_dead;
          }
        }

        case ANCHOR_N_WBOUND: {
          if (t->s == init_s || t->s == s_end ||
              (_is_word_char(t->s[-1]) && !_is_word_char(t->s[0])) ||
              (!_is_word_char(t->s[-1]) && _is_word_char(t->s[0]))) {
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
          if (t->s[0] >= '0' && t->s[0] <= '9') {
            t->s++; // always 1 byte
            pc++;
            continue;
          } else {
            goto thread_dead;
          }
        }

        case CG_N_D: {
          CHECK_END;
          if (t->s[0] >= '0' && t->s[0] <= '9') {
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

#pragma mark ### compile

Val sb_vm_regexp_from_string(struct Iseq* iseq, Val s) {
  if (VAL_KLASS(s) != KLASS_STRING) {
    return nb_string_new_literal_c("not string");
  }

  const char* ptr = nb_string_ptr(s);
  int size = nb_string_byte_size(s);

  for (int i = 0; i < size; i++) {
    ENCODE(iseq, Arg32, ((Arg32){CHAR, ptr[i]}));
  }
  ENCODE(iseq, uint16_t, MATCH);
  ENCODE(iseq, uint16_t, END);

  return VAL_NIL;
}

MUT_ARRAY_DECL(Stack, Val);
MUT_ARRAY_DECL(Ints, int);

static Val MATCH_NODE=0, LABEL_NODE=0, JMP_NODE=0, FORK_NODE=0, ATOMIC_NODE=0;

static void _ensure_tags() {
  if (!MATCH_NODE) {
    void* ptr  = malloc(sizeof(Val) * 5);
    MATCH_NODE = (Val)ptr;
    LABEL_NODE = (Val)(ptr + 1);
    JMP_NODE   = (Val)(ptr + 2);
    FORK_NODE  = (Val)(ptr + 3);
    ATOMIC_NODE = (Val)(ptr + 4);
  }
}

static int _new_label(struct Ints* labels) {
  int i = Ints.size(labels);
  Ints.push(labels, 0);
  return i;
}

static void _push_label(struct Stack* stack, Val label) {
  Stack.push(stack, label);
  Stack.push(stack, LABEL_NODE);
}

static void _push_atomic(struct Stack* stack, Val label) {
  Stack.push(stack, label);
  Stack.push(stack, ATOMIC_NODE);
}

static void _push_fork(struct Stack* stack, Val label1, Val label2) {
  Stack.push(stack, label2);
  Stack.push(stack, label1);
  Stack.push(stack, FORK_NODE);
}

static void _push_jmp(struct Stack* stack, Val label) {
  Stack.push(stack, label);
  Stack.push(stack, JMP_NODE);
}

static void _push_seq(struct Stack* stack, Val seq) {
  Val content = nb_struct_get(seq, 0);
  for (Val tail = content; tail != VAL_NIL; tail = nb_cons_tail(tail)) {
    Stack.push(stack, nb_cons_head(tail));
  }
}

static void _push_branches(struct Stack* stack, struct Ints* labels, Val branches) {
  /* example encoding e1 | e2 | e3

    fork L1 L2
    L1: e1
    jmp L0
    L2:

    fork L3 L4
    L3: e2
    jmp L0
    L4:

    e3
    L0:

  */

  Val label0 = _new_label(labels);
  _push_label(stack, label0);
  Val tail = branches;
  if (tail != VAL_NIL) {
    Stack.push(stack, nb_cons_head(tail));
    tail = nb_cons_tail(tail);
  }

  for (; tail != VAL_NIL; tail = nb_cons_tail(tail)) {

    Val label1 = _new_label(labels);
    Val label2 = _new_label(labels);

    _push_label(stack, label2);
    _push_jmp(stack, label0);
    Stack.push(stack, nb_cons_head(tail));
    _push_label(stack, label1);

    _push_fork(stack, label1, label2);
  }
}

static void _push_quantified(struct Stack* stack, struct Ints* labels, Val node, void* arena) {
  Val e = nb_struct_get(node, 0);
  Val quantifier = nb_struct_get(node, 1);

  const char* ptr = nb_string_ptr(quantifier);
  int len = nb_string_byte_size(quantifier);

  if (len == 1) { // greedy
    switch (ptr[0]) {
      case '?': {
        // fork L1 L2
        // L1: e
        // L2:
        Val l1 = _new_label(labels);
        Val l2 = _new_label(labels);
        _push_label(stack, l2);
        Stack.push(stack, e);
        _push_label(stack, l1);
        _push_fork(stack, l1, l2);
        break;
      }
      case '+': {
        // L1: e
        // fork L1 L2
        // L2
        Val l1 = _new_label(labels);
        Val l2 = _new_label(labels);
        _push_label(stack, l2);
        _push_fork(stack, l1, l2);
        Stack.push(stack, e);
        _push_label(stack, l1);
        break;
      }
      case '*': {
        // L1: fork L2 L3
        // L2: e
        // jmp L1
        // L3:
        Val l1 = _new_label(labels);
        Val l2 = _new_label(labels);
        Val l3 = _new_label(labels);
        _push_label(stack, l3);
        _push_jmp(stack, l1);
        Stack.push(stack, e);
        _push_label(stack, l2);
        _push_fork(stack, l2, l3);
        _push_label(stack, l1);
        break;
      }
    }
  } else {
    switch (ptr[1]) {
      case '?': { // reluctant
        switch (ptr[0]) {
          case '?': {
            // fork L2 L1
            // L1: e
            // L2:
            Val l1 = _new_label(labels);
            Val l2 = _new_label(labels);
            _push_label(stack, l2);
            Stack.push(stack, e);
            _push_label(stack, l1);
            _push_fork(stack, l2, l1);
            break;
          }
          case '+': {
            // L1: e
            // fork L2 L1
            // L2:
            Val l1 = _new_label(labels);
            Val l2 = _new_label(labels);
            _push_label(stack, l2);
            _push_fork(stack, l2, l1);
            Stack.push(stack, e);
            _push_label(stack, l1);
            break;
          }
          case '*': {
            // L1: fork L3 L2
            // L2: e
            // jmp L1
            // L3:
            Val l1 = _new_label(labels);
            Val l2 = _new_label(labels);
            Val l3 = _new_label(labels);
            _push_label(stack, l3);
            _push_jmp(stack, l1);
            Stack.push(stack, e);
            _push_label(stack, l2);
            _push_fork(stack, l3, l2);
            _push_label(stack, l1);
            break;
          }
        }
        break;
      }
      case '+': { // possessive
        Val l = _new_label(labels);
        _push_label(stack, l);
        Val args[] = {e, nb_string_new_literal(1, ptr)};
        Val node = nb_struct_anew(arena, VAL_KLASS(node), 2, args);
        Stack.push(stack, node);
        _push_atomic(stack, l);
        break;
      }
    }
  }
}

// good: returns 0
// negative range: returns 1
// from > to: returns 2
// range overflow: returns 3
static int _push_quantified_range(struct Stack* stack, struct Ints* labels, Val node) {
  // node[e, from, to?, reluctant/possessive mark]
  Val e = nb_struct_get(node, 0);
  Val v_from = nb_struct_get(node, 1);
  Val v_to_maybe = nb_struct_get(node, 2);
  Val kind = nb_struct_get(node, 3);

  if (kind != VAL_NIL) {
    fatal_err("reluctant/possessive mark not supported yet");
  }

  int64_t from, to;
  from = VAL_TO_INT(v_from);
  if (from < 0) {
    return 1;
  }
  if (from > (1 << 30)) { // 1G, too large
    return 3;
  }
  if (v_to_maybe == VAL_NIL) {
    to = from;
  } else {
    to = VAL_TO_INT(nb_cons_head(to));
    if (from > to) {
      return 2;
    }
  }

  // example: e{2,5} -> e e e? e? e?
  for (int64_t i = 0; i < from; i++) {
    Stack.push(stack, e);
  }
  for (int64_t i = from; i < to; i++) {
    // fork L1 L2
    // L1: e
    // L2:
    Val l1 = _new_label(labels);
    Val l2 = _new_label(labels);
    _push_label(stack, l2);
    Stack.push(stack, e);
    _push_label(stack, l1);
    _push_fork(stack, l1, l2);
  }
  return 0;
}

static void _encode_char(struct Iseq* iseq, Val char_node, bool ignore_case) {
  int chr = VAL_TO_INT(char_node);
  // TODO ignore case
  ENCODE(iseq, Arg32, ((Arg32){CHAR, chr}));
}

static void _encode_anchor(struct Iseq* iseq, Val anchor) {
  const char* s = nb_string_ptr(anchor);
  switch (s[0]) {
    case '^': {
      ENCODE(iseq, uint16_t, ANCHOR_BOL);
      break;
    }
    case '$': {
      ENCODE(iseq, uint16_t, ANCHOR_EOL);
      break;
    }
    case '\\': {
      switch (s[1]) {
        case 'b': {
          ENCODE(iseq, uint16_t, ANCHOR_WBOUND);
          break;
        }
        case 'B': {
          ENCODE(iseq, uint16_t, ANCHOR_N_WBOUND);
          break;
        }
        case 'a': {
          ENCODE(iseq, uint16_t, ANCHOR_BOS);
          break;
        }
        case 'A': {
          ENCODE(iseq, uint16_t, ANCHOR_N_BOS);
          break;
        }
        case 'z': {
          ENCODE(iseq, uint16_t, ANCHOR_EOS);
          break;
        }
        case 'Z': {
          ENCODE(iseq, uint16_t, ANCHOR_N_EOS);
          break;
        }
      }
      break;
    }
  }
}

static void _encode_predef_char_group(struct Iseq* iseq, Val node) {
  const char* ptr = nb_string_ptr(nb_struct_get(node, 0));
  if (ptr[0] == '.') {
    ENCODE(iseq, uint16_t, CG_ANY);
    return;
  }

  switch (ptr[1]) {
    case 'd': {
      ENCODE(iseq, uint16_t, CG_D);
      return;
    }
    case 'D': {
      ENCODE(iseq, uint16_t, CG_N_D);
      return;
    }
    case 'w': {
      ENCODE(iseq, uint16_t, CG_W);
      return;
    }
    case 'W': {
      ENCODE(iseq, uint16_t, CG_N_W);
      return;
    }
    case 'h': {
      ENCODE(iseq, uint16_t, CG_H);
      return;
    }
    case 'H': {
      ENCODE(iseq, uint16_t, CG_N_H);
      return;
    }
    case 's': {
      ENCODE(iseq, uint16_t, CG_S);
      return;
    }
    case 'S': {
      ENCODE(iseq, uint16_t, CG_N_S);
      return;
    }
  }
}

static Val _flatten_char_group(Val node, void* arena, bool ignore_case) {
  // BracketCharGroup[beginer, (CharRange | BracketCharGroup)+]
  uint32_t kCharGroup = VAL_KLASS(node);
  uint32_t kCharRange = klass_find_c("CharRange", sb_klass());
}

static void _encode_char_group(struct Iseq* iseq, struct Ints* labels, struct Ints* label_refs, Val ranges) {
  // example encoding of [a-b c-d e-f]:
  //   jif_range a b L0
  //   jif_range c d L0
  //   jif_range e f L0
  //   die
  //   L0:

  int32_t l0 = _new_label(labels);
  for (Val tail = ranges; tail != VAL_NIL; tail = nb_cons_tail(tail)) {
    Val head = nb_cons_head(tail);
    int32_t from = (int32_t)VAL_TO_INT(nb_struct_get(head, 0));
    int32_t to = (int32_t)VAL_TO_INT(nb_struct_get(head, 1));
    Arg323232 payload = {JIF_RANGE, from, to, l0};
    Ints.push(label_refs, Iseq.size(iseq) + (1 + 2 + 2)); // index of offset
    ENCODE(iseq, Arg323232, payload);
  }

  // die
  ENCODE(iseq, uint16_t, DIE);

  // L0:
  int offset = Iseq.size(iseq);
  *Ints.at(labels, l0) = offset;
}

static void _translate_label_pos(struct Iseq* iseq, struct Ints* labels, struct Ints* label_refs) {
  int refs_size = Ints.size(label_refs);
  for (int i = 0; i < refs_size; i++) {
    int j = *Ints.at(label_refs, i);
    int32_t* ptr = (int32_t*)Iseq.at(iseq, j);
    ptr[0] = *Ints.at(labels, ptr[0]);
  }
}

Val sb_vm_regexp_compile(struct Iseq* iseq, void* arena, Val patterns_dict, Val node) {
  _ensure_tags();

  uint32_t kSeq              = klass_find_c("Seq", sb_klass());
  uint32_t kPredefAnchor     = klass_find_c("PredefAnchor", sb_klass());
  uint32_t kFlag             = klass_find_c("Flag", sb_klass());
  uint32_t kQuantified       = klass_find_c("Quantified", sb_klass());
  uint32_t kQuantifiedRange  = klass_find_c("QuantifiedRange", sb_klass());
  uint32_t kGroup            = klass_find_c("Group", sb_klass());
  uint32_t kCharGroupPredef  = klass_find_c("CharGroupPredef", sb_klass());
  uint32_t kUnicodeCharClass = klass_find_c("UnicodeCharClass", sb_klass());
  uint32_t kBracketCharGroup = klass_find_c("BracketCharGroup", sb_klass());

  bool ignore_case = false;
  struct Stack stack;
  struct Ints labels, label_refs;
  Stack.init(&stack, 25);
  Ints.init(&labels, 15);
  Ints.init(&label_refs, 15);

  /*
  compile stack layout
  - nodes to process are pushed from back to forth.
    since list nodes are already constructed in reverse order,
    so no need to reverse before pushing.
  - special nodes are pushed by compiler, and contents are inlined:
    - match: [MATCH_NODE]
    - label: [label_num, LABEL_NODE]
    - jmp:   [label_num, JMP_NODE]
    - fork:  [label_num2, label_num1, FORK_NODE]
    - atomic: [label_num, LABEL_NODE] e [label_num, ATOMIC_NODE]

  encoding of fork, jmp & labels
  1. create label nodes, only number is stored.
  2. generate label node: fill curr pos in Ints.
     generate fork/jmp node: encode label numbers instead.
  3. go through Splits and Jmps, replace all label numbers with label pos.
  */

  Stack.push(&stack, MATCH_NODE);

  // loop compile
  Val content = nb_struct_get(node, 0);
  Stack.push(&stack, content);
  while (Stack.size(&stack)) {
    Val curr = Stack.pop(&stack);

    if (curr == LABEL_NODE) {
      Val num = Stack.pop(&stack);
      int offset = Iseq.size(iseq);
      *Ints.at(&labels, num) = offset;
      continue;

    } else if (curr == FORK_NODE) {
      int32_t offset1 = (int32_t)Stack.pop(&stack);
      int32_t offset2 = (int32_t)Stack.pop(&stack);
      Ints.push(&label_refs, Iseq.size(iseq) + 1); // offset1
      Ints.push(&label_refs, Iseq.size(iseq) + 3); // offset2
      ENCODE(iseq, Arg3232, ((Arg3232){FORK, offset1, offset2}));
      continue;

    } else if (curr == JMP_NODE) {
      int32_t offset = Stack.pop(&stack);
      Ints.push(&label_refs, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){JMP, offset}));
      continue;

    } else if (curr == MATCH_NODE) {
      ENCODE(iseq, uint16_t, MATCH);
      continue;

    } else if (curr == ATOMIC_NODE) {
      int32_t offset = (int32_t)Stack.pop(&stack);
      Ints.push(&label_refs, Iseq.size(iseq) + 1);
      ENCODE(iseq, Arg32, ((Arg32){ATOMIC, offset}));
      continue;
    }

    uint32_t klass = VAL_KLASS(curr);
    if (klass == KLASS_INTEGER) { // char
      _encode_char(iseq, curr, ignore_case);

    } else if (klass == kSeq) {
      _push_seq(&stack, curr);

    } else if (klass == KLASS_CONS) { // branches
      _push_branches(&stack, &labels, curr);

    } else if (klass == kPredefAnchor) {
      _encode_anchor(iseq, curr);

    } else if (klass == kQuantified) {
      _push_quantified(&stack, &labels, curr, arena);

    } else if (klass == kQuantifiedRange) {
      int res = _push_quantified_range(&stack, &labels, curr);
      if (res != 0) {
        Stack.cleanup(&stack);
        Ints.cleanup(&labels);
        Ints.cleanup(&label_refs);
        return nb_string_new_literal_c("bad char range");
      }

    } else if (klass == kGroup) {
      // TODO

    } else if (klass == kCharGroupPredef) {
      _encode_predef_char_group(iseq, curr);

    } else if (klass == kUnicodeCharClass) {
      // TODO

    } else if (klass == kBracketCharGroup) {
      Val ranges = _flatten_char_group(curr, arena, ignore_case);
      _encode_char_group(iseq, &labels, &label_refs, ranges);

    } else if (klass == kFlag) {
      Val flag = nb_struct_get(curr, 0);
      if (flag == VAL_TRUE) {
        ignore_case = true; // todo language-dependent ignore case
      } else if (flag == VAL_FALSE) {
        ignore_case = false;
      } else {
        fatal_err("encoding flag not supported yet");
      }

    } else {
      Stack.cleanup(&stack);
      Ints.cleanup(&labels);
      Ints.cleanup(&label_refs);
      return nb_string_new_f("unrecognized AST node klass %u", klass);
    }
  }

  _translate_label_pos(iseq, &labels, &label_refs);

  Stack.cleanup(&stack);
  Ints.cleanup(&labels);
  Ints.cleanup(&label_refs);

  ENCODE(iseq, uint16_t, END);
  return VAL_NIL;
}

void sb_vm_regexp_decompile(struct Iseq* iseq, int32_t start, int32_t size) {
  uint16_t* pc_start = Iseq.at(iseq, start);
  uint16_t* pc_end = pc_start + size;
  uint16_t* pc = pc_start;
  while (pc < pc_end) {
    printf("%ld: %s", pc - pc_start, op_code_names[*pc]);
    switch (*pc) {

      case ANCHOR_BOL:
      case ANCHOR_EOL:
      case ANCHOR_WBOUND:
      case ANCHOR_N_WBOUND:
      case ANCHOR_BOS:
      case ANCHOR_N_BOS:
      case ANCHOR_EOS:
      case ANCHOR_N_EOS:
      case CG_ANY:
      case CG_D:
      case CG_N_D:
      case CG_W:
      case CG_N_W:
      case CG_H:
      case CG_N_H:
      case CG_S:
      case CG_N_S:
      case MATCH:
      case DIE:
      case END: {
        printf("\n");
        pc++;
        break;
      }

      case SAVE: {
        printf(" %d\n", DECODE(Arg16, pc).arg1);
        break;
      }

      case JMP:
      case CHAR: {
        printf(" %d\n", DECODE(Arg32, pc).arg1);
        break;
      }

      case CHAR2:
      case FORK:
      case ATOMIC:
      case AHEAD:
      case N_AHEAD: {
        Arg3232 offsets = DECODE(Arg3232, pc);
        printf(" %d %d\n", offsets.arg1, offsets.arg2);
        break;
      }

      case JIF_RANGE: {
        Arg323232 payload = DECODE(Arg323232, pc);
        printf(" %d %d %d\n", payload.arg1, payload.arg2, payload.arg3);
        break;
      }

      default: {
        fatal_err("bad pc: %d", (int)*pc);
      }
    }
  }
}
