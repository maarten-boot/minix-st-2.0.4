! EM run-time symbols that ACK's libem.a expects from ACK's own start-off
! (plat/minix68k/boot.s), for programs that start with the Minix crtso
! (lib/m68000/rts/ncrtso.s, which already defines EXIT, .trpim and .trppc).
! Linked right after crtso.o, so that its references to _brk and _write
! pull those in from libc.a.
	.define	.lino, .filn, hol0, .reghp, .limhp, .ignmask
	.define	BRK, WRITE
	.define	EARRAY, ERANGE, ESET, EIDIVZ, EHEAP, EILLINS, ECASE, EBADGTO
	.sect	.text
	.sect	.rom
	.sect	.data
	.sect	.bss

EARRAY	= 0
ERANGE	= 1
ESET	= 2
EIDIVZ	= 6
EHEAP	= 17
EILLINS	= 18
ECASE	= 20
EBADGTO	= 27

	.sect	.text
BRK:
	jmp	__brk
WRITE:
	jmp	__write

	.sect	.data
.reghp:
	.data4	__end		! EM heap pointer
.limhp:
	.data4	__end
! the zeros last: the program's own data follows, and strings(1) (sh2)
! must not see the low byte of __end as part of its first string
hol0:
.lino:
	.data2	0,0		! line number
.filn:
	.data4	0		! file name
	.sect	.bss
	.comm	.ignmask, 4
