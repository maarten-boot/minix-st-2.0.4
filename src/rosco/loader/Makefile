# Makefile for the rosco_m68k loader stub and the M1 test payload.
#
# Cross tools run in the roscopeco/rosco_m68k docker image through ../xrun.
# With a native m68k-elf toolchain on the PATH use: make XRUN=
#
#	make		build ROSCODE1.BIN (loader + M1 test payload)
#	make run	run it in r68k with the DUART model (../emu/build-r68k.sh)
#	make clean

XRUN	= ../xrun
CROSS	= $(XRUN) m68k-elf-
CC	= $(CROSS)gcc
LD	= $(CROSS)ld
OBJDUMP	= $(CROSS)objdump
ASFLAGS	= -mcpu=68010 -Wa,--register-prefix-optional
HOSTCC	= cc
R68K	= ../emu/r68k/r68k

all:	ROSCODE1.BIN

run:	ROSCODE1.BIN
	$(R68K) -d ROSCODE1.BIN

ROSCODE1.BIN:	mkrosco loader.bin hello.bin
	./mkrosco -o $@ loader.bin hello.bin

loader.bin:	loader.o loader.ld
	$(LD) -T loader.ld -o $@ loader.o

hello.bin:	hello.o payload.ld
	$(LD) -T payload.ld -o $@ hello.o

loader.o:	loader.S
	$(CC) $(ASFLAGS) -c loader.S

hello.o:	hello.S
	$(CC) $(ASFLAGS) -c hello.S

mkrosco:	mkrosco.c
	$(HOSTCC) -O -Wall -o $@ mkrosco.c

dis:	loader.o hello.o
	$(OBJDUMP) -d loader.o hello.o

clean:
	rm -f *.o *.bin mkrosco ROSCODE1.BIN
