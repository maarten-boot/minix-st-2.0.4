/*
 * rosco build: stand-in for src/kernel/staddr.h.
 *
 * clock.c includes "staddr.h" and "stmfp.h" for init_clock(), which
 * programs timer C of the Atari's MFP.  The rosco Makefile compiles a copy
 * of clock.c in this directory, so these two files are found instead of
 * the ST ones.  The rosco clock is the DUART counter/timer, started by
 * initrosco() in roscmain.c.
 */
