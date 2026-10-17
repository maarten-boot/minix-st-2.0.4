/*
 * rosco_m68k Classic v2: empty versions of the Atari ST driver functions
 * that the shared kernel code still calls.
 *
 *   fd_timer:	called by clock.c on every scheduling tick (no floppy)
 *   rosco_dummy_mfp: clock.c's init_clock() programs MFP timer C, see
 *		src/rosco/kernel/stmfp.h
 *   errno:	set by the library sigset functions (lib/posix/_sigset.c),
 *		defined by libc in the ACK build
 */

#include "kernel.h"
#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include <termios.h>
#include "tty.h"
#include "../rosco/kernel/stmfp.h"

PUBLIC struct mfp rosco_dummy_mfp;
PUBLIC int errno;

/*===========================================================================*
 *				fd_timer				     *
 *===========================================================================*/
PUBLIC void fd_timer()
{
/* No floppy motor to switch off. */
}

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
