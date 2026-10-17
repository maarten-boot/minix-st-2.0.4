/* rosco_sizeof.h - give sizeof(...) the type it had with ACK.
 *
 * With gcc -mshort, sizeof yields gcc's size_t, unsigned long (32 bits);
 * with ACK it was unsigned int (16 bits), and old K&R code relies on that:
 * in a call without a prototype, e.g. after "char *malloc();",
 * malloc(n * sizeof(struct x)) passes 4 bytes where 2 are read, so the
 * callee sees the high word (0).  This macro makes "sizeof(...)" an
 * unsigned int again (a macro is not expanded inside itself, so the inner
 * sizeof is the operator).  "sizeof x" without parentheses is unchanged.
 * Use it with -include for code built from such sources.
 */
#ifndef _ROSCO_SIZEOF_H
#define _ROSCO_SIZEOF_H
#define	sizeof(x)	((unsigned) sizeof(x))
#endif
