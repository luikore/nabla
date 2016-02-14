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

// NOTE
// captures[0] stores max index of captures
// captures[1] stores $0.size
static bool _exec(uint16_t* init_pc, int64_t size, const char* init_s, int32_t* captures) {
  struct Threads ts;
  Threads.init(&ts, 10);
  const char* s_end = init_s + size;

  _add_thread(&ts, init_pc, init_s, captures);
  while (ts.size) {
    Thread* t = Threads.at(&ts, ts.size - 1);
    ts.size--;
    uint16_t* pc = t->pc;
    for (;;) {
      switch (*pc) {
        case CHAR: {
          if (t->s < s_end) {
            int c = DECODE(Arg32, pc).arg1;
            int scanned = s_end - t->s;
            int u8_char = utf_8_scan(t->s, &scanned);
            if (u8_char == c) {
              t->s += scanned;
              continue;
            }
          }
          goto thread_dead;
        }

        case RANGE: {
          if (t->s < s_end) {
            Arg3232 range = DECODE(Arg3232, pc);
            int scanned = s_end - t->s;
            int u8_char = utf_8_scan(t->s, &scanned);
            if (u8_char >= range.arg1 && u8_char <= range.arg2) {
              t->s += scanned;
              continue;
            }
          }
          goto thread_dead;
        }

        case MATCH: {
          memcpy(captures, t->captures, CAPTURE_CAP);
          captures[1] = (t->s - init_s);
          Threads.cleanup(&ts);
          return true;
        }

        case JMP: {
          int offset = DECODE(Arg32, pc).arg1;
          pc = init_pc + offset;
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

static void _encode_range(struct Iseq* iseq, Val range_node) {
  Val from = nb_struct_get(range_node, 0);
  Val to = nb_struct_get(range_node, 1);
  Arg3232 range = {RANGE, (int32_t)VAL_TO_INT(from), (int32_t)VAL_TO_INT(to)};
  ENCODE(iseq, Arg3232, range);
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

static void _translate_label_pos(struct Iseq* iseq, struct Ints* labels, struct Ints* jmps, struct Ints* forks) {
  int jmps_size = Ints.size(jmps);
  for (int i = 0; i < jmps_size; i++) {
    int j = *Ints.at(jmps, i);
    // [JMP:uint16_t, offset:int32_t]
    int32_t* ptr = (int32_t*)Iseq.at(iseq, j + 1);
    ptr[0] = *Ints.at(labels, ptr[0]);
  }

  int forks_size = Ints.size(forks);
  for (int i = 0; i < forks_size; i++) {
    int j = *Ints.at(forks, i);
    // [FORK:uint16_t, left:int32_t, right:int32_t]
    int32_t* ptr = (int32_t*)Iseq.at(iseq, j + 1);
    ptr[0] = *Ints.at(labels, ptr[0]);
    ptr[1] = *Ints.at(labels, ptr[1]);
  }
}

Val sb_vm_regexp_compile(struct Iseq* iseq, void* arena, Val patterns_dict, Val node) {
  _ensure_tags();

  uint32_t kSeq              = klass_find_c("Seq", sb_klass());
  uint32_t kPredefAnchor     = klass_find_c("PredefAnchor", sb_klass());
  uint32_t kFlag             = klass_find_c("Flag", sb_klass());
  uint32_t kQuantified       = klass_find_c("Quantified", sb_klass());
  uint32_t kQuantifiedRange  = klass_find_c("QuantifiedRange", sb_klass());
  uint32_t kUnit             = klass_find_c("Unit", sb_klass());
  uint32_t kGroup            = klass_find_c("Group", sb_klass());
  uint32_t kCharGroupPredef  = klass_find_c("CharGroupPredef", sb_klass());
  uint32_t kUnicodeCharClass = klass_find_c("UnicodeCharClass", sb_klass());
  uint32_t kBracketCharGroup = klass_find_c("BracketCharGroup", sb_klass());
  uint32_t kCharRange        = klass_find_c("CharRange", sb_klass());

  struct Stack stack;
  struct Ints labels, arg3232_refs, arg32_refs;
  Stack.init(&stack, 25);
  Ints.init(&labels, 15);
  Ints.init(&arg3232_refs, 5);
  Ints.init(&arg32_refs, 5);

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
      Ints.push(&arg3232_refs, Iseq.size(iseq));
      ENCODE(iseq, Arg3232, ((Arg3232){FORK, offset1, offset2}));
      continue;
    } else if (curr == JMP_NODE) {
      int32_t offset = Stack.pop(&stack);
      Ints.push(&arg32_refs, Iseq.size(iseq));
      ENCODE(iseq, Arg32, ((Arg32){JMP, offset}));
      continue;
    } else if (curr == MATCH_NODE) {
      ENCODE(iseq, uint16_t, MATCH);
      continue;
    } else if (curr == ATOMIC_NODE) {
      int32_t offset = (int32_t)Stack.pop(&stack);
      Ints.push(&arg32_refs, Iseq.size(iseq));
      ENCODE(iseq, Arg32, ((Arg32){ATOMIC, offset}));
      continue;
    }

    uint32_t klass = VAL_KLASS(curr);
    if (klass == KLASS_INTEGER) { // char
      int chr = VAL_TO_INT(curr);
      ENCODE(iseq, Arg32, ((Arg32){CHAR, chr}));

    } else if (klass == kCharRange) {
      _encode_range(iseq, curr);

    } else if (klass == kSeq) {
      _push_seq(&stack, curr);

    } else if (klass == KLASS_CONS) { // branches
      _push_branches(&stack, &labels, curr);

    } else if (klass == kPredefAnchor) {
      _encode_anchor(iseq, curr);

    } else if (klass == kQuantified) {
      _push_quantified(&stack, &labels, curr, arena);

    } else if (klass == kFlag) {

    } else if (klass == kQuantifiedRange) {

    } else if (klass == kUnit) {

    } else if (klass == kGroup) {

    } else if (klass == kCharGroupPredef) {

    } else if (klass == kUnicodeCharClass) {
    } else if (klass == kBracketCharGroup) {
    } else {
      val_throw(nb_string_new_literal_c("unrecognized klass"));
    }
  }
  // Seq[$1]
  // PredefAnchor[$1]
  // Flag[$1]
  // Quantified[$1, $2]
  // QuantifiedRange[$1, $3, $4, $6]
  // Unit[$1]
  // Group[$2, $3]
  // CharGroupPredef[$1]
  // UnicodeCharClass[$1]
  // BracketCharGroup[$1, $2]
  // CharRange[$1, $3]

  _translate_label_pos(iseq, &labels, &arg32_refs, &arg3232_refs);

  Stack.cleanup(&stack);
  Ints.cleanup(&labels);
  Ints.cleanup(&arg32_refs);
  Ints.cleanup(&arg3232_refs);

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
      case MATCH:
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

      case RANGE:
      case FORK:
      case ATOMIC:
      case AHEAD:
      case N_AHEAD: {
        Arg3232 offsets = DECODE(Arg3232, pc);
        printf(" %d %d\n", offsets.arg1, offsets.arg2);
        break;
      }
    }
  }
}
