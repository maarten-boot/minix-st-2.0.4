/* Loader stub: runs where the rosco firmware loads ROSCODE1.BIN. */
OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
	. = 0x40000;
	.text : { *(.text) *(.rodata*) *(.data) }
	/DISCARD/ : { *(.bss) *(.comment) *(.note*) }
}
