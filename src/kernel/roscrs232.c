/*==========================================================================*
 *	roscrs232.c - serial driver for the rosco_m68k XR68C681 DUART	    *
 *==========================================================================*/

/* This is rs232.c with the 8250 and MFP hardware replaced by the two
 * channels of the XR68C681 DUART.  The buffering, flow control and tty
 * interface are those of rs232.c.  It drives
 *
 *	line 0: channel A, the console (tty_table[0], see roscons.c)
 *	line 1: channel B, /dev/tty00 (tty_table[NR_CONS])
 *
 * Differences from rs232.c:
 *  - One interrupt for everything: duartint() (roscmain.c) calls
 *    rs_duart_int() with the active ISR bits.
 *  - TxRDY is level triggered, so the TxRDY interrupt of a channel is only
 *    enabled while there is output to do and it may be sent.
 *  - CTS is handled by the DUART itself (MR2 "CTS enables transmitter")
 *    unless CLOCAL is set; there is no DCD, so no hangup detection.
 *    RTS (input flow control) is an output port bit.  The console never
 *    waits for CTS: on the board CTS has no pull resistor, and a console
 *    that hangs without a cable handshake is no use.
 *  - The console's speed is fixed at ROSCO_CONS_BAUD (tty.c resets the
 *    termios on the last close, which would set 9600), see rs_cons_setup.
 *  - Received characters with errors are counted and dropped; a received
 *    break is dropped too.
 *
 * The entries into this file are:
 *   rs_init:		initialize channel B (from tty.c)
 *   rs_cons_setup:	early polled setup of channel A (from initrosco)
 *   rs_cons_init:	initialize channel A (from scr_init in roscons.c)
 *   rs_duart_int:	DUART receive/transmit interrupts
 *   rs_test_line:	send a test line (debug key PF4, roscons.c)
 */

#include "kernel.h"
#include <termios.h>
#include <signal.h>
#include "tty.h"
#include "proc.h"

#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include "roscaddr.h"
#include "roscduart.h"
#include "rosco.h"

#if (NR_RS_LINES > 1)
#error				/* the DUART has one channel for rs232 */
#endif

#define RS_IBUFSIZE	1024	/* input buffer size: a SLIP frame (MTU 296,
				 * up to twice that escaped) arrives faster
				 * than TTY drains 256 bytes at 115200 */
#define RS_OBUFSIZE	256	/* output buffer size */

/* Input buffer watermarks.
 * The external device is asked to stop sending when the buffer
 * exactly reaches high water, or when TTY requests it.  Sending restarts
 * when the input buffer empties below the low watermark.
 */
#define RS_ILOWWATER	(1 * RS_IBUFSIZE / 4)
#define RS_IHIGHWATER	(3 * RS_IBUFSIZE / 4)

/* Output buffer low watermark.
 * TTY is notified when the output buffer empties below the low watermark, so
 * it may continue filling the buffer if doing a large write.
 */
#define RS_OLOWWATER	(1 * RS_OBUFSIZE / 4)

/* Registers of one channel, as offsets in struct duart (channel B is 16
 * bytes above channel A).
 */
#define	CH_A		0
#define	CH_B		16
#define	du_mr(rs)	DUART->dub[(rs)->ch + 1]
#define	du_sr(rs)	DUART->dub[(rs)->ch + 3]
#define	du_csr(rs)	DUART->dub[(rs)->ch + 3]
#define	du_cr(rs)	DUART->dub[(rs)->ch + 5]
#define	du_rhr(rs)	DUART->dub[(rs)->ch + 7]
#define	du_thr(rs)	DUART->dub[(rs)->ch + 7]

/* Flow control.  RTS is active low on the output port: setting the OP bit
 * asserts it.  istart() tells external device we are ready, istop() that we
 * are not.
 */
#define istart(rs)	(DUART->du_opset = (rs)->op_rts, (rs)->idevready = TRUE)
#define istop(rs)	(DUART->du_opclr = (rs)->op_rts, (rs)->idevready = FALSE)

/* Transmitter ready test */
#define txready(rs)	(du_sr(rs) & SR_TXRDY)

/* Types. */
typedef unsigned char bool_t;	/* boolean */

