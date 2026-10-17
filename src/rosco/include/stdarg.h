/* rosco version of <stdarg.h>.
 *
 * The __GNUC__ branch of include/stdarg.h is written for gcc 1 and 2 and
 * does not compile with a current gcc; use the compiler builtins instead.
 * The rosco makefiles put src/rosco/include before ../../include.
 */

#ifndef _STDARG_H
#define _STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last)	__builtin_va_start(ap, last)
#define va_arg(ap, type)	__builtin_va_arg(ap, type)
#define va_end(ap)		__builtin_va_end(ap)

#endif /* _STDARG_H */
