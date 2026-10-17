/* rosco build of lib/other/regexp.c.
 *
 * Its K&R definitions take "char *" where <regexp.h> declares "const
 * char *", which gcc rejects ("argument doesn't match prototype").
 * Compile it with const defined away; nothing in this file depends on it.
 */
#define	const
#include "../../lib/other/regexp.c"
