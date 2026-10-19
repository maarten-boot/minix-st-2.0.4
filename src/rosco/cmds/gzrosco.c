/* gzrosco.c - support for gzip on the rosco_m68k (16-bit int, -mshort).
 *
 * gzip (src/rosco/gzip-1.2.4, built with -DROSCO, see gzip.patch) uses:
 *
 * fcalloc(items, size) for its 64K window and hash tables.  calloc() can't
 *	do 64K with a 16-bit size_t, so the memory comes straight from
 *	sbrk(), in chunks an int can hold.  It is never given back (fcfree):
 *	gzip frees these buffers only when it exits.
 * rosco_read() and rosco_write() for read() and write(): gzip reads its
 *	32K input buffer and writes its 32K window in one call, and FS takes
 *	a count of 32K or more as negative (EINVAL).  At most 16K goes per
 *	call; gzip handles short reads.
 */

#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#define CHUNK	0x4000		/* also the most per read() or write() */

void *fcalloc(items, size)
unsigned items;
unsigned size;
{
  long n = ((long) items * size + 3) & ~3L;
  char *p, *q;
  unsigned c;

  p = sbrk(0);
  if (((long) p & 1) != 0) {		/* keep it word aligned */
	if (sbrk(1) == (char *) -1) return NULL;
	p++;
  }
  for (q = p; n > 0; q += c, n -= c) {
	c = n > CHUNK ? CHUNK : (unsigned) n;
	if (sbrk(c) != q) {		/* out of memory, or not contiguous */
		brk(p);
		return NULL;
	}
	memset(q, 0, c);
  }
  return p;
}

void fcfree(ptr)
void *ptr;
{
}

int rosco_read(fd, buf, n)
int fd;
void *buf;
size_t n;
{
  return read(fd, buf, n > CHUNK ? CHUNK : n);
}

int rosco_write(fd, buf, n)
int fd;
const void *buf;
size_t n;
{
  size_t done = 0;
  int r;

  while (done < n) {
	r = write(fd, (const char *) buf + done, n - done > CHUNK ? CHUNK
								: n - done);
	if (r <= 0) return done > 0 ? (int) done : r;
	done += r;
  }
  return (int) done;
}
