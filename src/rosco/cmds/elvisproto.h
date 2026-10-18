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
