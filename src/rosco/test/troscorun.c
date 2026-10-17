/* troscorun - test the gcc run-time support of the rosco build.
 *
 * The rosco libc provides what gcc -mshort calls behind the program's back:
 * 32 bit multiply/divide/modulo (lib/m68kmul.S), memcpy & co. with a 32 bit
 * length for struct copies (lib/gccmem.c), and varargs (include/stdarg.h).
 * And the kernel's emulation of "move sr,<ea>" in user mode, which old
 * 68000 binaries use but which is privileged on the 68010.
 * Each check prints the expression on failure; the exit status is the
 * number of failures.  Run from /usr/test.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int errs;

#define	CHECK(expr, want) \
  check((long) (expr), (long) (want), #expr, __LINE__)

void check(got, want, what, line)
long got, want;
char *what;
int line;
{
  if (got == want) return;
  printf("line %d: %s = %ld (0x%lx), want %ld (0x%lx)\n",
	line, what, got, got, want, want);
  errs++;
}

/* volatile: no constant folding, the helpers must do the work */
volatile long l_pos = 1000000L, l_neg = -1000000L, l_big = 0x7FFFFFF0L;
volatile long l_7 = 7, l_m7 = -7, l_60 = 60, l_m60 = -60, l_big_div = 100000L;
volatile unsigned long u_big = 0xFFFFFFF0UL, u_3 = 3, u_70000 = 70000UL;

struct blob { char c[100]; long tail; };

int sum(int n, ...)
{
  va_list ap;
  int s = 0;

  va_start(ap, n);
  while (n-- > 0) s += va_arg(ap, int);
  va_end(ap);
  return s;
}

long lsum(int n, ...)
{
  va_list ap;
  long s = 0;

  va_start(ap, n);
  while (n-- > 0) s += va_arg(ap, long);
  va_end(ap);
  return s;
}

int main()
{
  struct blob a, b;
  int i;

  printf("Test rosco run-time ");

  /* multiply */
  CHECK(l_pos * l_7, 7000000L);
  CHECK(l_neg * l_7, -7000000L);
  CHECK(l_neg * l_m7, 7000000L);
  CHECK(l_big_div * l_big_div, 1410065408L);	/* 10^10 mod 2^32 */
  CHECK(u_70000 * u_70000, 605032704L);		/* 4.9e9 mod 2^32 */

  /* signed divide and modulo, all sign combinations */
  CHECK(l_pos / l_7, 142857L);
  CHECK(l_pos % l_7, 1L);
  CHECK(l_neg / l_7, -142857L);
  CHECK(l_neg % l_7, -1L);
  CHECK(l_pos / l_m7, -142857L);
  CHECK(l_pos % l_m7, 1L);
  CHECK(l_neg / l_m7, 142857L);
  CHECK(l_neg % l_m7, -1L);
  CHECK(l_m60 / l_60, -1L);
  CHECK((l_m60 + 59) / l_60, 0L);
  CHECK(l_big / l_big_div, 21474L);
  CHECK(l_big % l_big_div, 83632L);
  CHECK(l_big / l_7, 306783376L);

  /* unsigned divide and modulo, small and large divisors */
  CHECK(u_big / u_3, 0x55555550L);
  CHECK(u_big % u_3, 0L);
  CHECK(u_big / u_70000, 61356L);
  CHECK(u_big % u_70000, 47280L);
  CHECK(u_big / (u_big - 1), 1L);

  /* struct copy: gcc calls memcpy with a 32 bit length */
  for (i = 0; i < sizeof(a.c); i++) a.c[i] = i;
  a.tail = 0x12345678L;
  memset(&b, 0, sizeof(b));
  b = a;
  CHECK(b.tail, 0x12345678L);
  CHECK(b.c[99], 99);
  CHECK(memcmp(&a, &b, sizeof(a)), 0);

  /* move sr, in three addressing modes: Dn, (An), d16(An) */
  {
	unsigned short w[4];
	long d = 0x12340000L;

	w[0] = w[2] = 0xFFFF;
	__asm__ volatile ("move.w %%sr,%0" : "+d" (d));
	__asm__ volatile ("move.w %%sr,(%0)" : : "a" (&w[0]) : "memory");
	__asm__ volatile ("move.w %%sr,4(%0)" : : "a" (&w[0]) : "memory");
	/* user mode (S bit clear); Minix runs user code at IPL 2 */
	CHECK(d & 0xFFFF2000L, 0x12340000L);	/* high word kept */
	CHECK(w[0] & 0x2000, 0);
	CHECK(w[2] & 0x2000, 0);
	CHECK(w[0], w[2]);
  }

  /* varargs */
  CHECK(sum(4, 1, 2, 3, 4), 10);
  CHECK(lsum(3, 100000L, 200000L, -50000L), 250000L);

  if (errs == 0) printf("ok\n");
  return errs;
}
