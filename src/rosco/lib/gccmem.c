/* memcpy, memmove, memset, memcmp as gcc calls them.
 *
 * gcc emits calls to these for struct assignment and initialization with
 * the length as a 32 bit size_t, even with -mshort.  Minix code calls the
 * 16 bit size_t versions under other names (src/rosco/include/string.h),
 * so these are only reached from compiler generated code.  This file must
 * not include <string.h>, and is compiled with
 * -fno-tree-loop-distribute-patterns so the loops do not turn into calls
 * of themselves.
 */

typedef unsigned long glen_t;		/* gcc's size_t */

void *memcpy(void *dst, const void *src, glen_t n)
{
  char *d = dst;
  const char *s = src;

  if ((((long) d | (long) s) & 1) == 0) {
	while (n >= 4) {
		*(long *) d = *(const long *) s;
		d += 4;
		s += 4;
		n -= 4;
	}
  }
  while (n-- > 0) *d++ = *s++;
  return dst;
}

void *memmove(void *dst, const void *src, glen_t n)
{
  char *d = dst;
  const char *s = src;

  if (d <= s || d >= s + n) return memcpy(dst, src, n);
  d += n;
  s += n;
  while (n-- > 0) *--d = *--s;
  return dst;
}

void *memset(void *dst, int c, glen_t n)
{
  char *d = dst;

  while (n-- > 0) *d++ = c;
  return dst;
}

int memcmp(const void *a, const void *b, glen_t n)
{
  const unsigned char *p = a, *q = b;

  for (; n > 0; n--, p++, q++) {
	if (*p != *q) return *p - *q;
  }
  return 0;
}
