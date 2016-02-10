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
  int32_t saved[20];
} Thread;

MUT_ARRAY_DECL(Threads, Thread);
MUT_ARRAY_DECL(Stack, Val);

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
bool sb_vm_regexp_exec(uint16_t* init_pc, int64_t size, const char* str, int32_t* captures) {
  struct Threads ts[2];
  struct Threads* ct = ts;
  struct Threads* nt = ts + 1;
  uint16_t* pc;
  Threads.init(ct, 10);
  Threads.init(nt, 10);

  _add_thread(ct, init_pc, captures);
  for (int j = 0; j <= size; j++) {
    for (int i = 0; i < Threads.size(ct); i++) {
      Thread* t = Threads.at(ct, i);
      pc = t->pc;
      switch (*pc) {
        case CHAR: {
          if (j == size) {
            break;
          }
          int c = DECODE(Arg32, pc).arg1;
          if (str[j] != c) {
            break;
          }
          _add_thread(nt, pc, t->saved);
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
          int offset = DECODE(Arg32, pc).arg1;
          _add_thread(ct, init_pc + offset, t->saved);
          break;
        }

        case FORK: {
          Arg3232 offsets = DECODE(Arg3232, pc);
          _add_thread(ct, init_pc + offsets.arg1, t->saved);
          _add_thread(ct, init_pc + offsets.arg2, t->saved);
          break;
        }

        case SAVE: {
          int16_t save_pos = DECODE(Arg16, pc).arg1;
          if (t->saved[0] < save_pos) {
            t->saved[0] = save_pos;
          }
          t->saved[save_pos] = j;
          _add_thread(ct, pc, t->saved);
          break;
        }

        case AHEAD: {
          int32_t offset = DECODE(Arg32, pc).arg1;
          bool matched = sb_vm_regexp_exec(init_pc + offset, size - j, str + j, t->saved);
          if (matched) {
            _add_thread(ct, pc, t->saved);
          }
        }

        case N_AHEAD: {
          int32_t offset = DECODE(Arg32, pc).arg1;
          bool matched = sb_vm_regexp_exec(init_pc + offset, size - j, str + j, t->saved);
          if (!matched) {
            _add_thread(ct, pc, t->saved);
          }
        }

        case END: {
          // todo error, should never reach here
        }
      }
    }
    if (nt->size == 0) {
      goto not_match;
    }
    SWAP(ct, nt);
    nt->size = 0;
  }

not_match:

  Threads.cleanup(ct);
  Threads.cleanup(nt);
  return false;
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

// labels[num] = pos
MUT_ARRAY_DECL(Labels, int);

// forks = [iseq_pos1, iseq_pos2, ...]
MUT_ARRAY_DECL(Forks, int);

// jmps = [iseq_pos1, iseq_pos2, ...]
MUT_ARRAY_DECL(Jmps, int);

static Val MATCH_NODE=0, LABEL_NODE=0, JMP_NODE=0, FORK_NODE=0;

static void _ensure_tags() {
    if (!MATCH_NODE) {
    void* ptr  = malloc(sizeof(Val) * 4);
    MATCH_NODE = (Val)ptr;
    LABEL_NODE = (Val)(ptr + 1);
    JMP_NODE   = (Val)(ptr + 2);
    FORK_NODE  = (Val)(ptr + 3);
  }
}

static int _new_label(Labels* labels) {
  int i = Labels.size(labels);
  Labels.push(labels, 0);
  return i;
}

static void _push_label(struct Stack* stack, Val label) {
  Stack.push(stack, label);
  Stack.push(stack, LABEL_NODE);
}

static void _push_fork(struct Stack* stack, Val label1, Val label2) {
  Stack.push(stack, label2);
  Stack.push(stack, label1);
  Stack.push(stack, FORK_NODE);
}

static void _push_seq(struct Stack* stack, Val seq) {
  Val content = nb_struct_get(seq, 0);
  for (Val tail = content; tail != VAL_NIL; tail = nb_cons_tail(tail)) {
    Stack.push(stack, nb_cons_head(tail));
  }
}

static void _push_branches(struct Stack* stack, Val branches) {
  /* example encoding e1 | e2 | e3

    fork L1 L2
    L1: e1
    jmp L0
    L2:

    fork L3 L4
    L3: e2
    jmp L0
    L4

    fork L5 L6
    L5: e3
    jmp L0
    L6:

    L0:

  */
  Val label0 = _new_label(&labels);
  _push_label(&stack, label0);
  for (Val tail = branches; tail != VAL_NIL; tail = nb_cons_tail(tail)) {

    Val label1 = _new_label(&labels);
    Val label2 = _new_label(&labels);

    _push_label(&stack, label2);
    _push_jmp(&stack, label0);
    Stack.push(&stack, nb_cons_head(tail));
    _push_label(&stack, label1);

    _push_fork(&stack, label1, label2);
  }
}

static void _encode_range(struct Iseq* iseq, Val range_node) {
}

Val sb_vm_regexp_compile(struct Iseq* iseq, void* arena, Val patterns_dict, Val node) {
  _ensure_tags();

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

  encoding of fork, jmp & labels
  1. create label nodes, only number is stored.
  2. generate label node: fill curr pos in Labels.
     generate fork/jmp node: encode label numbers instead.
  3. go through Splits and Jmps, replace all label numbers with label pos.
  */

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
  struct Labels labels;
  struct Forks forks;
  Stack.init(&stack);
  Labels.init(&labels);
  Forks.init(&forks);

  Stack.push(&stack, MATCH_NODE);

  // loop compile
  Val content = nb_struct_get(node, 0);
  Stack.push(&stack, content);
  while (Stack.size(&stack)) {
    Val curr = Stack.pop(&stack);
    if (curr == LABEL_NODE) {
      continue;
    } else if (curr == FORK_NODE) {
      continue;
    } else if (curr == JMP_NODE) {
      continue;
    } else if (curr == MATCH_NODE) {
      continue;
    }

    uint32_t klass = VAL_KLASS(curr);
    if (klass == KLASS_INTEGER) { // char
      int chr = VAL_TO_INT(curr);
      ENCODE(iseq, Arg32, ((Arg32){CHAR, chr}));

    } else if (klass == kCharRange) {
      _encode_range(&stack, curr);
    } else if (klass == kSeq) {
      _push_seq(&stack, curr);
    } else if (klass == KLASS_CONS) { // branches
      _push_branches(&stack, curr);
    } else if (klass == kPredefAnchor) {
    } else if (klass == kFlag) {
    } else if (klass == kQuantified) {
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

  _compute_labels(iseq, &labels, &forks);

  Stack.cleanup(&stack);
  Labels.cleanup(&labels);
  Forks.cleanup(&forks);

  ENCODE(iseq, uint16_t, END);
  return VAL_NIL;
}
