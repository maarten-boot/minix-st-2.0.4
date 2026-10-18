/*
 * ROSCODE1.BIN loader stub for Minix on the rosco_m68k Classic v2.
 *
 * The rosco firmware loads ROSCODE1.BIN (from SD card or Kermit) to
 * 0x40000 and jumps to its first byte.  The file is
 *
 *	[this stub][payload][optional extra image]
 *
 * and mkrosco fills in the header below.  The payload (the Minix boot image,
 * linked at load_addr, normally 0, vectors first) is copied down to
 * load_addr, except for the bytes in [keep_lo, keep_hi) which are left
 * alone; that range normally protects the firmware's reserved RAM
 * (0x400-0x2000: SDB, EFP table, data/bss) so the firmware SD routines stay
 * usable.
 *
 * Because the payload may be larger than 0x40000 the destination can overlap
 * this stub, so the copy loop is first moved past the end of the file.  The
 * copy runs upwards; dst < src, so overlap is harmless.
 *
 * On entry to the payload:
 *	sr = 0x2700, VBR = 0, all DUART interrupts masked, counter stopped
 *	d0 = 'ROSC'		magic
 *	d1 = memory size	first RAM block in bytes (from the firmware SDB)
 *	a0 = extra image	address of the extra image (0 if none)
 *	d2 = extra length	in bytes
 *	sp = top of the relocated copier stack (scratch, payload sets its own)
 */

#define	DUART_SRA	0xF00003	/* R: status A */
#define	DUART_THRA	0xF00007	/* W: transmit A */
#define	DUART_IMR	0xF0000B	/* W: interrupt mask */
#define	DUART_STOPCC	0xF0001F	/* R: stop counter command */
#define	SR_TXRDY	2		/* bit number in SRA */

#define	SDB_MAGIC	0x400
#define	SDB_MEMSIZE	0x414
#define	SDB_MAGIC_VAL	0xB105D47A
#define	DEFAULT_MEM	0x100000	/* on-board RAM if the SDB is not valid */

#define	COPIER_STACK	0x400		/* stack bytes above the copier */

	.text
	.globl	_start
_start:
	bra.w	entry			/* 4 bytes */
hdr:
	.ascii	"MXRO"			/* +4  magic, checked by mkrosco */
payload_off:	.long	0		/* +8  file offset of the payload */
payload_len:	.long	0		/* +12 length, multiple of 4 */
load_addr:	.long	0		/* +16 destination address */
entry_addr:	.long	0		/* +20 payload entry point */
keep_lo:	.long	0x400		/* +24 do not overwrite [keep_lo, */
keep_hi:	.long	0x2000		/* +28   keep_hi); lo == hi: none */
extra_off:	.long	0		/* +32 file offset of extra image, 0 none */
extra_len:	.long	0		/* +36 its length */
file_len:	.long	0		/* +40 total file length */

entry:
	move.w	#0x2700,%sr
	move.b	#0,DUART_IMR		/* silence the firmware tick */
	tst.b	DUART_STOPCC		/* stop counter, clear its interrupt */

	lea	banner(%pc),%a0
	bsr	puts

	/* Memory size from the firmware, if its SDB looks valid. */
	move.l	#DEFAULT_MEM,%d6
	lea	ramdef(%pc),%a0		/* "RAM? xxxxxxxx": SDB not valid */
	cmp.l	#SDB_MAGIC_VAL,SDB_MAGIC
	bne.s	1f
	move.l	SDB_MEMSIZE,%d6
	lea	ramis(%pc),%a0		/* "RAM xxxxxxxx" (hex, bytes) */
1:	bsr	puts
	move.l	%d6,%d1
	bsr	puthex
	lea	crlf(%pc),%a0
	bsr	puts
	lea	_start(%pc),%a5		/* where the firmware loaded us */

	/* a4 = end of the loaded file, rounded up to 16 bytes */
	move.l	%a5,%d0
	add.l	file_len(%pc),%d0
	add.l	#15,%d0
	and.l	#-16,%d0
	move.l	%d0,%a4

	/* The copier, its stack and some slack must fit in RAM. */
	move.l	%d0,%d1
	add.l	#copier_end-copier+COPIER_STACK+16,%d1
	cmp.l	%d6,%d1
	bls.s	2f
	lea	nomem(%pc),%a0
	bsr	puts
	bra	halt
2:
	/* Move the copier to a4 and give it a stack above itself. */
	lea	copier(%pc),%a0
	move.l	%a4,%a1
	move.w	#(copier_end-copier)/2-1,%d0
3:	move.w	(%a0)+,(%a1)+
	dbf	%d0,3b
	lea	COPIER_STACK(%a1),%sp

	/* Arguments for the copier. */
	move.l	%a5,%a0
	add.l	payload_off(%pc),%a0	/* src */
	move.l	load_addr(%pc),%a1	/* dst */
	move.l	payload_len(%pc),%d0	/* count */
	move.l	keep_lo(%pc),%d2
	move.l	keep_hi(%pc),%d3
	move.l	entry_addr(%pc),%a3
	sub.l	%a2,%a2			/* extra image address */
	move.l	extra_len(%pc),%d5
	beq.s	4f
	move.l	%a5,%a2
	add.l	extra_off(%pc),%a2
4:
	jmp	(%a4)

/*
 * Copier: position independent, runs from above the loaded file.
 *	a0 = src, a1 = dst, d0 = byte count (multiple of 4)
 *	[d2, d3) = destination range to skip
 *	a3 = entry, a2/d5 = extra image, d6 = memory size
 */
	.even
copier:
	lsr.l	#2,%d0			/* count in longs */
	bra.s	cnext
cloop:
	cmp.l	%d2,%a1			/* in the keep range? */
	bcs.s	ccopy
	cmp.l	%d3,%a1
	bcc.s	ccopy
	addq.l	#4,%a0			/* yes: skip this long */
	addq.l	#4,%a1
	bra.s	cnext
ccopy:
	move.l	(%a0)+,(%a1)+
cnext:
	subq.l	#1,%d0
	bcc.s	cloop

	moveq	#0,%d0
	movec	%d0,%vbr		/* vectors at 0 (68010) */
	move.l	#0x524F5343,%d0		/* 'ROSC' */
	move.l	%d6,%d1
	move.l	%a2,%a0
	move.l	%d5,%d2
	jmp	(%a3)
copier_end:

/* Polled output on DUART channel A; a0 = string. */
puts:
	move.b	(%a0)+,%d0
	beq.s	2f
1:	btst	#SR_TXRDY,DUART_SRA
	beq.s	1b
	move.b	%d0,DUART_THRA
	bra.s	puts
2:	rts

/* Polled output of d1 as 8 hex digits; trashes d0, d1, d2. */
puthex:
	moveq	#7,%d2
1:	rol.l	#4,%d1
	move.b	%d1,%d0
	and.b	#15,%d0
	add.b	#'0',%d0
	cmp.b	#'9',%d0
	bls.s	2f
	addq.b	#'A'-'9'-1,%d0
2:	btst	#SR_TXRDY,DUART_SRA
	beq.s	2b
	move.b	%d0,DUART_THRA
	dbf	%d2,1b
	rts

halt:
	stop	#0x2700
	bra.s	halt

banner:	.asciz	"\r\nMinix rosco loader\r\n"
ramis:	.asciz	"RAM "
ramdef:	.asciz	"RAM? "
crlf:	.asciz	"\r\n"
nomem:	.asciz	"loader: image too large for RAM\r\n"
	.even
