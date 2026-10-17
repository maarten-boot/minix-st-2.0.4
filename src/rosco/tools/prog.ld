/* Link a Minix program for the rosco GNU build (combined I&D, linked at 0;
 * MM relocates it at exec time from the relocations that mkminix.py takes
 * from --emit-relocs).
 *
 * crtso.o comes first (section .text.crt0, entry at 0), the data follows
 * the text directly and starts with crtso's 0xDADA block (.data.crt0),
 * which the boot image builder patches for FS.  rodata goes with the text:
 * it is read only and may be shared.  "end" is the initial break.
 */
OUTPUT_FORMAT("elf32-m68k")
ENTRY(_start)

SECTIONS
{
	. = 0;
	.text : {
		KEEP(*(.text.crt0))
		*(.text .text.*)
		*(.rodata .rodata.*)
		. = ALIGN(4);
		etext = .;
	}
	.data : {
		KEEP(*(.data.crt0))
		*(.data .data.*)
		. = ALIGN(4);
		edata = .;
	}
	.bss : {
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(4);
		end = .;
		_end = .;
	}
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
