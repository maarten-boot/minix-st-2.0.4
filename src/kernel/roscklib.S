/*
 * rosco_m68k kernel: assembly utility routines.
 *
 * GNU assembler version of klib68k.s (copyclicks, flipclicks, zeroclicks,
 * phys_copy; unchanged apart from the syntax and the symbol names), plus
 * the 32-bit multiply and divide helpers gcc calls on a 68000/68010.  The
 * rosco toolchain has no libgcc built for -mshort, so they live here.
 *
 * All the routines in here destroy d0,d1,a0,a1. All other regs are preserved.
 */

#include <minix/config.h>
#include <minix/const.h>
#if (CHIP == M68000)

#if CLICK_SIZE < 256
#error "Only CLICK_SIZE >= 256 supported"
#endif
CSHIFT	= CLICK_SHIFT


|
| public labels
|
	.globl	copyclicks	| copy a block of n clicks
	.globl	flipclicks	| exchange 2 blocks of n clicks
	.globl	zeroclicks	| zero a block of n clicks
	.globl	phys_copy	| copy a block of n bytes

	.text

|*===========================================================================*
|*				copyclicks				     *
|*===========================================================================*
|PUBLIC void copyclicks(phys_clicks src, phys_clicks dest, phys_clicks nclicks)

#define	cc_src	4(sp)
#define	cc_dest	6(sp)
#define	cc_ncl	8(sp)

copyclicks:
	move.l	#CSHIFT,d1

	move.l	#0,d0
	move.w	cc_src,d0		| source address in clicks
	lsl.l	d1,d0
	move.l	d0,a0			| source address

	move.l	#0,d0
	move.w	cc_dest,d0		| destination address in clicks
	lsl.l	d1,d0
	move.l	d0,a1			| destination address

	move.l	#0,d0
	move.w	cc_ncl,d0		| number of clicks
	beq	cc_ret			| nothing to copy

	movem.l	d2-d7/a2-a5,-(sp)	| save registers

#if CLICK_SHIFT == 8
	sub.w	#1,d0			| prepare the dbra word counter
#else
	lsl.l	#CSHIFT-8,d0
	bra	cc_lend			| jump to dbra counter tests
cc_oloop:				| each outer loop copies 16 megabytes
	swap	d0
#endif
cc_iloop:				| each inner loop copies 256 bytes
	movem.l	(a0)+,d1-d7/a2-a5
	movem.l	d1-d7/a2-a5,(a1)	|  44 Bytes copied
	movem.l	(a0)+,d1-d7/a2-a5
	movem.l	d1-d7/a2-a5,44(a1)	|  88 Bytes copied
	movem.l	(a0)+,d1-d7/a2-a5
	movem.l	d1-d7/a2-a5,88(a1)	| 132 Bytes copied
	movem.l	(a0)+,d1-d7/a2-a5
	movem.l	d1-d7/a2-a5,132(a1)	| 176 Bytes copied
	movem.l	(a0)+,d1-d7/a2-a5
	movem.l	d1-d7/a2-a5,176(a1)	| 220 Bytes copied
	movem.l	(a0)+,d1-d7/a2-a3
	movem.l	d1-d7/a2-a3,220(a1)	| 256 Bytes copied

	add.w	#256,a1
cc_lend:
	dbra	d0,cc_iloop		| do inner loop count adjustment
#if CLICK_SHIFT != 8
	swap	d0			| exchange high and low part of count
	dbra	d0,cc_oloop		| do outer loop count adjustment
#endif

	movem.l	(sp)+,d2-d7/a2-a5	| restore registers
cc_ret:
	rts


|*===========================================================================*
|*				flipclicks				     *
|*===========================================================================*
|PUBLIC void flipclicks(phys_clicks c1, phys_clicks c2, phys_clicks n)

#define	fc_c1	4(sp)
#define	fc_c2	6(sp)
#define	fc_ncl	8(sp)

flipclicks:
	move.l	#CSHIFT,d1

	move.l	#0,d0
	move.w	fc_c1,d0		| source1 address in clicks
	lsl.l	d1,d0
	move.l	d0,a0			| source1 address

	move.l	#0,d0
	move.w	fc_c2,d0		| source2 address in clicks
	lsl.l	d1,d0
	move.l	d0,a1			| source2 address

	move.l	#0,d0
	move.w	fc_ncl,d0		| number of clicks
	beq	fc_ret			| nothing to flip
	lsl.l	#CSHIFT-6,d0		| we are flipping in 64 byte blocks

	movem.l	d2-d7/a2-a6,-(sp)	| save registers
	bra	fc_lend			| jump to end of loop for dbra

fc_oloop:				| each outer loop flips 4 megabytes
	swap	d0			| get high and low part of count right
