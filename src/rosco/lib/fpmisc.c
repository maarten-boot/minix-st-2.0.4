/* fpmisc.c - frexp() and modf() for the rosco libm.
 *
 * These replace lib/math/frexp.s and modf.s (ACK assembly).  IEEE double,
 * big endian: l[0] holds the sign, the 11 exponent bits and the top 20
 * mantissa bits, l[1] the low 32 mantissa bits.
 */

#include <math.h>

union dbl {
  double d;
  unsigned long l[2];
};

#define	EXP(u)	((int) (((u).l[0] >> 20) & 0x7FF))

double frexp(value, eptr)
double value;
int *eptr;
{
/* Return the mantissa in [0.5, 1) and put the exponent in *eptr. */
  union dbl u;
  int e;

  *eptr = 0;
  u.d = value;
  e = EXP(u);
  if (e == 0x7FF) return value;			/* Inf or NaN */
  if (e == 0) {
	if ((u.l[0] & 0x000FFFFFL) == 0 && u.l[1] == 0) return value; /* 0 */
	u.d *= 18014398509481984.0;		/* denormal: times 2^54 */
	e = EXP(u) - 54;
  }
  *eptr = e - 1022;
  u.l[0] = (u.l[0] & 0x800FFFFFL) | (1022L << 20);
  return u.d;
}

double modf(value, iptr)
double value;
double *iptr;
{
/* Split value into an integral part (*iptr) and a fraction, both with the
 * sign of value.
 */
  union dbl u;
  int e;

  u.d = value;
  e = EXP(u) - 1023;
  if (e < 0) {				/* |value| < 1 */
	u.l[0] &= 0x80000000L;
	u.l[1] = 0;
	*iptr = u.d;
	return value;
  }
  if (e >= 52) {			/* no fraction bits (or Inf/NaN) */
	*iptr = value;
	u.l[0] &= 0x80000000L;
	u.l[1] = 0;
	return u.d;
  }
  if (e < 20) {
	u.l[0] &= ~(0x000FFFFFL >> e);
	u.l[1] = 0;
  } else {
	u.l[1] &= ~(0xFFFFFFFFL >> (e - 20));
  }
  *iptr = u.d;
  return value - u.d;
}
