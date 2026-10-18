/* The 64 bit (long long) support routines gcc calls, for the ACK passes
 * built as Minix programs (mkpass.py): libgcc has no -mshort variant.  gcc
 * passes the shift count as a 32 bit word (libgcc's shift count mode) even
 * with -mshort, hence the long count below; an int would read the upper,
 * zero half and not shift at all.  Written on
 * the two 32 bit halves only, so that gcc does not call these routines
 * from themselves (32 bit multiply and divide come from m68kmul.S in the
 * rosco libc).  Big endian: the high half comes first.
 */

typedef unsigned long u32;
typedef union {
	long long ll;
	unsigned long long ull;
	struct { u32 hi, lo; } s;
} dw;

long long __ashldi3(long long a, long b)
{
	dw u;

	u.ll = a;
	if (b >= 64) { u.s.hi = u.s.lo = 0; }
	else if (b >= 32) { u.s.hi = u.s.lo << (b - 32); u.s.lo = 0; }
	else if (b > 0) {
		u.s.hi = (u.s.hi << b) | (u.s.lo >> (32 - b));
		u.s.lo <<= b;
	}
	return u.ll;
}

long long __lshrdi3(long long a, long b)
{
	dw u;

	u.ll = a;
	if (b >= 64) { u.s.hi = u.s.lo = 0; }
	else if (b >= 32) { u.s.lo = u.s.hi >> (b - 32); u.s.hi = 0; }
	else if (b > 0) {
		u.s.lo = (u.s.lo >> b) | (u.s.hi << (32 - b));
		u.s.hi >>= b;
	}
	return u.ll;
}

long long __ashrdi3(long long a, long b)
{
	dw u;
	u32 sign;

	u.ll = a;
	sign = (u.s.hi & 0x80000000UL) ? 0xFFFFFFFFUL : 0;
	if (b >= 64) { u.s.hi = u.s.lo = sign; }
	else if (b >= 32) {
		u.s.lo = b == 32 ? u.s.hi
			: (u32) ((long) u.s.hi >> (b - 32));
		u.s.hi = sign;
	} else if (b > 0) {
		u.s.lo = (u.s.lo >> b) | (u.s.hi << (32 - b));
		u.s.hi = (u32) ((long) u.s.hi >> b);
	}
	return u.ll;
}

/* 32 x 32 -> 64 bits, unsigned */
static void mul32(u32 x, u32 y, u32 *hi, u32 *lo)
{
	u32 xl = x & 0xFFFF, xh = x >> 16, yl = y & 0xFFFF, yh = y >> 16;
	u32 ll = xl * yl, lh = xl * yh, hl = xh * yl, hh = xh * yh;
	u32 mid = (ll >> 16) + (lh & 0xFFFF) + (hl & 0xFFFF);

	*lo = (ll & 0xFFFF) | (mid << 16);
	*hi = hh + (lh >> 16) + (hl >> 16) + (mid >> 16);
}

long long __muldi3(long long a, long long b)
{
	dw u, v, r;

	u.ll = a;
	v.ll = b;
	mul32(u.s.lo, v.s.lo, &r.s.hi, &r.s.lo);
	r.s.hi += u.s.hi * v.s.lo + u.s.lo * v.s.hi;
	return r.ll;
}

/* unsigned 64 / 64 by shift and subtract: q = n / d, r = n % d */
static void udivmod(dw n, dw d, dw *q, dw *r)
{
	dw rem, quo;
	int i;

	rem.s.hi = rem.s.lo = 0;
	quo = n;
	for (i = 0; i < 64; i++) {
		/* (rem:quo) <<= 1 */
		rem.s.hi = (rem.s.hi << 1) | (rem.s.lo >> 31);
		rem.s.lo = (rem.s.lo << 1) | (quo.s.hi >> 31);
		quo.s.hi = (quo.s.hi << 1) | (quo.s.lo >> 31);
		quo.s.lo <<= 1;
		/* if rem >= d: rem -= d, set the quotient bit */
		if (rem.s.hi > d.s.hi || (rem.s.hi == d.s.hi && rem.s.lo >= d.s.lo)) {
			u32 lo = rem.s.lo - d.s.lo;
			rem.s.hi = rem.s.hi - d.s.hi - (lo > rem.s.lo);
			rem.s.lo = lo;
			quo.s.lo |= 1;
		}
	}
	*q = quo;
	*r = rem;
}

static void neg(dw *u)
{
	u->s.lo = ~u->s.lo + 1;
	u->s.hi = ~u->s.hi + (u->s.lo == 0);
}

unsigned long long __udivdi3(unsigned long long a, unsigned long long b)
{
	dw n, d, q, r;

	n.ull = a;
	d.ull = b;
	udivmod(n, d, &q, &r);
	return q.ull;
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b)
{
	dw n, d, q, r;

	n.ull = a;
	d.ull = b;
	udivmod(n, d, &q, &r);
	return r.ull;
}

long long __divdi3(long long a, long long b)
{
	dw n, d, q, r;
	int negq = 0;

	n.ll = a;
	d.ll = b;
	if (n.s.hi & 0x80000000UL) { neg(&n); negq = !negq; }
	if (d.s.hi & 0x80000000UL) { neg(&d); negq = !negq; }
	udivmod(n, d, &q, &r);
	if (negq) neg(&q);
	return q.ll;
}

long long __moddi3(long long a, long long b)
{
	dw n, d, q, r;
	int negr = 0;

	n.ll = a;
	d.ll = b;
	if (n.s.hi & 0x80000000UL) { neg(&n); negr = 1; }
	if (d.s.hi & 0x80000000UL) neg(&d);
	udivmod(n, d, &q, &r);
	if (negr) neg(&r);
	return r.ll;
}