/* RS232 device structure, one per device. */
typedef struct rs232 {
  tty_t *tty;			/* associated TTY structure */

  int icount;			/* number of bytes in the input buffer */
  char *ihead;			/* next free spot in input buffer */
  char *itail;			/* first byte to give to TTY */
  bool_t idevready;		/* nonzero if we are ready to receive (RTS) */

  unsigned char ostate;		/* combination of flags: */
#define ODONE          1	/* output completed (< output enable bits) */
#define ORAW           2	/* raw mode for xoff disable (< enab. bits) */
#define ODEVREADY   0x10	/* external device hardware ready (CTS) */
#define OQUEUED     0x20	/* output buffer not empty */
#define OSWREADY    0x40	/* external device software ready (no xoff) */
  unsigned char oxoff;		/* char to stop output */
  bool_t inhibited;		/* output inhibited? (follows tty_inhibited) */
  bool_t drain;			/* if set drain output and reconfigure line */
  bool_t console;		/* the console line */
  int ocount;			/* number of bytes in the output buffer */
  char *ohead;			/* next free spot in output buffer */
  char *otail;			/* next char to output */

  int ch;			/* CH_A or CH_B */
  unsigned char isr_rx;		/* the channel's ISR/IMR bits */
  unsigned char isr_tx;
  unsigned char op_rts;		/* output port bit for RTS */
  unsigned char cts_mr2;	/* MR2_TXCTS if CTS gates the transmitter */
  unsigned char mr1, mr2;	/* mode registers as last set (0: not yet) */
  struct speed2csr *speed;	/* speed as last set (NULL: not yet) */

  unsigned framing_errors;	/* error counts (no reporting yet) */
  unsigned overrun_errors;
  unsigned parity_errors;
  unsigned break_interrupts;
  char ibuf[RS_IBUFSIZE];	/* input buffer */
  char obuf[RS_OBUFSIZE];	/* output buffer */
} rs232_t;

PRIVATE rs232_t rs_lines[1 + NR_RS_LINES];

/* Speeds: clock select code and whether the XR "extended" bit is needed,
 * with ACR[7] = 0 (baud rate set 1) for both channels.  38400 is then the
 * plain MC68681 code 0xC.  Set 2 with the extended bit (code 0xC) should
 * also give 38400, but on the real board it gave 19200.  115200 is code
 * 0x8 with the extended bit in both sets (the firmware uses set 2).
 * From the XR68C681 rate table as in MAME's mc68681.cpp.
 */
PRIVATE struct speed2csr {
  speed_t	speed;
  long		baud;		/* the same as a number */
  unsigned char	csr;		/* code for RX and TX (same nibble) */
  unsigned char	ext;		/* 1: extended rate */
} s2c[] = {
  { B50,	50,	0x0, 0 },
  { B75,	75,	0x0, 1 },
  { B110,	110,	0x1, 0 },
  { B134,	134,	0x2, 0 },
  { B150,	150,	0x3, 1 },
  { B200,	200,	0x3, 0 },
  { B300,	300,	0x4, 0 },
  { B600,	600,	0x5, 0 },
  { B1200,	1200,	0x6, 0 },
  { B1800,	1800,	0xA, 1 },
  { B2400,	2400,	0x8, 0 },
  { B4800,	4800,	0x9, 0 },
  { B9600,	9600,	0xB, 0 },
  { B19200,	19200,	0xC, 1 },
  { B38400,	38400,	0xC, 0 },
  { B57600,	57600,	0x7, 1 },
  { B115200,	115200,	0x8, 1 },
};
#define	S2C_END	(s2c + sizeof(s2c)/sizeof(s2c[0]))

PRIVATE struct speed2csr *cons_speed;	/* entry for ROSCO_CONS_BAUD */

FORWARD _PROTOTYPE( void rs_line_init, (tty_t *tp, rs232_t *rs)		);
FORWARD _PROTOTYPE( void set_speed, (rs232_t *rs, struct speed2csr *sp)	);
FORWARD _PROTOTYPE( void in_int, (rs232_t *rs)				);
FORWARD _PROTOTYPE( void out_int, (rs232_t *rs)				);
FORWARD _PROTOTYPE( void rs_write, (tty_t *tp)				);
FORWARD _PROTOTYPE( void rs_echo, (tty_t *tp, int c)			);
FORWARD _PROTOTYPE( void rs_ioctl, (tty_t *tp)				);
FORWARD _PROTOTYPE( void rs_config, (rs232_t *rs)			);
FORWARD _PROTOTYPE( void rs_read, (tty_t *tp)				);
FORWARD _PROTOTYPE( void rs_icancel, (tty_t *tp)			);
FORWARD _PROTOTYPE( void rs_ocancel, (tty_t *tp)			);
FORWARD _PROTOTYPE( void rs_ostart, (rs232_t *rs)			);
FORWARD _PROTOTYPE( void rs_break, (tty_t *tp)				);
FORWARD _PROTOTYPE( void rs_close, (tty_t *tp)				);


