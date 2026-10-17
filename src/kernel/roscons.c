/*
 * The console driver for the rosco_m68k Classic v2: a serial terminal on
 * channel A of the XR68C681 DUART.
 *
 * It replaces stcon.c, stvdu.c and stkbd.c and provides the same entry
 * points to tty.c and the rest of the kernel.  Output is polled (the line
 * runs at 115200 baud, 87us per character); input is interrupt driven:
 * cons_int() is called from duartint() (roscmain.c) and puts the received
 * characters in a small buffer, which the TTY task drains with cons_read().
 *
 * The entries into this file are:
 *   scr_init:	initialize the console tty (called from tty.c)
 *   cons_int:	DUART channel A receive interrupt
 *   putk:	kernel printf() output
 *   out_char, console, flush, func_key, dump, kb_init, kb_timer,
 *   vduswitch, vdu_loadfont: as in the ST console driver
 *
 * The debug keys of the ST console (Ctrl-Alt-F1..F3) are the VT100 keys
 * PF1..PF3 (ESC O P, ESC O Q, ESC O R) here: process table with the
 * uptime, memory map, registers of the billed process.
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

#define CONS_IBUFSIZE	64	/* receive buffer, filled by cons_int() */
#define CONS_OBUFSIZE	64	/* output processing buffer */

PRIVATE char ibuf[CONS_IBUFSIZE];
PRIVATE char *ihead = ibuf;	/* next free spot, advanced by cons_int() */
PRIVATE char *itail = ibuf;	/* next char for cons_read() */
PRIVATE int icount;		/* # chars in ibuf */
PRIVATE int ioverrun;		/* chars lost because ibuf was full */
PRIVATE int pfstate;		/* debug key recognition: 0, ESC, ESC O */

FORWARD _PROTOTYPE( void cons_putc, (int c)				);
FORWARD _PROTOTYPE( void cons_read, (tty_t *tp)				);
FORWARD _PROTOTYPE( void cons_echo, (tty_t *tp, int c)			);
FORWARD _PROTOTYPE( int debug_key, (tty_t *tp, int c)			);

/*===========================================================================*
 *				cons_putc				     *
 *===========================================================================*/
PRIVATE void cons_putc(c)
int c;
{
/* Send one character, waiting for the transmitter. */

  while ((DUART->du_sra & SR_TXRDY) == 0)
	;
  DUART->du_thra = c;
}

/*===========================================================================*
 *				putk					     *
 *===========================================================================*/
PUBLIC void putk(c)
int c;
{
/* This procedure is used by the kernel version of printf().  printf() ends
 * every call with putk(0), which is not printed.
 */

  if (c == 0) return;
  if (c == '\n') cons_putc('\r');
  cons_putc(c);
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
 *				console					     *
 *===========================================================================*/
PUBLIC void console(tp)
register tty_t *tp;
{
/* (*devwrite)() routine for the console.  Copy the user bytes in pieces,
 * do the output processing (out_process may map LF to CR LF and then stops
 * early, so only 'count' input bytes are consumed) and send the result.
 */
  char buf[CONS_OBUFSIZE];
  int count, ocount, i;
  phys_bytes user_phys;

  while ((count = tp->tty_outleft) != 0 && !tp->tty_inhibited) {
	if (count > CONS_OBUFSIZE / 2) count = CONS_OBUFSIZE / 2;
	ocount = CONS_OBUFSIZE;

	user_phys = proc_vir2phys(proc_addr(tp->tty_outproc), tp->tty_out_vir);
	phys_copy(user_phys, vir2phys(buf), (phys_bytes) count);
	out_process(tp, buf, buf, buf + CONS_OBUFSIZE, &count, &ocount);
	if (count == 0) break;

	for (i = 0; i < ocount; i++) cons_putc(buf[i]);

	/* Assume echoing messed up by output. */
	tp->tty_reprint = TRUE;

	tp->tty_out_vir += count;
	tp->tty_outcum += count;
	tp->tty_outleft -= count;
  }

  /* Reply to the writer if all output is finished. */
  if (tp->tty_outleft == 0 && tp->tty_outcum != 0) {
	tty_reply(tp->tty_outrepcode, tp->tty_outcaller, tp->tty_outproc,
							tp->tty_outcum);
	tp->tty_outcum = 0;
  }
}

/*===========================================================================*
 *				cons_echo				     *
 *===========================================================================*/
PRIVATE void cons_echo(tp, c)
tty_t *tp;
int c;
{
/* Echo one character, with output processing. */
  char buf[2];
  int count = 1, ocount = 2, i;

  buf[0] = c;
  out_process(tp, buf, buf, buf + 2, &count, &ocount);
  for (i = 0; i < ocount; i++) cons_putc(buf[i]);
}

/*===========================================================================*
 *				cons_int				     *
 *===========================================================================*/
PUBLIC void cons_int()
{
/* Channel A has received data (called from duartint with interrupts off).
 * Empty the receive FIFO into ibuf and tell the TTY task.
 */
  int c, sr;

  while ((sr = DUART->du_sra) & SR_RXRDY) {
	c = DUART->du_rhra;
	if (sr & SR_ERRS) {
		DUART->du_cra = CR_RSTERR;
		if (sr & SR_RB) {
			DUART->du_cra = CR_RSTBRK;
			continue;		/* a break is no character */
		}
	}
	if (icount < CONS_IBUFSIZE) {
		*ihead++ = c;
		if (ihead == bufend(ibuf)) ihead = ibuf;
		icount++;
	} else {
		ioverrun++;
	}
  }
  tty_table[0].tty_events = 1;
  force_timeout();
}

/*===========================================================================*
 *				cons_read				     *
 *===========================================================================*/
PRIVATE void cons_read(tp)
tty_t *tp;
{
/* (*devread)() routine: pass the received characters to the tty layer. */
  char c;

  while (icount > 0) {
	c = *itail++;
	if (itail == bufend(ibuf)) itail = ibuf;
	lock();
	icount--;
	unlock();
	if (debug_key(tp, c & 0xFF)) continue;
	(void) in_process(tp, &c, 1);
  }
}

/*===========================================================================*
 *				debug_key				     *
 *===========================================================================*/
PRIVATE int debug_key(tp, c)
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
	return(c == ESC ? debug_key(tp, c) : 0);
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
/* Initialize the console: hook the device functions into the tty.  The
 * DUART itself is set up by initrosco() (roscmain.c).
 */

  if (tp != &tty_table[0]) return;

  tp->tty_devread = cons_read;
  tp->tty_devwrite = console;
  tp->tty_echo = cons_echo;
  tp->tty_break = tty_devnop;

  /* A terminal of the usual size. */
  tp->tty_winsize.ws_row = 24;
  tp->tty_winsize.ws_col = 80;

  kb_init(tp);
}

/*===========================================================================*
 *				kb_init					     *
 *===========================================================================*/
PUBLIC void kb_init(tp)
tty_t *tp;
{
/* Start receive interrupts for the console. */

  duart_imr_set(ISR_RXRDYA);
}

/*===========================================================================*
 *		Hooks the shared code calls into the ST console driver	     *
 *===========================================================================*/
PUBLIC void flush(tp)
tty_t *tp;
{
/* Output is not buffered. */
}

PUBLIC void func_key()
{
/* There are no function keys on a serial console. */
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
