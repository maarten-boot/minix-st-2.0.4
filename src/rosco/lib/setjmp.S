/*
 * setjmp/longjmp for the rosco GNU build: lib/m68000/rts/setjmp.s with
 * _SETJMP_SAVES_REGS == 1, as <setjmp.h> selects for gcc.
 *
 * jmp_buf: int __flags (0), long __mask (2), registers (6): d0-d7, a0-a5,
 * a6, sp, pc, ccr -- the order of struct sigregs, so _sigjmp() can
 * sigreturn to it.  The condition codes are saved with "move ccr": "move
 * sr" is privileged on a 68010.
 *
 *	__setjmp(jmp_buf, savemask)	setjmp() and _setjmp() macros
 *	_longjmp(jmp_buf, val)		no signal context restored
 *	longjmp(jmp_buf, val)		restores the signal mask too
 */

SC_SIGCONTEXT	=	2	/* nonzero if signal context is included */

	.text
	.globl	__setjmp, _longjmp, longjmp, _longjmperr

__setjmp:
	link	%a6,#0
	move.l	%a0,-(%sp)		/* save a0 */
	move.l	8(%a6),%a0		/* jmp_buf */
	movem.l	%d0-%d7,6(%a0)		/* data registers */
	move.l	(%sp)+,6+32(%a0)	/* a0 */
	movem.l	%a1-%a5,6+32+4(%a0)	/* a1-a5 */
	move.l	(%a6),6+32+24(%a0)	/* caller's frame pointer */
	pea	8(%a6)			/* caller's stack pointer */
	move.l	(%sp)+,6+32+28(%a0)
	move.l	4(%a6),6+32+32(%a0)	/* program counter */
	move.w	%ccr,%d0
	and.w	#0xFF,%d0
	move.w	%d0,6+32+32+4(%a0)	/* status (condition codes only) */
	clr.w	6+32+32+6(%a0)		/* dummy */
	pea	0			/* signal mask (filled in if needed) */
	move.w	#0,-(%sp)		/* flags: no SC_SIGCONTEXT */
	tst.w	12(%a6)			/* savemask */
	beq	1f
	or.w	#SC_SIGCONTEXT,-6(%a6)	/* flags |= SC_SIGCONTEXT */
	pea	-4(%a6)			/* place to store old mask */
	pea	0			/* set pointer: NULL */
	move.w	#0,-(%sp)		/* how: SIG_BLOCK */
	jsr	sigprocmask
	add.l	#10,%sp
1:	move.l	8(%a6),%a0		/* jmp_buf */
	move.w	(%sp)+,(%a0)+		/* __flags */
	move.l	(%sp)+,(%a0)+		/* __mask */
	clr.w	%d0
	unlk	%a6
	rts

_longjmp:
	link	%a6,#0
	move.l	8(%a6),%a0		/* jmp_buf */
	tst.w	(%a0)			/* no signal context expected */
	bne	_longjmperr
	move.w	12(%a6),%d0		/* value for setjmp to return */
	bne	1f
	moveq	#1,%d0
1:	movem.l	6+4(%a0),%d1-%d7	/* restore d1-d7 */
	movem.l	6+32+4(%a0),%a1-%a7	/* a1-a5, a6, sp */
	move.l	6+32+32(%a0),-(%sp)	/* program counter to return to */
	move.l	6+32(%a0),%a0		/* a0 */
	rts

longjmp:
	link	%a6,#0
	move.l	8(%a6),%a0		/* jmp_buf */
	cmp.w	#SC_SIGCONTEXT,(%a0)	/* signal context present? */
	bne	_longjmperr
	move.w	12(%a6),%d0
	bne	1f
	moveq	#1,%d0
1:	move.w	%d0,-(%sp)
	move.l	8(%a6),-(%sp)
	jsr	_sigjmp			/* lib/posix/_sigreturn.c, no return */

/* A jmp_buf of the wrong kind: lib/other/_longjerr.c reports and aborts. */
_longjmperr:
	jmp	_longjerr