/*==========================================================================*
 *				rs_write				    *
 *==========================================================================*/
PRIVATE void rs_write(tp)
register tty_t *tp;
{
/* (*devwrite)() routine for RS232. */

  rs232_t *rs = tp->tty_priv;
  int count, ocount;
  phys_bytes user_phys;

  if (rs->inhibited != tp->tty_inhibited) {
	/* Inhibition state has changed. */
	lock();
	rs->ostate |= OSWREADY;
	if (tp->tty_inhibited) rs->ostate &= ~OSWREADY;
	rs_ostart(rs);
	unlock();
	rs->inhibited = tp->tty_inhibited;
  }

  if (rs->drain) {
	/* Wait for the line to drain then reconfigure and continue output. */
	if (rs->ocount > 0) return;
	rs->drain = FALSE;
	rs_config(rs);
  }

  /* While there is something to do. */
  for (;;) {
	ocount = buflen(rs->obuf) - rs->ocount;
	count = bufend(rs->obuf) - rs->ohead;
	if (count > ocount) count = ocount;
	if (count > tp->tty_outleft) count = tp->tty_outleft;
	if (count == 0 || tp->tty_inhibited) break;

	/* Copy from user space to the RS232 output buffer. */
	user_phys = proc_vir2phys(proc_addr(tp->tty_outproc), tp->tty_out_vir);
	phys_copy(user_phys, vir2phys(rs->ohead), (phys_bytes) count);

	/* Perform output processing on the output buffer. */
	out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count, &ocount);
	if (count == 0) break;

	/* Assume echoing messed up by output. */
	tp->tty_reprint = TRUE;

	/* Bookkeeping. */
	lock();			/* protect interrupt sensitive rs->ocount */
	rs->ocount += ocount;
	rs_ostart(rs);
	unlock();
	if ((rs->ohead += ocount) >= bufend(rs->obuf))
		rs->ohead -= buflen(rs->obuf);
	tp->tty_out_vir += count;
	tp->tty_outcum += count;
	if ((tp->tty_outleft -= count) == 0) {
		/* Output is finished, reply to the writer. */
		tty_reply(tp->tty_outrepcode, tp->tty_outcaller,
					tp->tty_outproc, tp->tty_outcum);
		tp->tty_outcum = 0;
	}
  }
  if (tp->tty_outleft > 0 && tp->tty_termios.c_ospeed == B0) {
	/* Oops, the line has hung up. */
	tty_reply(tp->tty_outrepcode, tp->tty_outcaller, tp->tty_outproc, EIO);
	tp->tty_outleft = tp->tty_outcum = 0;
  }
}


/*==========================================================================*
 *				rs_echo					    *
 *==========================================================================*/
PRIVATE void rs_echo(tp, c)
tty_t *tp;			/* which TTY */
int c;				/* character to echo */
{
/* Echo one character.  (Like rs_write, but only one character, optionally.) */

  rs232_t *rs = tp->tty_priv;
  int count, ocount;

  ocount = buflen(rs->obuf) - rs->ocount;
  if (ocount == 0) return;		/* output buffer full */
  count = 1;
  *rs->ohead = c;			/* add one character */

  out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count, &ocount);
  if (count == 0) return;

  lock();
  rs->ocount += ocount;
  rs_ostart(rs);
  unlock();
  if ((rs->ohead += ocount) >= bufend(rs->obuf)) rs->ohead -= buflen(rs->obuf);
}


/*==========================================================================*
 *				rs_ioctl				    *
 *==========================================================================*/
PRIVATE void rs_ioctl(tp)
tty_t *tp;			/* which TTY */
{
/* Reconfigure the line as soon as the output has drained. */
  rs232_t *rs = tp->tty_priv;

  rs->drain = TRUE;
}


