/*
 * rosco_m68k Classic v2: empty versions of the Atari ST driver functions
 * that the shared kernel code still calls, and the temporary rs232 line.
 *
 *   fd_timer:	called by clock.c on every scheduling tick (no floppy)
 *   rs_init:	called by tty.c for the rs232 lines; until roscrs232.c
 *		(milestone M3) the line on DUART channel B accepts nothing
 *		and discards its output
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

FORWARD _PROTOTYPE( void rs_discard, (tty_t *tp)			);
FORWARD _PROTOTYPE( void rs_noecho, (tty_t *tp, int c)			);

/*===========================================================================*
 *				fd_timer				     *
 *===========================================================================*/
PUBLIC void fd_timer()
{
/* No floppy motor to switch off. */
}

#if (NR_RS_LINES > 0)
/*===========================================================================*
 *				rs_init					     *
 *===========================================================================*/
PUBLIC void rs_init(tp)
tty_t *tp;
{
/* Temporary rs232 line: no input, output is thrown away. */

  tp->tty_devread = tty_devnop;
  tp->tty_devwrite = rs_discard;
  tp->tty_echo = rs_noecho;
  tp->tty_break = tty_devnop;
}

PRIVATE void rs_discard(tp)
tty_t *tp;
{
  if (tp->tty_outleft == 0) return;
  tp->tty_outcum += tp->tty_outleft;
  tp->tty_outleft = 0;
  tty_reply(tp->tty_outrepcode, tp->tty_outcaller, tp->tty_outproc,
							tp->tty_outcum);
  tp->tty_outcum = 0;
}

PRIVATE void rs_noecho(tp, c)
tty_t *tp;
int c;
{
}
#endif /* NR_RS_LINES > 0 */

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
