/* rosco: host build on an LP64 system.  The system headers the sources
 * use first, then long as the 32 bit type the Sozobon sources assume (the
 * object and executable formats), and local functions renamed where they
 * clash with the host C library. */
#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#define long int
#define link soz_link
#define strnlen soz_strnlen
#define bcopy soz_bcopy
