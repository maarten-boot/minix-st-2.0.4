/* 64 bit disk addresses for the rosco GNU build.
 *
 * Replaces lib/m68000/misc/u64.c, whose cmp64u() is wrong:
 *	return((int)(u64._[1]>(unsigned long)u - u64._[1]<(unsigned long)u));
 * parses as ((x > (u - x)) < u), which is 0 for u == 0 whatever x is, so a
 * device size never compared unequal to 0.  mul64u() lost the high word.
 * Same functions and the same layout: _[0] is the high, _[1] the low long.
 */

#include <minix/u64.h>

unsigned long cv64ul(u64_t u64)
{
  return(u64._[0] != 0 ? 0xFFFFFFFFUL : u64._[1]);
}

u64_t cvu64(unsigned int u)		/* this for mkfs */
{
  u64_t u64;

  u64._[0] = 0;
  u64._[1] = (unsigned long) u;
  return(u64);
}

u64_t cvul64(unsigned long ul)
{
  u64_t u64;

  u64._[0] = 0;
  u64._[1] = ul;
  return(u64);
}

unsigned long div64u(u64_t u64, unsigned u)
{
/* Only the low long is divided: disk addresses here stay below 4GB. */
  return(u64._[1] / (unsigned long) u);
}

int cmp64u(u64_t u64, unsigned u)
{
  if (u64._[0] != 0) return(1);
  if (u64._[1] > (unsigned long) u) return(1);
  if (u64._[1] < (unsigned long) u) return(-1);
  return(0);
}

u64_t mul64u(unsigned long ul, unsigned u)
{
/* 32 x 16 bit multiply with a 48 bit result. */
  u64_t u64;
  unsigned long lo, hi;

  lo = (ul & 0xFFFFUL) * u;
  hi = (ul >> 16) * u + (lo >> 16);
  u64._[0] = hi >> 16;
  u64._[1] = (hi << 16) | (lo & 0xFFFFUL);
  return(u64);
}
