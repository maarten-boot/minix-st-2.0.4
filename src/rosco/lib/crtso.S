/*
 * C run-time start-off for Minix programs built with the rosco GNU
 * toolchain (lib/m68000/rts/ncrtso.s in GNU syntax).
 *
 * EXEC leaves argc (a long), the argv pointers, a null, the envp pointers
 * and a null on the stack.  argc == 0 marks a server started by the kernel
 * (MM, FS, INIT from the boot image): call main() as it is.
 *
 * The data part of this file must be the first data of the program (the
 * linker script puts section .data.crt0 first): the boot image builder
 * looks for the 0xDADA magic at the start of the data and patches INIT's
 * origin and size behind it for FS.
 */

	.section .text.crt0,"ax"
	.globl	_start, environ, _penviron, _penvp, etext, edata
_start:
	move.l	%sp,%a0
	move.l	(%a0)+,%d0		/* long due to lib/exec.c and mm/exec.c */
	beq	2f			/* argc == 0: server, just call main() */
	move.l	%d0,%d1
	addq.l	#1,%d1
	asl.l	#2,%d1			/* pointers are four bytes */
	move.l	%a0,%a1
	add.l	%d1,%a1			/* a1 = envp */
	move.l	%a1,environ
	move.l	%a1,-(%sp)		/* push envp */
	move.l	_penviron,%a1
	move.l	(%sp),(%a1)		/* *_penviron = envp */
	move.l	%a0,-(%sp)		/* push argv */
	move.w	%d0,-(%sp)		/* push argc (int is 16 bits) */
	jsr	main
	add.w	#10,%sp
	bra	3f
2:	jsr	main
3:	move.w	%d0,-(%sp)		/* exit(main(...)) */
	jsr	exit
4:	bra	4b

	.section .data.crt0,"aw"
data_org:	/* space for build to fill in sizes of KERNEL, MM, FS, INIT */
	.short	0xDADA, 0, 0, 0, 0, 0, 0, 0
	.long	0			/* *NULL == 0 */
_penviron:
	.long	_penvp			/* pointer to environ, or hidden pointer */
environ:
	.long	0

	.bss
	.even
_penvp:	.space	4			/* hidden environment vector */
