/* Link the rosco Minix kernel at address 0.
 *
 * The text starts with roscompx.S (section .text.boot: vector table,
 * firmware gap, startup and the trap/interrupt entry code).  The data part
 * (rodata, data, bss) starts on a click boundary, because the kernel tasks'
 * memory maps describe text and data in clicks; sizes[0] and sizes[1] in
 * roscompx.S are filled in from __ktext_clicks and __kdata_clicks.
 */
OUTPUT_FORMAT("elf32-m68k")
ENTRY(start)

SECTIONS
{
	. = 0;
	.text : {
		KEEP(*(.text.boot))
		*(.text .text.*)
	}
	. = ALIGN(256);			/* CLICK_SIZE */
	.data : {
		*(.rodata .rodata.*)
		*(.data .data.*)
	}
	.bss (NOLOAD) : {
		. = ALIGN(2);
		__bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(2);
		_end = .;
	}
	PROVIDE(end = _end);		/* memory.c: extern int end */
	__ktext_clicks = ADDR(.data) >> 8;
	__kdata_clicks = (_end - ADDR(.data) + 255) >> 8;
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