/*==========================================================================*
 *				rs_config				    *
 *==========================================================================*/
PRIVATE void rs_config(rs)
rs232_t *rs;			/* which line */
{
/* Set various line control parameters for RS232 I/O.  The DUART can't do
 * split speed per channel either, so the output speed is used.
 */

  tty_t *tp = rs->tty;
  struct speed2csr *s2cp, *sp;
  int mr1, mr2;

  if (rs->console) {
	/* The console keeps its speed. */
	tp->tty_termios.c_ispeed = tp->tty_termios.c_ospeed = cons_speed->speed;
  }

  /* RS232 needs to know the xoff character, and if CTS works. */
  rs->oxoff = tp->tty_termios.c_cc[VSTOP];
  rs->cts_mr2 = (rs->console || (tp->tty_termios.c_cflag & CLOCAL))
							? 0 : MR2_TXCTS;

  /* Look up the clock select code from the output speed. */
  sp = NULL;
  for (s2cp = s2c; s2cp < S2C_END; s2cp++) {
	if (s2cp->speed == tp->tty_termios.c_ospeed) sp = s2cp;
  }
  if (sp == NULL) {
	/* B0: hang up, drop RTS (there is no DTR). */
	if (!rs->console) {
		lock();
		istop(rs);
		unlock();
	}
	return;
  }

  /* Compute the mode registers. */
  mr1 = MR1_NOPAR;
  if (tp->tty_termios.c_cflag & PARENB) {
	mr1 = MR1_PAR;
	if (tp->tty_termios.c_cflag & PARODD) mr1 |= MR1_ODD;
  }
  switch (tp->tty_termios.c_cflag & CSIZE) {
	case CS5:	mr1 |= MR1_CS5; break;
	case CS6:	mr1 |= MR1_CS6; break;
	case CS7:	mr1 |= MR1_CS7; break;
	case CS8:	mr1 |= MR1_CS8; break;
  }
  mr2 = MR2_NORMAL | rs->cts_mr2;
  mr2 |= (tp->tty_termios.c_cflag & CSTOPB) || sp->speed == B110
					? MR2_STOP2 : MR2_STOP1;

  /* Program the DUART only if something changes (for the console nothing
   * ever does, e.g. on the termios reset at the last close), and not in
   * the middle of a character: the output buffer is empty, but the
   * transmitter may still be sending.  Bounded, in case CTS holds it.
   */
  if (mr1 != rs->mr1 || mr2 != rs->mr2 || sp != rs->speed) {
	unsigned wait = 0xFFFF;

	while ((du_sr(rs) & SR_TXEMT) == 0 && --wait != 0)
		;
  }
  lock();
  if (mr1 != rs->mr1 || mr2 != rs->mr2 || sp != rs->speed) {
	/* Reprogram with receiver and transmitter disabled: the real
	 * XR68C681 may not take a new mode or rate on a running channel
	 * (tty00 changed by stty gave no output at 38400 on the board).
	 */
	du_cr(rs) = CR_RXDIS | CR_TXDIS;
	if (mr1 != rs->mr1 || mr2 != rs->mr2) {
		du_cr(rs) = CR_RSTMR;
		du_mr(rs) = rs->mr1 = mr1;
		du_mr(rs) = rs->mr2 = mr2;
	}
	/* The full reset as at boot (rs_line_init): after a rate change on
	 * the board the channel stopped sending, also when set back.
	 */
	du_cr(rs) = CR_RSTRX;
	du_cr(rs) = CR_RSTTX;
	du_cr(rs) = CR_RSTERR;
	du_cr(rs) = CR_RSTBRK;
	if (sp != rs->speed) {
		set_speed(rs, sp);
		rs->speed = sp;
	}
	du_cr(rs) = CR_RXEN | CR_TXEN;
  }

  /* Not hung up: assert RTS again.  rs_close() drops it with HUPCL, and
   * nothing else raised it for the next open of the line, so tty00 sent
   * nothing after the first getty (the PC driver raises DTR here too).
   */
  if (!rs->console) istart(rs);

  /* CTS is checked by the DUART, so it is always "ready" here. */
  rs->ostate |= ODEVREADY | ORAW;
  if ((tp->tty_termios.c_lflag & IXON) && rs->oxoff != _POSIX_VDISABLE)
	rs->ostate &= ~ORAW;
  unlock();
}


