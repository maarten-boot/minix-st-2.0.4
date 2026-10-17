/*
 * M1 test payload for the rosco loader.
 *
 * Linked at 0 like the Minix kernel: vector table at 0, then a gap up to
 * 0x2000 which the loader does not copy (the firmware's reserved RAM: SDB,
 * EFP table, VDA and its data/bss stay intact), then code.  It checks the
 * pieces the Minix kernel will rely on:
 *
 *	- the loader hand-over (d0 magic, d1 memory size)
 *	- the firmware SDB magic at 0x400 survived the copy
 *	- the DUART vectored interrupt (IVR 0x45, IPL 4) and the counter/timer
 *	  at 60 Hz (preset 1920): prints a '.' every second
 *	- unexpected exceptions are reported with their vector number
 *	- 'q' returns to the firmware through the ROM reset vector
 * Other characters typed on the console are echoed.
 */

#define	DUART_SRA	0xF00003	/* R: status A */
#define	DUART_RHRA	0xF00007	/* R: receive A */
#define	DUART_THRA	0xF00007	/* W: transmit A */
#define	DUART_ACR	0xF00009	/* W: auxiliary control */
#define	DUART_IMR	0xF0000B	/* W: interrupt mask */
#define	DUART_CTUR	0xF0000D	/* W: counter preset high */
#define	DUART_CTLR	0xF0000F	/* W: counter preset low */
#define	DUART_IVR	0xF00019	/* RW: interrupt vector */
#define	DUART_STARTCC	0xF0001D	/* R: start counter command */
#define	DUART_STOPCC	0xF0001F	/* R: stop counter (timer mode: ack) */

#define	SR_RXRDY	0		/* bit numbers in SRA */
#define	SR_TXRDY	2
#define	ACR_BRG2	0x80		/* keep: console uses rate set 2 */
#define	ACR_TIMER16	0x70		/* timer mode, X1/16 */
#define	ISR_CTR		0x08

#define	HZ		60
#define	TIMER_PRESET	(3686400 / 16 / 2 / HZ)	/* 1920 */
#define	DUART_VEC	0x45

#define	SDB_MAGIC	0x400
#define	SDB_MAGIC_VAL	0xB105D47A
#define	ROM_SSP		0xE00000
#define	ROM_PC		0xE00004

	.text
	.globl	_start
_start:
vectors:
	.long	0			/* 0: reset SSP (unused, RAM) */
	.long	start			/* 1: reset PC */
	.rept	DUART_VEC - 2
	.long	unexpected		/* 2 .. 0x44 */
	.endr
	.long	tick			/* 0x45: DUART */
	.rept	256 - DUART_VEC - 1
	.long	unexpected		/* 0x46 .. 0xFF */
	.endr

	/* Firmware reserved RAM 0x400-0x2000: not copied by the loader. */
	.space	0x2000 - 0x400

start:
	move.w	#0x2700,%sr
	move.l	%d1,%sp			/* stack at the top of RAM */
	move.l	%d0,%d7
	move.l	%d1,%d6

	lea	s_hello(%pc),%a0
	bsr	puts

	lea	s_magic(%pc),%a0
	bsr	puts
	move.l	%d7,%d0
	bsr	puthex
	cmp.l	#0x524F5343,%d7
	beq.s	1f
	lea	s_bad(%pc),%a0
	bsr	puts
1:	lea	s_mem(%pc),%a0
	bsr	puts
	move.l	%d6,%d0
	bsr	puthex

	lea	s_sdb(%pc),%a0
	bsr	puts
	lea	s_ok(%pc),%a0
	cmp.l	#SDB_MAGIC_VAL,SDB_MAGIC
	beq.s	2f
	lea	s_bad(%pc),%a0
2:	bsr	puts

	/* 60 Hz tick from the DUART counter/timer, vectored on IPL 4. */
	clr.w	ticks
	move.b	#DUART_VEC,DUART_IVR
	move.b	#ACR_BRG2+ACR_TIMER16,DUART_ACR
	move.b	#TIMER_PRESET>>8,DUART_CTUR
	move.b	#TIMER_PRESET&0xFF,DUART_CTLR
	tst.b	DUART_STARTCC
	move.b	#ISR_CTR,DUART_IMR
	lea	s_tick(%pc),%a0
	bsr	puts
	move.w	#0x2300,%sr		/* allow IPL 4 */

loop:
	btst	#SR_RXRDY,DUART_SRA
	beq.s	loop
	move.b	DUART_RHRA,%d0
	cmp.b	#'q',%d0
	beq.s	reboot
	bsr	putc
	bra.s	loop

/* Back to the firmware: address 0/4 is RAM now, use the ROM vectors. */
reboot:
	move.w	#0x2700,%sr
	move.b	#0,DUART_IMR
	tst.b	DUART_STOPCC
	lea	s_bye(%pc),%a0
	bsr	puts
	move.l	ROM_SSP,%sp
	move.l	ROM_PC,%a0
	jmp	(%a0)

tick:
	tst.b	DUART_STOPCC		/* acknowledge, timer keeps running */
	addq.w	#1,ticks
	cmp.w	#HZ,ticks
	bcs.s	1f
	clr.w	ticks
	move.l	%d0,-(%sp)
	move.b	#'.',%d0
	bsr	putc
	move.l	(%sp)+,%d0
1:	rte

/* 68010 frame: sr(2) pc(4) format/vector offset(2) */
unexpected:
	move.w	#0x2700,%sr
	lea	s_unexp(%pc),%a0
	bsr	puts
	moveq	#0,%d0
	move.w	6(%sp),%d0
	and.w	#0x0FFF,%d0
	lsr.w	#2,%d0
	bsr	puthex
	lea	s_pc(%pc),%a0
	bsr	puts
	move.l	2(%sp),%d0
	bsr	puthex
1:	stop	#0x2700
	bra.s	1b

/* Polled console output. puts: a0 = string; putc: d0.b; puthex: d0.l */
puts:
	move.b	(%a0)+,%d0
	beq.s	1f
	bsr.s	putc
	bra.s	puts
1:	rts

putc:
	btst	#SR_TXRDY,DUART_SRA
	beq.s	putc
	move.b	%d0,DUART_THRA
	rts

puthex:
	movem.l	%d1-%d2,-(%sp)
	move.l	%d0,%d1
	moveq	#7,%d2
1:	rol.l	#4,%d1
	move.b	%d1,%d0
	and.b	#0x0F,%d0
	add.b	#'0',%d0
	cmp.b	#'9',%d0
	bls.s	2f
	add.b	#'A'-'9'-1,%d0
2:	bsr.s	putc
	dbf	%d2,1b
	movem.l	(%sp)+,%d1-%d2
	rts

s_hello: .asciz	"rosco M1 test payload running at 0x2000\r\n"
s_magic: .asciz	"  loader magic: "
s_mem:	.asciz	"\r\n  memory size: "
s_sdb:	.asciz	"\r\n  firmware SDB at 0x400: "
s_ok:	.asciz	"intact\r\n"
s_bad:	.asciz	" BAD\r\n"
s_tick:	.asciz	"  DUART timer on, one '.' per second; type to echo, q = reboot\r\n"
s_bye:	.asciz	"\r\nback to firmware\r\n"
s_unexp: .asciz	"\r\nunexpected exception, vector "
s_pc:	.asciz	" pc "
	.even

	.bss
ticks:	.space	2
