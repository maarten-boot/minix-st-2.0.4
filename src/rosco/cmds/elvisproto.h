/* Forced into the gcc build of elvis (cmds/Makefile): prototypes for the
 * elvis functions that are called with "sizeof x" (no parentheses, so
 * rosco_sizeof.h does not catch it) as an argument.  gcc's sizeof is 32
 * bits, the K&R definitions take a 16-bit int: tio.c called
 * ttyread(..., sizeof keybuf - cend, ...), ttyread() read a length of 0,
 * read() returned 0 and elvis quit as at the end of an ex script.  The
 * prototypes make gcc convert the argument.  char parameters are declared
 * int, as they are promoted in a K&R definition.
 */
int ttyread(char *_buf, int _len, int _time);
int vgets(int _prompt, char *_buf, int _bsize);

/* rosco: the same for a pointer difference (32 bits): tmp.c called
 * exstring(buf, str - start, '\\') for a modeline, exstring() read a
 * length of 0 and ":set modelines" did nothing.  (Found by compiling
 * elvis with prototypes for all its K&R functions and
 * -Wtraditional-conversion; the other differences are msg()'s arguments,
 * which it passes on to sprintf() as they are.)
 */
void exstring(char *_buf, int _len, int _qchar);

/* rosco: and for the library: cmd1.c called strncpy(prevtag, extra,
 * sizeof prevtag) without a declaration, so strncpy() copied 0 bytes and
 * ":tag name" looked up an empty tag.  <string.h> is compatible with
 * elvis' own "extern char *strchr();".
 */
#include <string.h>
