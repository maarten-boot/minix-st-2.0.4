/*
 * rosco_m68k Classic v2 hardware:
 * Exar XR68C681 Dual Asynchronous Receiver/Transmitter
 *
 * The DUART sits on the low data byte (odd addresses) at 0xF00001, with
 * register n at 0xF00001 + 2*n.  It interrupts on IPL 4 and supplies its
 * own vector from IVR.  Crystal 3.6864 MHz.
 *
 * Register layout as used by the rosco firmware 2.4 (rosco_m68k_public.asm,
 * duart.asm) and the MC68681 family data; the pins are checked against the
 * r2 schematic, the baud rates (rate set 1) on the board.
 */

#define	DUART	((struct duart *)AD_DUART)

struct duart {
	_VOLATILE unsigned char	dub[32];	/* use only the odd bytes */
};

/* Channel A */
#define	du_mra		dub[ 1]	/* RW: MR1A/MR2A (pointer auto-advances) */
#define	du_sra		dub[ 3]	/* R:  status register A */
#define	du_csra		dub[ 3]	/* W:  clock select register A */
#define	du_misr		dub[ 5]	/* R:  masked interrupt status (XR only) */
#define	du_cra		dub[ 5]	/* W:  command register A */
#define	du_rhra		dub[ 7]	/* R:  receive holding register A */
#define	du_thra		dub[ 7]	/* W:  transmit holding register A */
/* Common */
#define	du_ipcr		dub[ 9]	/* R:  input port change register */
#define	du_acr		dub[ 9]	/* W:  auxiliary control register */
#define	du_isr		dub[11]	/* R:  interrupt status register */
#define	du_imr		dub[11]	/* W:  interrupt mask register (write only!) */
#define	du_ctu		dub[13]	/* R:  counter/timer upper */
#define	du_ctur		dub[13]	/* W:  counter/timer upper preset */
#define	du_ctl		dub[15]	/* R:  counter/timer lower */
#define	du_ctlr		dub[15]	/* W:  counter/timer lower preset */
/* Channel B */
#define	du_mrb		dub[17]	/* RW: MR1B/MR2B */
#define	du_srb		dub[19]	/* R:  status register B */
#define	du_csrb		dub[19]	/* W:  clock select register B */
#define	du_crb		dub[21]	/* W:  command register B */
#define	du_rhrb		dub[23]	/* R:  receive holding register B */
#define	du_thrb		dub[23]	/* W:  transmit holding register B */
/* Common */
#define	du_ivr		dub[25]	/* RW: interrupt vector register */
#define	du_ip		dub[27]	/* R:  input port */
#define	du_opcr		dub[27]	/* W:  output port configuration */
#define	du_startcc	dub[29]	/* R:  start counter command */
#define	du_opset	dub[29]	/* W:  set output port bits (pin goes LOW) */
#define	du_stopcc	dub[31]	/* R:  stop counter command; in timer mode
				 *     this only clears ISR_CTR */
#define	du_opclr	dub[31]	/* W:  reset output port bits (pin goes HIGH) */

/* bits in SRA/SRB: */
#define	SR_RXRDY	0x01	/* receiver ready */
#define	SR_FFULL	0x02	/* receive FIFO full */
#define	SR_TXRDY	0x04	/* transmitter ready */
#define	SR_TXEMT	0x08	/* transmitter empty */
#define	SR_OE		0x10	/* overrun error */
#define	SR_PE		0x20	/* parity error */
#define	SR_FE		0x40	/* framing error */
#define	SR_RB		0x80	/* received break */
#define	SR_ERRS		(SR_OE|SR_PE|SR_FE|SR_RB)

/* bits in ISR/IMR: */
#define	ISR_TXRDYA	0x01	/* channel A transmitter ready */
#define	ISR_RXRDYA	0x02	/* channel A receiver ready / FIFO full */
#define	ISR_BRKA	0x04	/* channel A change in break */
#define	ISR_CTR		0x08	/* counter/timer ready */
#define	ISR_TXRDYB	0x10	/* channel B transmitter ready */
#define	ISR_RXRDYB	0x20	/* channel B receiver ready / FIFO full */
#define	ISR_BRKB	0x40	/* channel B change in break */
#define	ISR_IPC		0x80	/* input port change */

/* command register (CRA/CRB) values: low nibble enables/disables,
 * high nibble is a miscellaneous command.  Write one command at a time.
 */
