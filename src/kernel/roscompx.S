/*
 * Low level kernel code for the rosco_m68k Classic v2 (68010, XR68C681).
 *
 * GNU assembler version of stmpx.s with the Atari ST hardware removed:
 * vector table, startup, trap and interrupt entry, save/restart, lock and
 * unlock, reboot.  The code that does not depend on the hardware (genint,
 * save, restart, sys, the frame size table) is kept as close to stmpx.s as
 * possible; see the comments there.
 *
 * Differences from stmpx.s:
 *  - C symbols have no leading underscore (m68k-elf), and gcc only
 *    clobbers d0-d1/a0-a1, so FREEREGS is d0-d1/a0-a1.
 *  - The image is entered from the rosco loader (src/rosco/loader) with
 *    d0 = 'ROSC' and d1 = memory size; the size is kept for get_mem_size.
 *  - The firmware's RAM at 0x400-0x2000 is left alone (ROSCO_KEEP_FW): the
 *    vectors fill 0x000-0x3FF and the code starts at 0x2000.
 *  - One interrupt source: the DUART on IPL 4 with the vector from its IVR
 *    (ROSCO_DUART_VEC).  It goes through genint to duartint() (roscmain.c).
 *  - No ST model detection, no MFP/ACIA/DMA vectors, no Multiboard probing.
 *  - The bss is cleared here; the loader only copies text and data.
 *  - kreboot jumps through the reset vector in ROM, not through address 4.
 *  - A privilege violation first goes to prv_emulate() (roscmain.c): user
 *    mode "move sr,<ea>" is legal on a 68000 but not on the 68010.
 *
 * All of this file goes into section .text.boot, which the linker script
 * puts first.  genint relies on every trap and interrupt entry lying below
 * the label restart and on all C code lying above it.
 */

#include <minix/config.h>
#if (CHIP == M68000)

#include "const.h"
#include "roscaddr.h"

#define	FREEREGS	d0-d1/a0-a1
#define	SPACEREGS	16

#define	LOADER_MAGIC	0x524F5343		/* 'ROSC', from the loader */
#define	DUART_IMR	(AD_DUART + 0x0B)	/* W: interrupt mask */

/*
 * offsets into a proc table entry (see proc.h; checked by roscmain.c)
 */
sava6	= 56
savsp	= 60
savpc	= 64
savsr	= 68
savtt	= 77

	.globl	lock, unlock, restore, kreboot, test_and_set, get_mem_size
	.globl	sizes, invicache, invdcache, proctyp, fpptyp, mmutyp
	.globl	rosco_memsize, rosco_extra, rosco_extralen
	.globl	restart, start

	.section .text.boot,"ax"

/*
 * Vector table at address 0.
 */
	.long	0		/*   0	 0 reset initial sp (ROM supplies it) */
	.long	start		/*   1	 4 reset initial pc */
	.long	buserr		/*   2	 8 bus error */
	.long	adrerr		/*   3	 c address error */
	.long	illtrp		/*   4	10 illegal instruction */
	.long	zertrp		/*   5	14 zero divide */
	.long	chktrp		/*   6	18 chk instruction */
	.long	trptrp		/*   7	1c trapv instruction */
	.long	prvtrp		/*   8	20 privilege violation */
	.long	trc		/*   9	24 trace */
	.long	atrp		/*  10	28 line 1010 emulator */
	.long	ftrp		/*  11	2c line 1111 emulator */
	.long	trp		/*  12	30 reserved */
	.long	cpvtrp		/*  13	34 coprocessor protocol violation */
	.long	fortrp		/*  14	38 format error */
	.long	trp		/*  15	3c uninitialized interrupt */
	.rept	8
	.long	trp		/* 16-23 reserved */
	.endr
	.long	intr		/*  24	60 spurious interrupt */
	.rept	7
	.long	intr		/* 25-31 autovectors: nothing uses them */
	.endr
	.long	sys		/*  32	80 trap #0, used for system calls */
	.rept	15
	.long	trpntrp		/* 33-47 trap #1-15 (firmware TRAPs are gone) */
	.endr
	.rept	8
	.long	fpcptrp		/* 48-55 fpcp errors */
	.endr
	.rept	3
	.long	pmmutrp		/* 56-58 pmmu errors */
	.endr
	.rept	5
	.long	trp		/* 59-63 reserved */
	.endr
	.rept	ROSCO_DUART_VEC - 64
	.long	non		/* user vectors below the DUART */
	.endr
	.long	duart		/* DUART, vector from IVR */
	.rept	255 - ROSCO_DUART_VEC
	.long	non		/* user vectors above the DUART */
	.endr

