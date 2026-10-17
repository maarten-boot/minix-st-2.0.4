/* rosco version of <string.h>.
 *
 * gcc calls memcpy, memmove, memset and memcmp by itself (struct copies,
 * zeroing) and passes the length as its own size_t, which is 32 bits even
 * with -mshort.  The Minix size_t is 16 bits (as with ACK), so the Minix
 * functions read the wrong half of the length.  Explicit calls are renamed
 * here to the Minix versions (lib/ansi, built under these names); the plain
 * names are the 32 bit versions in src/rosco/lib/gccmem.c, for gcc.
 * The rosco makefiles put src/rosco/include before ../../include.
 */

#ifndef _ROSCO_STRING_H
#define _ROSCO_STRING_H

#define	memcpy	_memcpy16
#define	memmove	_memmove16
#define	memset	_memset16
#define	memcmp	_memcmp16

#include_next <string.h>

#endif /* _ROSCO_STRING_H */