#define	CR_RXEN		0x01	/* enable receiver */
#define	CR_RXDIS	0x02	/* disable receiver */
#define	CR_TXEN		0x04	/* enable transmitter */
#define	CR_TXDIS	0x08	/* disable transmitter */
#define	CR_RSTMR	0x10	/* reset MR pointer to MR1 */
#define	CR_RSTRX	0x20	/* reset receiver */
#define	CR_RSTTX	0x30	/* reset transmitter */
#define	CR_RSTERR	0x40	/* reset error status */
#define	CR_RSTBRK	0x50	/* reset break change interrupt */
#define	CR_STARTBRK	0x60	/* start break */
#define	CR_STOPBRK	0x70	/* stop break */
#define	CR_XRXEXT	0x80	/* XR: set receiver extended baud rate bit */
#define	CR_XRXCLR	0x90	/* XR: clear receiver extended baud rate bit */
#define	CR_XTXEXT	0xA0	/* XR: set transmitter extended baud rate bit */
#define	CR_XTXCLR	0xB0	/* XR: clear transmitter extended baud rate bit */

/* bits in MR1: */
#define	MR1_RXRTS	0x80	/* receiver controls RTS */
#define	MR1_FFULL	0x40	/* RxRDY interrupt on FIFO full only */
#define	MR1_PAR		0x00	/* with parity */
#define	MR1_NOPAR	0x10	/* no parity */
#define	MR1_ODD		0x04	/* odd parity (with MR1_PAR) */
#define	MR1_CS5		0x00	/* 5 data bits */
#define	MR1_CS6		0x01	/* 6 data bits */
#define	MR1_CS7		0x02	/* 7 data bits */
#define	MR1_CS8		0x03	/* 8 data bits */

/* bits in MR2: */
#define	MR2_NORMAL	0x00	/* normal channel mode */
#define	MR2_TXRTS	0x20	/* transmitter controls RTS */
#define	MR2_TXCTS	0x10	/* CTS enables transmitter */
#define	MR2_STOP1	0x07	/* 1 stop bit (6-8 data bits) */
#define	MR2_STOP2	0x0F	/* 2 stop bits (6-8 data bits) */

/* bits in ACR: */
#define	ACR_BRG1	0x00	/* baud rate generator set 1 (both channels!) */
#define	ACR_BRG2	0x80	/* baud rate generator set 2 (both channels!) */
#define	ACR_TIMER16	0x70	/* counter/timer: timer mode, X1/X2 clock / 16 */
#define	ACR_IP0		0x01	/* interrupt on change of IP0 (CTS A) */
#define	ACR_IP1		0x02	/* interrupt on change of IP1 (CTS B) */

/* bits in IPCR: */
#define	IPCR_IP0	0x01	/* current state of IP0 */
#define	IPCR_IP1	0x02	/* current state of IP1 */
#define	IPCR_DIP0	0x10	/* IP0 changed */
#define	IPCR_DIP1	0x20	/* IP1 changed */

/* Clock select values (RX in high nibble, TX in low nibble): see the
 * speed table in roscrs232.c.  The firmware uses ACR_BRG2 with the XR
 * extended bits set (CR_XRXEXT, CR_XTXEXT) and 0x88 for 115200.
 */

/* Counter/timer: in timer mode with X1/16 the output toggles every
 * preset counts, so the interrupt rate is 3686400/16/(2*preset).
 */
#define	DUART_XTAL	3686400L
#define	DUART_TIMER(hz)	((unsigned) (DUART_XTAL / 16 / 2 / (hz)))

/* Output port bits (OP pins are active low: du_opset drives them low).
 * RTS A/B on OP0/OP1, CTS A/B on IP0/IP1 and the SPI pins are checked
 * against the r2 schematic (design/r2/kicad/DUART.kicad_sch); CTS has no
 * pull resistor there.  The LEDs are as used by the firmware.
 */
#define	OP_RTSA		0x01	/* OP0: RTS channel A */
#define	OP_RTSB		0x02	/* OP1: RTS channel B */
#define	OP_SPI_CS	0x04	/* OP2: SD card chip select   - do not touch */
#define	OP_LED_RED	0x08	/* OP3: red LED */
#define	OP_SPI_SCK	0x10	/* OP4: SD card clock         - do not touch */
#define	OP_LED_GREEN	0x20	/* OP5: green (heartbeat) LED */
#define	OP_SPI_COPI	0x40	/* OP6: SD card data out      - do not touch */
#define	OP_SPI_CS1	0x80	/* OP7: second SPI select     - do not touch */

/* Input port bits */
#define	IP_CTSA		0x01	/* IP0: CTS channel A */
#define	IP_CTSB		0x02	/* IP1: CTS channel B */
#define	IP_SPI_CIPO	0x04	/* IP2: SD card data in       - do not touch */