/*
 * The firmware's System Data Block, extension function table and its own
 * data/bss: not copied by the loader, not touched by the kernel.
 */
	.space	AD_FW_END - 0x400

/*
 * Startup code, entered from the loader with
 *	d0 = 'ROSC', d1 = memory size, a0/d2 = extra image (address, length)
 * Call main() and do the first process switch when it returns.
 */
start:
	move.w	#0x2700,sr
	move.b	#0,DUART_IMR	/* the loader did this already; be sure */

/* clear the bss; no stack is used until it is done */
	lea	__bss_start,a1
	lea	_end,a2
1:	cmp.l	a2,a1
	bcc	2f
	clr.w	(a1)+
	bra	1b
2:
/* keep the hand-over values */
	cmp.l	#LOADER_MAGIC,d0
	beq	3f
	move.l	#AD_RAM_END,d1	/* not started by our loader: assume 1MB */
	sub.l	a0,a0
	moveq	#0,d2
3:	move.l	d1,rosco_memsize
	move.l	a0,rosco_extra
	move.l	d2,rosco_extralen

	move.l	#k_stktop,sp	/* load sp so exceptions can be stored */

#ifdef FIXED_CPU
	move.b	#0,fpptyp
	move.b	#0,mmutyp
	move.b	#FIXED_CPU,proctyp
#else
/* The firmware has detected the CPU: the top 3 bits of SDB_CPUINFO are the
 * family (0: 68000, 1: 68010, 2: 68020, ...).  No MMU or FPU is used on
 * rosco (SHADOWING, no FPP).
 */
	clr.b	fpptyp
	clr.b	mmutyp
	cmp.l	#SDB_MAGIC_VAL,SDB_MAGIC
	bne	probe_cpu
	move.l	SDB_CPUINFO,d0
	moveq	#29,d1
	lsr.l	d1,d0
	move.b	d0,proctyp
	bra	got_types

/* Without a valid SDB probe the CPU as stmpx.s does.  Note that emulators
 * may not trap the 68020 instructions in 68010 mode.
 */
probe_cpu:
	move.l	0x2c,a0		/* save line 1111 exception handler */
	move.l	#proctype,0x2c	/* install own error handler */
	clr.b	mmutyp		/* no mmu */
	.long	0xf0002400	/* pflusha */
	move.b	#1,mmutyp	/* assume it is a 68851 */
proctype:
	move.l	a0,0x2c		/* restore old handler */
	move.l	#k_stktop,sp

	move.l	0x10,a0		/* save illegal instruction handler */
	move.l	#fputype,0x10	/* install own error handler */
	clr.b	proctyp		/* at least a 68000 */
	.short	0x42c0		/* move ccr,d0: illegal on a 68000 */
	move.b	#1,proctyp	/* 68010 or up */
	move.l	#0x3919,d0
	.long	0x4e7b0002	/* movec.l d0,cacr: illegal below 68020 */
	move.b	#2,proctyp
	.long	0x4e7a0002	/* movec.l cacr,d0 */
	tst.w	d0
	beq	got_68040
	and.w	#0xff00,d0
	beq	fputype		/* no data cache: 68020 */
	move.b	#3,proctyp
	move.b	#3,mmutyp
	bra	fputype
got_68040:
	move.l	#0x80008000,d0	/* enable both caches */
	.long	0x4e7b0002	/* movec.l d0,cacr */
	move.b	#4,proctyp
	move.b	#4,mmutyp
	move.b	#4,fpptyp
fputype:
	move.l	a0,0x10		/* restore illegal instruction handler */
	move.l	#k_stktop,sp

	move.l	0x2c,a0		/* save line 1111 exception handler */
	move.l	#got_types,0x2c	/* install own error handler */
	.short	0xf027		/* fsave -(a7) */
	cmp.b	#0x18,1(a7)	/* 0x18: 68881, 0x38: 68882 */
	bne	got_68882
	move.b	#1,fpptyp
	bra	got_types
got_68882:
	move.b	#2,fpptyp