/*==========================================================================*
 *				set_speed				    *
 *==========================================================================*/
PRIVATE void set_speed(rs, sp)
rs232_t *rs;
struct speed2csr *sp;
{
/* Select a speed for receiver and transmitter: the XR extended rate bits,
 * then the clock select register.  ACR[7] (rate set 2) is always off.
 */
  du_cr(rs) = sp->ext ? CR_XRXEXT : CR_XRXCLR;
  du_cr(rs) = sp->ext ? CR_XTXEXT : CR_XTXCLR;
  du_csr(rs) = (sp->csr << 4) | sp->csr;
}


/*==========================================================================*
 *				rs_cons_setup				    *
 *==========================================================================*/
PUBLIC void rs_cons_setup()
{
/* Set up channel A for the console before anything else: 8N1 at
 * ROSCO_CONS_BAUD, RTS asserted.  Polled output (putk) works after this.
 * Called from initrosco() with interrupts off; ACR is already written.
 */
  static rs232_t early;		/* just enough of a line for the macros */
  register rs232_t *rs = &early;

  for (cons_speed = s2c; cons_speed < S2C_END; cons_speed++) {
	if (cons_speed->baud == ROSCO_CONS_BAUD) break;
  }
  if (cons_speed == S2C_END) cons_speed = &s2c[12];	/* 9600, see table */

  rs->ch = CH_A;
  rs->op_rts = OP_RTSA;
  while ((du_sr(rs) & SR_TXEMT) == 0)	/* let the loader's output finish */
	;
  du_cr(rs) = CR_RSTERR;
  du_cr(rs) = CR_RSTMR;
  du_mr(rs) = MR1_NOPAR | MR1_CS8;
  du_mr(rs) = MR2_NORMAL | MR2_STOP1;
  set_speed(rs, cons_speed);
  istart(rs);
  du_cr(rs) = CR_RXEN | CR_TXEN;
}


/*==========================================================================*
 *				rs_init					    *
 *==========================================================================*/
PUBLIC void rs_init(tp)
tty_t *tp;			/* which TTY */
{
/* Initialize RS232 for one line: /dev/tty00 is DUART channel B. */

  register rs232_t *rs = &rs_lines[1];

  rs->ch = CH_B;
  rs->isr_rx = ISR_RXRDYB;
  rs->isr_tx = ISR_TXRDYB;
  rs->op_rts = OP_RTSB;
  rs_line_init(tp, rs);
}


/*==========================================================================*
 *				rs_cons_init				    *
 *==========================================================================*/
PUBLIC void rs_cons_init(tp)
tty_t *tp;			/* the console TTY */
{
/* Initialize the console line: DUART channel A. */

  register rs232_t *rs = &rs_lines[0];

  rs->ch = CH_A;
  rs->isr_rx = ISR_RXRDYA;
  rs->isr_tx = ISR_TXRDYA;
  rs->op_rts = OP_RTSA;
  rs->console = TRUE;
  tp->tty_winsize.ws_row = 24;		/* a terminal of the usual size */
  tp->tty_winsize.ws_col = 80;
  rs_line_init(tp, rs);
}


/*==========================================================================*
 *				rs_line_init				    *
 *==========================================================================*/
PRIVATE void rs_line_init(tp, rs)
tty_t *tp;
register rs232_t *rs;
{
/* Common part of the line initialization. */

  /* Associate RS232 and TTY structures. */
  tp->tty_priv = rs;
  rs->tty = tp;

  /* Set up input queue. */
  rs->ihead = rs->itail = rs->ibuf;

  /* Set up the hardware to a base state: no input wanted yet, channel
   * interrupts off, line configured, errors cleared.  The console keeps
   * transmitting, the loader and the kernel have been printing on it.
   */
  istop(rs);
  duart_imr_clr(rs->isr_rx | rs->isr_tx);
  if (rs->console) {
	while ((du_sr(rs) & SR_TXEMT) == 0)	/* let the output finish */
		;
  }
  rs_config(rs);
  du_cr(rs) = CR_RSTERR;
  du_cr(rs) = CR_RSTBRK;
  while (du_sr(rs) & SR_RXRDY) (void) du_rhr(rs);	/* empty the FIFO */
  du_cr(rs) = CR_RXEN | CR_TXEN;

  rs->ostate |= OSWREADY;		/* rs_config set ODEVREADY, ORAW */
  rs->ohead = rs->otail = rs->obuf;

  /* Fill in TTY function hooks. */
  tp->tty_devread = rs_read;
  tp->tty_devwrite = rs_write;
  tp->tty_echo = rs_echo;
  tp->tty_icancel = rs_icancel;
  tp->tty_ocancel = rs_ocancel;
  tp->tty_ioctl = rs_ioctl;
  tp->tty_break = rs_break;
  tp->tty_close = rs_close;

  /* Tell external device we are ready, and take input. */
  istart(rs);
  duart_imr_set(rs->isr_rx);
}


