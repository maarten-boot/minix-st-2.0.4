/* strdup() for the ACK passes built as Minix programs (mkpass.py): not in
 * the Minix 2.0.4 C library.
 */
#include <stdlib.h>
#include <string.h>

char *strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = malloc(n);

	if (p != NULL) memcpy(p, s, n);
	return p;
}