got_types:
	move.l	a0,0x2c		/* restore line 1111 handler */
#endif /* FIXED_CPU */

	move.l	#k_stktop,sp	/* forget the exception data on the stack */
	jsr	main
	bra	restart

/*
 * The vector handlers for synchronous traps.  Save full context.
 */
buserr:
	move.b	#2,vectnum
	tst.b	proctyp
	bne	gentrp
	add.l	#8,sp		/* remove extra context on 68000 processor */
	bra	gentrp
adrerr:
	move.b	#3,vectnum
	tst.b	proctyp
	bne	gentrp
	add.l	#8,sp		/* remove extra context on 68000 processor */
	bra	gentrp
trp:
	move.b	#12,vectnum
	bra	gentrp
illtrp:
	move.b	#4,vectnum
	bra	gentrp
zertrp:
	move.b	#5,vectnum
	bra	gentrp
chktrp:
	move.b	#6,vectnum
	bra	gentrp
trptrp:
	move.b	#7,vectnum
	bra	gentrp
prvtrp:
	move.b	#8,vectnum
	bsr	save
	jsr	prv_emulate	/* "move sr,<ea>" from 68000 code? */
	tst.w	d0
	bne	1f
	jsr	trap
1:	bra	restart
atrp:
	move.b	#10,vectnum
	bra	gentrp
ftrp:
	move.b	#11,vectnum
	bra	gentrp
cpvtrp:
	move.b	#13,vectnum
	bra	gentrp
fortrp:
	move.b	#14,vectnum
	bra	gentrp
trpntrp:
	move.b	#33,vectnum
	bra	gentrp
fpcptrp:
	move.b	#48,vectnum
	bra	gentrp
pmmutrp:
	move.b	#56,vectnum
	bra	gentrp

gentrp:
	bsr	save
	jsr	trap
	bra	restart
trc:
	move.b	#9,vectnum
	btst	#5,(sp)		/* tracing through trap? */
	beq	gentrp		/* No, do normal trace processing */
	rte			/* do not trace; execute system call */
non:
	bsr	save
	jsr	none
	bra	restart
intr:
	bsr	save
	jsr	rupt
	bra	restart

sys:
	move.b	#32,vectnum
	bsr	save		/* d0, d1 and a0 not modified */
	bsr	unlock		/* do not lock during sys_call */
	move.l	a0,-(sp)	/* m_ptr */
	move.w	d1,-(sp)	/* src_dest */
	move.w	d0,-(sp)	/* SEND/RECEIVE/BOTH */
				/* warning: sys_call may change proc_ptr */
	jsr	sys_call	/* sys_call(func,src_dest,m_ptr) */
	move.l	d0,(a6)		/* a6 is set by save to proc_ptr */
	add.l	#8,sp

	move.w	#0x2700,sr
	tst.b	pick_proc	/* proc_ptr changed in a interrupt routine? */
	beq	1f
	sf	pick_proc	/* clear pick_proc */
	jsr	lock_pick_proc	/* proc_ptr changed -> call lock_pick_proc */
1:	bra	restart

/*
 * The vector handler for the asynchronous DUART interrupt.  Context is
 * saved only if necessary (genint).
 */
duart:
	movem.l	FREEREGS,-(sp)
	move.w	#0,d0
	move.l	#duartint,a0
	bra	genint

/*
 * genint: call the handler in a0 with argument d0.w.  Unchanged from
 * stmpx.s apart from the symbol names; see the comments there.
 */
genint:
	add.b	#1,k_reenter		/* from -1 if not reentering */
	bne	3f
	btst	#5,SPACEREGS(sp)	/* interrupted user mode? */
	beq	2f
	cmp.l	#restart,2+SPACEREGS(sp) /* interrupted supervisor code */
	bge	2f			/* below restart: a trap or irq */
	add.b	#1,k_reenter		/* then avoid a context switch */

	move.l	proc_ptr,-(sp)		/* save proc_ptr */
	move.w	d0,-(sp)		/* push (potential) argument */
	jsr	(a0)			/* call service routine */
	add.l	#2,sp			/* pop argument */
	move.l	(sp)+,d0

	move.w	#0x2700,sr
	sub.b	#2,k_reenter
	cmp.l	proc_ptr,d0
	beq	1f			/* proc_ptr changed? */
	st	pick_proc		/*    yes -> force pick_proc */
	move.l	d0,proc_ptr		/*           and restore proc_ptr */
