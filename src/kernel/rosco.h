/*
 * rosco_m68k Classic v2: kernel functions and variables of the rosco
 * machine layer that the shared kernel code does not know about.  The
 * functions the shared code calls (scr_init, putk, ...) are declared in
 * proto.h as for the Atari ST.
 */

/* roscompx.S */
extern unsigned long rosco_memsize;	/* RAM size from the loader */
extern char *rosco_extra;		/* extra image (root fs) or 0 */
extern unsigned long rosco_extralen;	/* its length in bytes */
extern char proctyp;			/* 0: 68000, 1: 68010, ... */

/* clock.c (PUBLIC for MACHINE == ATARI, but not in proto.h) */
_PROTOTYPE( int clock_handler, (int hook)				);

/* roscmain.c */
_PROTOTYPE( void duartint, (int dummy)					);
_PROTOTYPE( void duart_imr_set, (int bits)				);
_PROTOTYPE( void duart_imr_clr, (int bits)				);

/* roscons.c */
_PROTOTYPE( void cons_int, (void)					);
