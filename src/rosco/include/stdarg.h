/* rosco version of <stdarg.h>.
 *
 * The __GNUC__ branch of include/stdarg.h is written for gcc 1 and 2 and
 * does not compile with a current gcc.  The Minix headers declare the
 * v*printf functions with a "char *" argument list (stdio.h), so va_list
 * stays a char pointer here instead of gcc's __builtin_va_list (void *).
 * On the 68000 all arguments are on the stack, int-sized or larger, so
 * va_arg walks them with that pointer.
 * The rosco makefiles put src/rosco/include before ../../include.
 */

#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

#define __va_rounded_size(type)	\
  (((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

/* __builtin_va_start (not __builtin_next_arg) tells gcc that the function
 * walks its arguments, so it is not cloned into a fixed argument version.
 * On the 68000 gcc's va_list is a plain pointer into the argument area.
 */
#define va_start(ap, last)	\
  ((void) ({ __builtin_va_list __va; __builtin_va_start(__va, last);	\
	     (ap) = (char *) __va; }))
#define va_arg(ap, type)	\
  ((ap) += __va_rounded_size(type),	\
   *((type *) ((ap) - __va_rounded_size(type))))
#define va_end(ap)		((void) 0)

#endif /* _STDARG_H */
