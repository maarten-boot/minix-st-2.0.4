/*
 * rosco_m68k Classic v2 hardware:
 * Address map
 *
 * Sources: rosco_m68k firmware releases 2.1-2.42 and develop (all agree),
 * code/shared/rosco_m68k_public.asm and the r2 address decoder
 * (code/pld/r2/address_decoder).
 */

/* Usable from C and from assembler (roscompx.S). */
#ifdef __ASSEMBLER__
#define	_RL(n)		n
#else
#define	_RL(n)		n##L
#endif

#define	AD_RAM		_RL(0x00000000)	/* on-board RAM, 1MB */
#define	AD_RAM_END	_RL(0x00100000)	/* end of on-board RAM */
#define	AD_EXPRAM_END	_RL(0x00E00000)	/* expansion RAM may reach up to here */
#define	AD_ROM		_RL(0x00E00000)	/* flash ROM, 1MB */
#define	AD_IO		_RL(0x00F00000)	/* I/O space, 1MB */

#define	AD_DUART	_RL(0x00F00000)	/* XR68C681, odd bytes 0xF00001.. */

/*
 * Reset vectors in ROM.  After the first four bus cycles following reset,
 * address 0 is RAM, so reboot must go through these and not through 0/4.
 */
#define	AD_ROM_SSP	(AD_ROM + 0)
#define	AD_ROM_PC	(AD_ROM + 4)

/*
 * Firmware System Data Area in low RAM (see the firmware's
 * InterfaceReference.md, section 2, and rosco_m68k_firmware_common.ld:
 * the firmware's data/bss live at 0x1800-0x1FFF).  The kernel vectors
 * overlay 0x000-0x3FF.
 */
#define	AD_FW_SDB	_RL(0x00000400)	/* System Data Block */
#define	AD_FW_END	_RL(0x00002000)	/* end of the firmware's reserved RAM: SDB,
					 * EFP table, VDA, data/bss at 0x1800 */

#define	SDB_MAGIC	_RL(0x00000400)	/* long: 0xB105D47A when valid */
#define	SDB_MEMSIZE	_RL(0x00000414)	/* long: size of first RAM block */
#define	SDB_UARTBASE	_RL(0x00000418)	/* long: base of the console UART */
#define	SDB_CPUINFO	_RL(0x0000041C)	/* long: CPU type and speed */

#define	SDB_MAGIC_VAL	_RL(0xB105D47A)

/* Extension Function Pointer table: block device entry points */
#define	EFP_SD_INIT	_RL(0x0000044C)
#define	EFP_SD_READ	_RL(0x00000450)
#define	EFP_SD_WRITE	_RL(0x00000454)

/* The firmware loads ROSCODE1.BIN here and jumps to it */
#define	AD_LOAD		_RL(0x00040000)
