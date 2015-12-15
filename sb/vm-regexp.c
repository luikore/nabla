// regexp compile and VM

#include "compile.h"

/* definition  number  opnd?  meaning */
#define  END  0  /* no  End of program. */
#define  BOL  1  /* no  Match beginning of line. */
#define  EOL  2  /* no  Match end of line. */
#define  ANY  3  /* no  Match any character. */
#define  ANYOF  4  /* str  Match any of these. */
#define  ANYBUT  5  /* str  Match any but one of these. */
#define  BRANCH  6  /* node  Match this, or the next..\&. */
#define  BACK  7  /* no  "next" ptr points backward. */
#define  EXACTLY  8  /* str  Match this string. */
#define  NOTHING  9  /* no  Match empty string. */
#define  STAR  10  /* node  Match this 0 or more times. */
#define  PLUS  11  /* node  Match this 1 or more times. */
#define  OPEN  20  /* no  Sub-RE starts here. */
      /*  OPEN+1 is number 1, etc. */
#define  CLOSE  30  /* no  Analogous to OPEN. */

/*
 * Opcode notes:
 *
 * BRANCH  The set of branches constituting a single choice are hooked
 *    together with their "next" pointers, since precedence prevents
 *    anything being concatenated to any individual branch.  The
 *    "next" pointer of the last BRANCH in a choice points to the
 *    thing following the whole choice.  This is also where the
 *    final "next" pointer of each individual branch points; each
 *    branch starts with the operand node of a BRANCH node.
 *
 * BACK    Normal "next" pointers all implicitly point forward; BACK
 *    exists to make loop structures possible.
 *
 * STAR,PLUS  '?', and complex '*' and '+', are implemented as circular
 *    BRANCH structures using BACK.  Simple cases (one character
 *    per match) are implemented with STAR and PLUS for speed
 *    and to minimize recursive plunges.
 *
 * OPEN,CLOSE  ...are numbered at compile time.
 */

// returns match number, 0 for no match
int64_t nb_vm_regexp_exec(VmRegexp* regexp, Ctx* ctx) {
  return 0;
}

VmRegexp* nb_vm_regexp_compile(void* arena, Val node, Spellbreak* spellbreak) {
  return NULL;
}

VmRegexp* nb_vm_regexp_from_string(Val s) {
  return NULL;
}
