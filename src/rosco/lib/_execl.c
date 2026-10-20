/* rosco version of lib/posix/_execl.c: execl() takes its own arguments as
 * the argv array.  With -mshort a bare 0 at the end of the list, as in
 * execl("/bin/sh", "sh", "-c", cmd, 0), is a 2-byte int, not a 4-byte null
 * pointer: the slot after the last argument reads as 0x0000xxxx, xxxx being
 * whatever the caller has next on its stack.  No argument of a user
 * process lies below 64K (the kernel, MM and FS are there), so such a value
 * is taken as the end of the list, and the arguments are copied into an
 * array that ends in a real null pointer.  Lists that end in (char *) 0 or
 * NULL go to execve() as before.  Changes are marked "rosco:".
 */
/*	execl() - execute				Author: Kees J. Bot
 *								21 Jan 1994
 */
#define execl _execl
#define execve _execve
#include <unistd.h>
#include <stdarg.h>

extern char * const **_penviron;	/* The default environment. */

/* rosco: a bare int 0 as the last argument (see above) */
#define SHORTNULL(p)	((unsigned long) (p) < 0x10000UL)

int execl(const char *path, const char *arg1, ...)
/* execl("/bin/sh", "sh", "-c", "example", (char *) 0); */
{
	/* Assumption:  The C-implementation for this machine pushes
	 * function arguments downwards on the stack making a perfect
	 * argument array.  Luckily this is almost always so.
	 */
	/* rosco: find the end of the list (with va_arg, so that the
	 * compiler sees the reads past arg1)
	 */
	va_list ap;
	const char *p;
	size_t n;

	va_start(ap, arg1);
	for (n= 0, p= arg1; !SHORTNULL(p); n++) p= va_arg(ap, const char *);
	va_end(ap);
	if (p != 0) {
		/* rosco: ended by a bare 0, copy the list */
		const char *argv[n + 1];
		size_t i;

		va_start(ap, arg1);
		for (i= 0, p= arg1; i < n; i++) {
			argv[i]= p;
			p= va_arg(ap, const char *);
		}
		va_end(ap);
		argv[n]= 0;
		return execve(path, (char * const *) argv, *_penviron);
	}
	return execve(path, (char * const *) &arg1, *_penviron);
}