/*==========================================================================*
 *				rs_icancel				    *
 *==========================================================================*/
PRIVATE void rs_icancel(tp)
tty_t *tp;			/* which TTY */
{
/* Cancel waiting input. */
  rs232_t *rs = tp->tty_priv;

  lock();
  rs->icount = 0;
  rs->itail = rs->ihead;
  istart(rs);
  unlock();
}


/*==========================================================================*
 *				rs_ocancel				    *
 *==========================================================================*/
PRIVATE void rs_ocancel(tp)
tty_t *tp;			/* which TTY */
{
/* Cancel pending output. */
  rs232_t *rs = tp->tty_priv;

  lock();
  rs->ostate &= ~(ODONE | OQUEUED);
  rs->ocount = 0;
  rs->otail = rs->ohead;
  duart_imr_clr(rs->isr_tx);
  unlock();
}


/*==========================================================================*
 *				rs_read					    *
 *==========================================================================*/
PRIVATE void rs_read(tp)
tty_t *tp;			/* which tty */
{
/* Process characters from the circular input buffer.  On the console the
 * debug keys (roscons.c) are taken out first, and a lone ESC that they held
 * back is passed on when it has timed out.
 */

  rs232_t *rs = tp->tty_priv;
  int icount, count;

  while ((count = rs->icount) > 0) {
	icount = bufend(rs->ibuf) - rs->itail;
	if (count > icount) count = icount;

	/* Perform input processing on (part of) the input buffer. */
	if (rs->console) {
		count = 1;
		if (!cons_debug_key(tp, *rs->itail & 0xFF)
				&& in_process(tp, rs->itail, 1) == 0)
			break;
	} else {
		if ((count = in_process(tp, rs->itail, count)) == 0) break;
	}

	lock();			/* protect interrupt sensitive variables */
	rs->icount -= count;
	if (!rs->idevready && rs->icount < RS_ILOWWATER) istart(rs);
	unlock();
	if ((rs->itail += count) == bufend(rs->ibuf)) rs->itail = rs->ibuf;
  }
  if (rs->console) cons_esc_timeout(tp);
}


/*==========================================================================*
 *				rs_ostart				    *
 *==========================================================================*/
PRIVATE void rs_ostart(rs)
rs232_t *rs;			/* which rs line */
{
/* Tell RS232 there is something waiting in the output buffer.  Interrupts
 * are off.  The TxRDY interrupt does the rest.
 */

  if (rs->ocount > 0) rs->ostate |= OQUEUED;
  if (rs->ostate >= (ODEVREADY | OQUEUED | OSWREADY))
	duart_imr_set(rs->isr_tx);
}


/*==========================================================================*
 *				rs_break				    *
 *==========================================================================*/
PRIVATE void rs_break(tp)
tty_t *tp;			/* which tty */
{
/* Generate a break condition for 0.4 sec.  Called from the TTY task, so
 * the clock runs meanwhile.
 */
  rs232_t *rs = tp->tty_priv;
  clock_t end;

  du_cr(rs) = CR_STARTBRK;
  end = get_uptime() + (HZ * 4 + 9) / 10;
  while (get_uptime() < end)
	;
  du_cr(rs) = CR_STOPBRK;
}


/*==========================================================================*
 *				rs_close				    *
 *==========================================================================*/
PRIVATE void rs_close(tp)
tty_t *tp;			/* which tty */
{
/* The line is closed; optionally hang up (drop RTS, there is no DTR). */
  rs232_t *rs = tp->tty_priv;

  if (!rs->console && (tp->tty_termios.c_cflag & HUPCL)) {
	lock();
	istop(rs);
	unlock();
  }
}


