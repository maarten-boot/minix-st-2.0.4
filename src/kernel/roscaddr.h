/*
 * rosco_m68k Classic v2 hardware:
 * Address map
 *
 * Sources: rosco_m68k release/version-2.4 and develop (ec229f9),
 * code/shared/rosco_m68k_public.asm and the r2 address decoder
 * (code/pld/r2/address_decoder).
 */

#define	AD_RAM		0x00000000L	/* on-board RAM, 1MB */
#define	AD_RAM_END	0x00100000L	/* end of on-board RAM */
#define	AD_EXPRAM_END	0x00E00000L	/* expansion RAM may reach up to here */
#define	AD_ROM		0x00E00000L	/* flash ROM, 1MB */
#define	AD_IO		0x00F00000L	/* I/O space, 1MB */

#define	AD_DUART	0x00F00000L	/* XR68C681, odd bytes 0xF00001.. */

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
#define	AD_FW_SDB	0x00000400L	/* System Data Block */
#define	AD_FW_END	0x00002000L	/* end of the firmware's reserved RAM: SDB,
					 * EFP table, VDA, data/bss at 0x1800 */

#define	SDB_MAGIC	0x00000400L	/* long: 0xB105D47A when valid */
#define	SDB_MEMSIZE	0x00000414L	/* long: size of first RAM block */
#define	SDB_UARTBASE	0x00000418L	/* long: base of the console UART */
#define	SDB_CPUINFO	0x0000041CL	/* long: CPU type and speed */

#define	SDB_MAGIC_VAL	0xB105D47AL

/* Extension Function Pointer table: block device entry points */
#define	EFP_SD_INIT	0x0000044CL
#define	EFP_SD_READ	0x00000450L
#define	EFP_SD_WRITE	0x00000454L

/* The firmware loads ROSCODE1.BIN here and jumps to it */
#define	AD_LOAD		0x00040000L