fc_iloop:				| each loop flips 64 bytes
	movem.l	(a0)+,d1-d6
	movem.l	(a1)+,d7/a2-a6
	movem.l	d1-d6,-24(a1)
	movem.l	d7/a2-a6,-24(a0)	|  24 Bytes copied
	movem.l	(a0)+,d1-d6
	movem.l	(a1)+,d7/a2-a6
	movem.l	d1-d6,-24(a1)
	movem.l	d7/a2-a6,-24(a0)	|  48 Bytes copied
	movem.l	(a0)+,d1-d4
	movem.l	(a1)+,a2-a5
	movem.l	d1-d4,-16(a1)
	movem.l	a2-a5,-16(a0)		|  64 Bytes copied
fc_lend:
	dbra	d0,fc_iloop		| do inner loop count adjustment
	swap	d0			| exchange high and low part of count
	dbra	d0,fc_oloop		| do outer loop count adjustment

	movem.l	(sp)+,d2-d7/a2-a6	| restore registers

fc_ret:
	rts


|*===========================================================================*
|*				zeroclicks				     *
|*===========================================================================*
|PUBLIC void zeroclicks(phys_clicks dest, phys_clicks nclicks)

#define	zc_dest	4(sp)
#define	zc_ncl	6(sp)

zeroclicks:
	move.l	#CSHIFT,d1


	move.l	#0,d0
	move.w	zc_dest,d0		| address in clicks
	add.w	zc_ncl,d0		| set address to top of area to clear
	lsl.l	d1,d0
	move.l	d0,a0

	move.l	#0,d0
	move.w	zc_ncl,d0		| number of clicks
	beq	zc_ret			| nothing to clear

	movem.l	d2-d7/a2-a6,-(sp)	| save registers

	move.l	#0,d1			| clear registers
	move.l	d1,d2
	move.l	d1,d3
	move.l	d1,d4
	move.l	d1,d5
	move.l	d1,d6
	move.l	d1,d7
	move.l	d1,a1
	move.l	d1,a2
	move.l	d1,a3
	move.l	d1,a4
	move.l	d1,a5
	move.l	d1,a6

#if CLICK_SHIFT == 8
	sub.w	#1,d0			| prepare the dbra word counter
#else
	lsl.l	#CSHIFT-8,d0
	bra	zc_lend			| jump to dbra counter tests
zc_oloop:				| each outer loop clears 16 megabytes
	swap	d0
#endif
zc_iloop:				| each inner loop clears 256 bytes
	movem.l	d1-d7/a1-a6,-(a0)	|  52 Bytes zeroed
	movem.l	d1-d7/a1-a6,-(a0)	| 104 Bytes zeroed
	movem.l	d1-d7/a1-a6,-(a0)	| 156 Bytes zeroed
	movem.l	d1-d7/a1-a6,-(a0)	| 208 Bytes zeroed
	movem.l	d1-d7/a1-a5,-(a0)	| 256 Bytes zeroed
zc_lend:
	dbra	d0,zc_iloop		| do inner loop count adjustment
#if CLICK_SHIFT != 8
	swap	d0			| exchange high and low part of count
	dbra	d0,zc_oloop		| do outer loop count adjustment
#endif

	movem.l	(sp)+,d2-d7/a2-a6	| restore registers
zc_ret:
	rts


|*===========================================================================*
|*				phys_copy				     *
|*===========================================================================*
|PUBLIC void phys_copy(phys_bytes src, phys_bytes dest, phys_bytes n)

#define	pc_src	4(sp)
#define	pc_dest	8(sp)
#define	pc_cnt	12(sp)

phys_copy:
	move.l	pc_src,a0		| load source pointer
	move.l	pc_dest,a1		| load destination pointer
	move.l	a0,d0
	move.l	a1,d1
	eor.b	d1,d0
	btst	#0,d0			| pointers mutually aligned?
	bne	ucopy
	move.l	pc_cnt,d0
	beq	end			| if cnt == 0 we are finished yet
	btst	#0,d1			| pointers aligned, but odd?
	beq	check64			| no
	move.b	(a0)+,(a1)+		| copy odd byte
	sub.l	#1,d0			| decrement count
check64:
	cmp.l	#26,d0			| message copy ?
	beq	copy_mes
	move.l	#63,d1			| +
	cmp.l	d1,d0			| +
	bls	copy4			| + count < 64
	movem.l	d2-d7/a2-a6,-(sp)	| save regs for movem use

|==== copying 256 bytes/loop ====
	clr.w	d1			| count mod 256
	move.b	d0,d1
	lsr.l	#8,d0			| count div 256
	bra	end256