/*==========================================================================*
 *				rs_test_line				    *
 *==========================================================================*/
PUBLIC void rs_test_line(tp)
tty_t *tp;			/* which tty */
{
/* Queue a test line for output on a line, through the normal output path
 * (output processing, buffer, TxRDY interrupt).  For bring-up, when no
 * process can open the line yet.
 */
  static char msg[] = "rosco tty00 test: the quick brown fox 0123456789\n";
  char *p;

  if (tp->tty_priv == NULL) return;
  for (p = msg; *p != 0; p++) rs_echo(tp, *p);
}


/* Low level (interrupt) routines. */

/*==========================================================================*
 *				rs_duart_int				    *
 *==========================================================================*/
PUBLIC void rs_duart_int(isr)
int isr;			/* active and enabled ISR bits */
{
/* Receive and transmit interrupts of both channels.  Called from duartint()
 * with interrupts off.
 */
  register rs232_t *rs;

  for (rs = rs_lines; rs < bufend(rs_lines); rs++) {
	if (rs->tty == NULL) continue;
	if (isr & rs->isr_rx) in_int(rs);
	if (isr & rs->isr_tx) out_int(rs);
  }
}


/*==========================================================================*
 *				in_int					    *
 *==========================================================================*/
PRIVATE void in_int(rs)
register rs232_t *rs;		/* line with input interrupt */
{
/* Read the data which just arrived, the whole receive FIFO.
 * If it is the oxoff char, clear OSWREADY, else if OSWREADY was clear, set
 * it and restart output (any char does this, not just xon).
 * Put data in the buffer if room, otherwise discard it.
 * Set a flag for the clock interrupt handler to eventually notify TTY.
 */

  int c, sr;

  while ((sr = du_sr(rs)) & SR_RXRDY) {
	c = du_rhr(rs);

	if (sr & SR_ERRS) {
		/* Record the error, drop the character. */
		if (sr & SR_FE) ++rs->framing_errors;
		if (sr & SR_OE) ++rs->overrun_errors;
		if (sr & SR_PE) ++rs->parity_errors;
		if (sr & SR_RB) {
			++rs->break_interrupts;
			du_cr(rs) = CR_RSTBRK;
		}
		du_cr(rs) = CR_RSTERR;
		continue;
	}

	if (!(rs->ostate & ORAW)) {
		if (c == rs->oxoff) {
			rs->ostate &= ~OSWREADY;
			duart_imr_clr(rs->isr_tx);
		} else
		if (!(rs->ostate & OSWREADY)) {
			rs->ostate |= OSWREADY;
			rs_ostart(rs);
		}
	}

	if (rs->icount == buflen(rs->ibuf)) continue;	/* full, discard */

	if (++rs->icount == RS_IHIGHWATER && rs->idevready) istop(rs);
	*rs->ihead = c;
	if (++rs->ihead == bufend(rs->ibuf)) rs->ihead = rs->ibuf;
	if (rs->icount == 1) {
		rs->tty->tty_events = 1;
		force_timeout();
	}
  }
}


/*==========================================================================*
 *				out_int					    *
 *==========================================================================*/
PRIVATE void out_int(rs)
register rs232_t *rs;		/* line with output interrupt */
{
/* The transmitter is ready.  If there is output to do and everything is
 * ready, do it; otherwise turn the TxRDY interrupt off, it would come back
 * at once.  Notify TTY when the buffer goes empty or runs low.
 */

  if (rs->ostate < (ODEVREADY | OQUEUED | OSWREADY)) {
	/* Bit test allows ORAW and requires the others. */
	duart_imr_clr(rs->isr_tx);
	return;
  }

  while (txready(rs)) {
	du_thr(rs) = *rs->otail;
	if (++rs->otail == bufend(rs->obuf)) rs->otail = rs->obuf;
	if (--rs->ocount == 0) {
		rs->ostate ^= (ODONE | OQUEUED);  /* ODONE on, OQUEUED off */
		duart_imr_clr(rs->isr_tx);
		rs->tty->tty_events = 1;
		force_timeout();
		break;
	} else
	if (rs->ocount == RS_OLOWWATER) {	/* running low? */
		rs->tty->tty_events = 1;
		force_timeout();
	}
  }
}

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
