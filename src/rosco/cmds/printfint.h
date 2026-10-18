/* Forced into the gcc build of printf (cmds/Makefile).  printf.c has
 *	#define atoi(a)		strtoul((a), NULL, 0)
 * and passes that unsigned long to printf("%d"), which takes a 16-bit int
 * here: it printed the upper half, 0.  Make strtoul() give an unsigned
 * int in printf.c, so the value is passed as an int (as with ACK's 16-bit
 * int and a 32-bit long, the original printf.c has the same fault).
 */
#include <stdlib.h>
#define strtoul(s, e, b)	((unsigned) (strtoul)(s, e, b))
