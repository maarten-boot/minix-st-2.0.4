/*
 * rosco build: stand-in for src/kernel/stmfp.h (see staddr.h here).
 *
 * init_clock() in clock.c writes the timer C data and control registers
 * and loops until the data register reads back the value written.  On the
 * rosco there is no MFP, so MFP points to a dummy in RAM (roscstub.c):
 * the loop ends at once and nothing touches the I/O space.
 */

#ifndef ROSCO_STMFP_H
#define ROSCO_STMFP_H

struct mfp {
  unsigned char mf_tcdr;		/* "timer data register C" */
  unsigned char mf_tcdcr;		/* "timer control register C+D" */
};

extern struct mfp rosco_dummy_mfp;

#define	MFP		(&rosco_dummy_mfp)
#define	T_Q064		0

#endif /* ROSCO_STMFP_H */
