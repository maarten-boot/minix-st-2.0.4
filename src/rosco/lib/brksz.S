/*
 * _brksize: the initial break, the end of the bss
 * (lib/m68000/rts/brksz.s; "end" comes from the linker script).
 */
	.data
	.even
	.globl	_brksize
_brksize:
	.long	end
