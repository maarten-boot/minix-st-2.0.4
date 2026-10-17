/*
 * The console driver for the rosco_m68k Classic v2: a serial terminal on
 * channel A of the XR68C681 DUART.
 *
 * It replaces stcon.c, stvdu.c and stkbd.c and provides the same entry
 * points to tty.c and the rest of the kernel.  The line itself is driven
 * by roscrs232.c (interrupt driven input and output, like /dev/tty00 on
 * channel B); this file adds the kernel's polled output and the debug keys.
 *
 * The entries into this file are:
 *   scr_init:	initialize the console tty (called from tty.c)
 *   putk:	kernel printf() output, polled
 *   cons_debug_key: debug key recognition for roscrs232.c
 *   out_char, flush, func_key, dump, kb_init, kb_timer, vduswitch,
 *   vdu_loadfont: as in the ST console driver
 *
 * The debug keys of the ST console (Ctrl-Alt-F1..F3) are the VT100 keys
 * PF1..PF3 (ESC O P, ESC O Q, ESC O R) here: process table with the
 * uptime, memory map, registers of the billed process.  PF4 (ESC O S)
 * sends a test line out of /dev/tty00 (DUART channel B).
 */

#include "kernel.h"
#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include <termios.h>
#include <minix/com.h>
#include "proc.h"
#include "tty.h"
#include "roscaddr.h"
#include "roscduart.h"
#include "rosco.h"

PRIVATE int pfstate;		/* debug key recognition: 0, ESC, ESC O */

/*===========================================================================*
 *				putk					     *
 *===========================================================================*/
PUBLIC void putk(c)
int c;
{
/* This procedure is used by the kernel version of printf().  printf() ends
 * every call with putk(0), which is not printed.  The output goes straight
 * to the transmitter, between the characters of the interrupt driven
 * output; the test and the write must not be split by that interrupt.
 */
  int s;

  if (c == 0) return;
  if (c == '\n') putk('\r');
  for (;;) {
	s = lock();
	if (DUART->du_sra & SR_TXRDY) break;
	restore(s);
  }
  DUART->du_thra = c;
  restore(s);
}

/*===========================================================================*
 *				out_char				     *
 *===========================================================================*/
PUBLIC void out_char(tp, c)
tty_t *tp;
int c;
{
/* Output a character for the kernel, as stvdu.c does. */

  putk(c);
}

/*===========================================================================*
 *				cons_debug_key				     *
 *===========================================================================*/
PUBLIC int cons_debug_key(tp, c)
tty_t *tp;
int c;
{
/* Recognize PF1..PF3 (ESC O P/Q/R) and do the debug dumps.  Returns 1 if
 * the character was used.  An ESC is held back until the next character
 * shows whether a PF key follows; if not, it is passed on late.
 */
  static char esc[2] = { ESC, 'O' };

  switch (pfstate) {
  case 0:
	if (c != ESC) return(0);
	pfstate = 1;
	return(1);
  case 1:
	if (c == 'O') { pfstate = 2; return(1); }
	pfstate = 0;
	(void) in_process(tp, esc, 1);		/* the held back ESC */
	return(c == ESC ? cons_debug_key(tp, c) : 0);
  default:
	pfstate = 0;
	switch (c) {
	case 'P':
		printf("\nuptime %lu ticks", (unsigned long) get_uptime());
		p_dmp();
		return(1);
	case 'Q':
		map_dmp();
		return(1);
	case 'R':
		reg_dmp(bill_ptr);
		return(1);
#if (NR_RS_LINES > 0)
	case 'S':
		rs_test_line(&tty_table[NR_CONS]);
		return(1);
#endif
	}
	(void) in_process(tp, esc, 2);		/* not ours: pass it on */
	return(0);
  }
}

/*===========================================================================*
 *				scr_init				     *
 *===========================================================================*/
PUBLIC void scr_init(tp)
tty_t *tp;
{
/* Initialize the console: DUART channel A as a serial line. */

  if (tp != &tty_table[0]) return;
  rs_cons_init(tp);
}

/*===========================================================================*
 *		Hooks the shared code calls into the ST console driver	     *
 *===========================================================================*/
PUBLIC void kb_init(tp)
tty_t *tp;
{
/* The "keyboard" is the receiver of channel A, set up by rs_cons_init. */
}

PUBLIC void flush(tp)
tty_t *tp;
{
/* Nothing is held back. */
}

PUBLIC void func_key()
{
/* There are no function keys on a serial console, see cons_debug_key. */
}

PUBLIC void dump()
{
  printf("dump\n");
  reg_dmp(proc_ptr);
  p_dmp();
}

PUBLIC void kb_timer()
{
/* No keyboard repeat to do. */
}

PUBLIC void vduswitch(tp)
tty_t *tp;
{
/* Only one console. */
}

PUBLIC int vdu_loadfont(m_ptr)
message *m_ptr;
{
  return(ENOTTY);
}

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