1:
	movem.l	(sp)+,FREEREGS
	rte

2:
	move.l	proc_ptr,a1		/* save proc_ptr */
	move.l	a1,old_proc_ptr
	move.l	2+SPACEREGS(sp),savpc(a1) /* save pc for profiling in clock.c */

	move.w	d0,-(sp)		/* push argument */
	jsr	(a0)			/* call service routine */
	add.l	#2,sp			/* pop argument */

	move.w	#0x2700,sr
	sub.b	#1,k_reenter
	move.l	old_proc_ptr,d0		/* proc_ptr changed? */
	cmp.l	proc_ptr,d0
	bne	1f
	tst.b	pick_proc		/* proc_ptr changed in an irq? */
	beq	2f
	sf	pick_proc
	jsr	lock_pick_proc
	move.l	old_proc_ptr,d0
	cmp.l	proc_ptr,d0
	beq	2f

1:	move.l	proc_ptr,old_proc_ptr
	move.l	d0,proc_ptr
	movem.l	(sp)+,FREEREGS

	bsr	save
	move.l	old_proc_ptr,proc_ptr
	bra	restart
2:
	movem.l	(sp)+,FREEREGS
	tst.l	held_head		/* are there unserviced interrupts? */
	bne	1f			/* yes: let restart take care */
	rte

1:	bsr	save
	bra	restart
3:
	move.l	proc_ptr,-(sp)		/* save proc_ptr */
	move.w	d0,-(sp)		/* push argument */
	jsr	(a0)			/* call service routine */
	add.l	#2,sp			/* pop argument */
	move.l	(sp)+,d0

	move.w	#0x2700,sr
	sub.b	#1,k_reenter
	cmp.l	proc_ptr,d0
	beq	1f			/* proc_ptr changed? */
	st	pick_proc		/*    yes -> force pick_proc */
	move.l	d0,proc_ptr		/*           and restore proc_ptr */
1:
	movem.l	(sp)+,FREEREGS
	rte

/*
 * Perform task switch by save and restart.
 */
save:
	move.w	#0x2700,sr
	move.l	a6,-(sp)
	move.l	proc_ptr,a6
	movem.l	d0-d7/a0-a5,(a6)
	move.l	(sp)+,sava6(a6)		/* a6 */
	move.l	usp,a1
	btst	#5,4(sp)		/* test old S-bit */
	beq	save_a1_ok
	lea	10(sp),a1		/* assume 68000 */
	tst.b	proctyp
	beq	save_a1_ok		/* yes, it was really a 68000 */
	/* it was not a 68000, look at the frame number in the frame */
	moveq	#0,d6
	move.w	10(sp),d6		/* format/offset word */
	moveq	#10,d7
	lsr.l	d7,d6
	lea	framesizes,a1
	add.l	d6,a1
	move.l	(a1),a1
	add.l	sp,a1
save_a1_ok:
	move.l	a1,savsp(a6)		/* old sp: usp or ksp */
	move.b	vectnum,savtt(a6)	/* trap type */
	move.l	(sp)+,a1		/* return address */
	move.w	(sp)+,savsr(a6)		/* sr */
	move.l	(sp)+,savpc(a6)		/* pc */
	add.b	#1,k_reenter		/* from -1 if not reentering */
	lea	k_stktop,sp
	jmp	(a1)

/*
 * restart: everything above this label is trap or interrupt entry code,
 * everything below it (and all C code) is not; see genint.
 */
restart:
	move.w	#0x2700,sr
	tst.b	k_reenter
	bne	over_call_unhold
	tst.l	held_head
	beq	over_call_unhold
	jsr	unhold
over_call_unhold:
	jsr	checksp
	move.l	proc_ptr,a6
	clr.b	savtt(a6)		/* trap type */
	clr.b	vectnum
	move.l	savsp(a6),a0		/* old sp: usp or ksp */
	btst	#5,savsr(a6)		/* test old S-bit */
	bne	L6			/* jump if S-bit on */
	move.l	a0,usp
	bra	L7
L6:
	move.l	a0,sp
L7:
	tst.b	proctyp
	beq	L8
	move.w	#0,-(sp)		/* format 0 frame */
	jsr	invicache
	jsr	invdcache