loop256:
	movem.l	(a0)+,d2-d7/a2-a6	| copy 11x4 bytes
	movem.l	d2-d7/a2-a6,(a1)
	movem.l	(a0)+,d2-d7/a2-a6	| copy 11x4 bytes
	movem.l	d2-d7/a2-a6,44(a1)
	movem.l	(a0)+,d2-d7/a2-a6	| copy 11x4 bytes
	movem.l	d2-d7/a2-a6,88(a1)
	movem.l	(a0)+,d2-d7/a2-a6	| copy 11x4 bytes
	movem.l	d2-d7/a2-a6,132(a1)
	movem.l	(a0)+,d2-d7/a2-a6	| copy 11x4 bytes
	movem.l	d2-d7/a2-a6,176(a1)
	movem.l	(a0)+,d2-d7/a2-a4	| copy  9x4 bytes
	movem.l	d2-d7/a2-a4,220(a1)
	lea	256(a1),a1
end256:
	dbra	d0,loop256		| decrement count, test and loop
	move.w	d1,d0			| remainder becomes new count
	beq	done			| more to copy? no	!

|==== copying 64 bytes/loop ====
	and.w	#0x3F,d1		| count mod 64
	lsr.w	#6,d0			| count div 64
	bra	end64
loop64:
	movem.l	(a0)+,d2-d7/a4-a5	| copy 8x4 bytes
	movem.l	d2-d7/a4-a5,(a1)
	movem.l	(a0)+,d2-d7/a4-a5	| copy 8x4 bytes
	movem.l	d2-d7/a4-a5,32(a1)
	lea	64(a1),a1
end64:
	dbra	d0,loop64		| decrement count, test and loop
done:	movem.l	(sp)+,d2-d7/a2-a6	| restore regs for movem use
	move.w	d1,d0			| remainder becomes new count
	beq	end

|==== copying 4 bytes/loop ====
copy4:					| NOTE: count must be max. 63
	move.w	d0,d1
	and.w	#3,d1
	lsr.w	#2,d0			| avoid expensive dbra instruction
	add.w	d0,d0			| by jumping to the appropriate
	neg.w	d0			| move.l instructions offset
	jmp	end4(pc,d0.w)

	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
end4:
	move.w	d1,d0			| remainder becomes new count

|==== copying 1 byte/loop ====
copy1:					| NOTE: count must be max. 15!
	add.w	d0,d0			| avoid expensive dbra instruction
	neg.w	d0			| by jumping to the appropriate
	jmp	end1(pc,d0.w)		| move.b instructions offset

	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
end1:
|==== exit phys_copy ====
end:
	rts

|==== 16 bytes/loop copy for unaligned pointers ====
ucopy:
	move.l	pc_cnt,d0
					| count can be big; test on it	!
	move.l	#16,d1			| == moveq; 4
	cmp.l	d1,d0			| 6
	blt	copy1
copy16:
	move.w	d0,d1
	and.w	#0x0F,d1
	lsr.l	#4,d0
	bra	end16
loop16:
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
end16:
	dbra	d0,loop16
	sub.l	#0x10000,d0		| count can even be bigger (>1MB)
	bhi	loop16			|  (dbra handles only word counters)
	move.w	d1,d0
	bra	copy1

|==== special 26 bytes fast message copy ====
copy_mes:  
	move.l	(a0)+,(a1)+		| fast copy for messages (26 bytes)
	move.l	(a0)+,(a1)+		|   (depends on message size	!) 
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	rts


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
	move.l	4(sp),d0
	move.l	8(sp),d1
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
	move.l	4(sp),d0
	move.l	8(sp),d1
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

|*===========================================================================*
|*		send, receive, sendrec for the kernel tasks			     *
|*===========================================================================*
| From lib/m68000/rts/sndrec.s: _send(dest, ptr), _receive(src, ptr),
| _sendrec(srcdest, ptr); int at 4(sp), message pointer at 6(sp).
| <minix/syslib.h> maps send/receive/sendrec to these names.

	.globl	_send, _receive, _sendrec

SEND	= 1
RECEIVE	= 2
BOTH	= 3

_send:	move.w	#SEND,d0
	bra	1f
_receive:
	move.w	#RECEIVE,d0
	bra	1f
_sendrec:
	move.w	#BOTH,d0
1:	move.w	4(sp),d1		| d1 = dest-src
	move.l	6(sp),a0		| a0 = message pointer
	trap	#0			| trap to the kernel
	rts

|*===========================================================================*
|*		sigset functions					     *
|*===========================================================================*
| As lib/syscall/sig*.s: the public names jump to the hidden ones in
| lib/posix/_sigset.c.

	.globl	sigaddset, sigdelset, sigemptyset, sigfillset, sigismember

sigaddset:	jmp	_sigaddset
sigdelset:	jmp	_sigdelset
sigemptyset:	jmp	_sigemptyset
sigfillset:	jmp	_sigfillset
sigismember:	jmp	_sigismember

#endif /* CHIP == M68000 */
