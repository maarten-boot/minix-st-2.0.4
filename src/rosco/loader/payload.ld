/* Test payload linked at 0, vectors first, like the Minix kernel. */
OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
	. = 0;
	.text : { *(.text) *(.rodata*) }
	.data : { *(.data) }
	.bss (NOLOAD) : { *(.bss) *(COMMON) }
	/DISCARD/ : { *(.comment) *(.note*) }
}
