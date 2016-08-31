#pragma once

#include "op-code-helper.h"

enum OpCodes {
  // op         // args                               # description
  META,         // size:uint32, data:void*            # container_size = (uint32_t)data [***]

  // lex specific callback ops
  LOAD,         // id:uint32                          # push var (indexed by id) on to stack
  STORE,        // id:uint32                          # pop stack and set var

  // callback ops, shared with those in vm-peg
  CAPTURE,      // n:uint16                           # load capture at bp[n]
  PUSH,         // val:Val                            # push literal
  POP,          //                                    # pop top of stack
  NODE_BEG,     // klass_id:uint32                    # push [node, (limit, counter=0)] [*]
  NODE_SET,     //                                    # (assume stack top is [node, (limit, counter), val]) node[counter++] = val
  NODE_SETV,    //                                    # (assume stack top is [node, (limit, counter), *vals]) node[counter..counter+vals.size] = *vals
  NODE_END,     //                                    # (assume stack top is [node, (limit, counter)]) finish building node, remove counter from stack top
  LIST,         //                                    # pop b:Cons, a:Val, push [a, *b] (members are pushed from left to right)
  LISTV,        //                                    # pop b:Cons, a:Cons, push [*a, *b] (members are pushed from left to right)
  JIF,          // true_clause:uint32                 # pops cond [**]
  JUNLESS,      // false_clause:uint32                # pops cond
  JMP,          // offset:uint32                      # unconditional jump
  CALL,         // argc:uint32, fname:uint32          # invoke a method (only pure builtin operators are supported), argc includes receiver obj

  END,          //                                    # terminate execution and tweak stack

  OP_CODES_SIZE //
};

// [*] 1: For node building: we can't use LIST/LISTV tricks here...
//     we allocate the node first, and then set attrs one by one or put several attrs by a splat.
//     if attr size exceeds limit of the node, deallocate the node and raise error.
//     (TODO we need some extra matching if node is defined like Foo[bar, *baz])
//     2: TODO If there is error, all tmp nodes in node stack will be deallocated if error handling doesn't resume.

// [**] if we don't pop cond, the following expression is not right: `[(if foo, bar), (if foo, bar)]`

// [***] container_size is the number of total node/list objects, it determines size of container_stack

static const char* op_code_names[] = {
  [META] = "meta",
  [LOAD] = "load",
  [STORE] = "store",
  [CAPTURE] = "capture",
  [PUSH] = "push",
  [POP] = "pop",
  [NODE_BEG] = "node_beg",
  [NODE_SET] = "node_set",
  [NODE_SETV] = "node_setv",
  [NODE_END] = "node_end",
  [LIST] = "list",
  [LISTV] = "listv",
  [JIF] = "jif",
  [JUNLESS] = "junless",
  [JMP] = "jmp",
  [CALL] = "call",
  [END] = "end"
};