L8:
	move.l	savpc(a6),-(sp)		/* pc */
	move.w	savsr(a6),-(sp)		/* sr */
	movem.l	(a6),d0-d7/a0-a6
	move.w	#0x2700,sr
	sub.b	#1,k_reenter
	rte

invicache:
	cmp.b	#1,proctyp
	ble	invi1			/* no instruction cache in 68000/010 */
	cmp.b	#4,proctyp
	bge	invi4			/* different for 68040 */
	move.l	d0,-(sp)
	.long	0x4e7a0002		/* movec.l cacr,d0 */
	bset	#3,d0			/* set the i cache clear bit */
	.long	0x4e7b0002		/* movec.l d0,cacr */
	move.l	(sp)+,d0
	rts
invi4:
	.short	0xf498			/* cinv instruction cache */
invi1:
	rts

invdcache:
	cmp.b	#2,proctyp
	ble	invd1			/* no data cache in 68000/010/020 */
	cmp.b	#4,proctyp
	bge	invd4			/* different for 68040 */
	move.l	d0,-(sp)
	.long	0x4e7a0002		/* movec.l cacr,d0 */
	bset	#11,d0			/* set the d cache clear bit */
	.long	0x4e7b0002		/* movec.l d0,cacr */
	move.l	(sp)+,d0
	rts
invd4:
	.short	0xf458			/* cinv data cache */
invd1:
	rts

/*
 * Reboot: mask the DUART and enter the firmware through the reset vectors
 * in ROM.  Address 0/4 is RAM (our vector table) after the first bus cycles
 * following a hardware reset.
 */
kreboot:
	move.w	#0x2700,sr
	move.b	#0,DUART_IMR
	move.l	AD_ROM_SSP,sp
	move.l	AD_ROM_PC,a0
	jmp	(a0)

lock:
	move.w	sr,d0
	move.w	#0x2700,sr
	rts
restore:
	move.w	4(sp),sr
	rts
unlock:
	move.w	#0x2000,sr		/* no HBL to block on rosco */
	rts
test_and_set:
	move.l	4(sp),a0
	tas	(a0)
	smi	d0
	ext.w	d0
	rts

/*
 * unsigned long get_mem_size(char *start)
 * The size of the RAM block starting at start.  On-board RAM starts at 0;
 * its size comes from the firmware through the loader.  Expansion RAM is
 * not supported yet.
 */
get_mem_size:
	tst.l	4(sp)
	bne	1f
	move.l	rosco_memsize,d0
	rts
1:	moveq	#0,d0
	rts

	.data

/* Patched by the boot image builder; sizes[0] and sizes[1] (kernel text
 * and data in clicks) are filled in by the linker.  The magic in front lets
 * the builder (src/rosco/tools/mkimage.py) find the table.
 */
	.ascii	"rosSIZES"
sizes:
	.short	__ktext_clicks, __kdata_clicks, 0, 0, 0, 0, 0, 0
#if (ENABLE_NETWORKING == 1)
	.short	0, 0, 0, 0
#endif /* ENABLE_NETWORKING == 1 */

/* below are the frame sizes for 680x0 processors, x>0, size in bytes, +4 !!
 * the sizes which are -1 are unused as far as I know.
 */
	.even
framesizes:
	.long	12, 12, 16, 16, -1, -1, -1, 34
	.long	62, 24, 36, 96, -1, -1, -1, -1

	.bss
	.even
	.space	K_STACK_BYTES
k_stktop:

old_proc_ptr:			/* old proc_ptr */
	.space	4
rosco_memsize:			/* RAM size from the loader */
	.space	4
rosco_extra:			/* extra image (root fs) from the loader */
	.space	4
rosco_extralen:
	.space	4
pick_proc:			/* call pick_proc? */
	.space	1
vectnum:			/* trap/interrupt vector number */
	.space	1
proctyp:	/* processor type, 0: 68000/68008, 1: 68010, 2: 68020,
		 *		   3: 68030, 4: 68040 */
	.space	1
fpptyp:		/* fpu processor type, 0: none, 1: 68881, 2: 68882, 4: 68040 */
	.space	1
mmutyp:		/* mmu type, 0: none, 1: 68851, 3: 68030, 4: 68040 */
	.space	1

#endif /* CHIP == M68000 */
