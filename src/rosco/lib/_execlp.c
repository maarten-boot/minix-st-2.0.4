/* rosco version of lib/posix/_execlp.c: a bare 0 ending the argument list
 * is taken as the end, as in _execl.c (see there).  Changes are marked
 * "rosco:".
 */
/*	execlp() - execute with PATH search		Author: Kees J. Bot
 *								22 Jan 1994
 */
#define execlp _execlp
#define execvp _execvp
#include <unistd.h>
#include <stdarg.h>

/* rosco: a bare int 0 as the last argument (see _execl.c) */
#define SHORTNULL(p)	((unsigned long) (p) < 0x10000UL)

int execlp(const char *file, const char *arg1, ...)
/* execlp("sh", "sh", "-c", "example", (char *) 0); */
{
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
		return execvp(file, (char * const *) argv);
	}
	return execvp(file, (char * const *) &arg1);
}
