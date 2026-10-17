/*
 * 32-bit multiply and divide helpers that gcc calls on a 68000/68010.
 * The rosco toolchain has no libgcc built for -mshort, so they are here,
 * for the kernel (libk.a) and for user programs (libc.a).
 */

|*===========================================================================*
|*		32-bit multiply and divide helpers for gcc			     *
|*===========================================================================*
| long __mulsi3(long a, long b), and the unsigned/signed divide and modulo.
| Arguments at 4(sp) and 8(sp), result in d0.  These are called with long
| arguments only, so -mshort does not change their interface.

	.globl	__mulsi3, __udivsi3, __divsi3, __umodsi3, __modsi3

__mulsi3:
	move.w	4(sp),d0		| a.hi
	mulu.w	10(sp),d0		| a.hi * b.lo
	move.w	6(sp),d1		| a.lo
	mulu.w	8(sp),d1		| a.lo * b.hi
	add.w	d1,d0
	swap	d0
	clr.w	d0			| (cross products) << 16
	move.w	6(sp),d1
	mulu.w	10(sp),d1		| a.lo * b.lo
	add.l	d1,d0
	rts

| udiv32: d0 = d0 / d1, d1 = d0 % d1 (unsigned).  Uses a0 as scratch.
udiv32:
	move.l	d2,-(sp)
	cmp.l	#0xFFFF,d1
	bhi	2f
| divisor fits in 16 bits: two divu steps
	move.l	d0,d2
	clr.w	d2
	swap	d2			| d2 = dividend.hi
	divu.w	d1,d2			| d2 = rem:quot of the high word
	move.w	d2,a0			| a0 = quotient high word
	move.w	d0,d2			| d2 = rem.hi : dividend.lo
	divu.w	d1,d2
	moveq	#0,d0
	move.w	a0,d0
	swap	d0
	move.w	d2,d0			| quotient
	clr.w	d2
	swap	d2
	move.l	d2,d1			| remainder
	move.l	(sp)+,d2
	rts
| general case: shift and subtract
2:	move.l	d3,-(sp)
	moveq	#0,d2			| remainder
	moveq	#31,d3
3:	add.l	d0,d0			| shift the next dividend bit ...
	addx.l	d2,d2			| ... into the remainder
	cmp.l	d1,d2
	bcs	4f
	sub.l	d1,d2
	addq.l	#1,d0			| quotient bit
4:	dbra	d3,3b
	move.l	d2,d1
	move.l	(sp)+,d3
	move.l	(sp)+,d2
	rts

__udivsi3:
	move.l	4(sp),d0
	move.l	8(sp),d1
	bra	udiv32

__umodsi3:
	move.l	4(sp),d0
	move.l	8(sp),d1
	bsr	udiv32
	move.l	d1,d0
	rts

| signed: divide the magnitudes; the quotient is negative when the signs
| differ, the remainder takes the sign of the dividend.
__divsi3:
	move.l	8(sp),d1
	move.l	4(sp),d0		| sets N; the movea below does not
	move.l	d0,a1
	bpl	1f
	neg.l	d0
1:	tst.l	d1
	bpl	2f
	neg.l	d1
2:	bsr	udiv32
	move.l	a1,d1
	eor.l	d1,8(sp)		| sign of the quotient (scratch on the stack)
	tst.l	8(sp)
	bpl	3f
	neg.l	d0
3:	move.l	a1,d1
	eor.l	d1,8(sp)		| restore the argument
	rts

__modsi3:
	move.l	8(sp),d1
	move.l	4(sp),d0		| sets N; the movea below does not
	move.l	d0,a1
	bpl	1f
	neg.l	d0
1:	tst.l	d1
	bpl	2f
	neg.l	d1
2:	bsr	udiv32
	move.l	d1,d0
	move.l	a1,d1
	bpl	3f
	neg.l	d0
3:	rts

