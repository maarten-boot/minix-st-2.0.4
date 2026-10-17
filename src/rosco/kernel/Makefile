# Makefile for the Minix kernel on the rosco_m68k Classic v2.
#
# GNU build (m68k-elf-gcc -mshort) of the shared kernel sources in
# src/kernel with the rosco machine layer (src/kernel/rosc*) instead of the
# Atari ST one.  Objects and the kernel go into this directory.
# Cross tools run in the roscopeco/rosco_m68k docker image through ../xrun.
# With a native m68k-elf toolchain on the PATH use: make XRUN=
#
#	make		build kernel (ELF), kernel.bin (image linked at 0)
#			and ROSCODE1.BIN (loader + kernel, milestone M2)
#	make run	run ROSCODE1.BIN in r68k with the DUART model
#	make clean
#
# clock.c is compiled from a copy in this directory, so that its
# #include "staddr.h" / "stmfp.h" find the stand-ins here (see stmfp.h).

XRUN	= ../xrun
CROSS	= $(XRUN) m68k-elf-
CC	= $(CROSS)gcc
AR	= $(CROSS)ar
OBJCOPY	= $(CROSS)objcopy
NM	= $(CROSS)nm
HOSTCC	= cc

k	= ../../kernel
l	= ../../lib
i	= ../../../include
ri	= ../include
LOADER	= ../loader
R68K	= ../emu/r68k/r68k

CPU	= -mcpu=68010 -mshort
# what ACK predefines for the 68000 (gcc defines __GNUC__ and __mc68010__)
DEFS	= -D_EM_WSIZE=2 -D_EM_PSIZE=4 -D_EM_LSIZE=4 -D__m68000=1
INCS	= -I$(ri) -I$i -I$k
# console speed, e.g. make CONSBAUD=115200 (default in <minix/config.h>)
CONSBAUD =
DEFS	+= $(if $(CONSBAUD),-DROSCO_CONS_BAUD=$(CONSBAUD))
# rebuild what uses the console speed when CONSBAUD changes
$(shell echo '$(CONSBAUD)' | cmp -s - consbaud.stamp || echo '$(CONSBAUD)' > consbaud.stamp)
CFLAGS	= $(CPU) -O2 -std=gnu89 -ffreestanding -fno-builtin -fno-common \
	  -fno-strict-aliasing -fno-delete-null-pointer-checks \
	  -Wall -Wno-implicit-int -Wno-return-type -Wno-main \
	  -Wno-parentheses -Wno-unused -Wno-char-subscripts \
	  -Wno-pointer-sign -Wno-missing-braces \
	  $(DEFS) $(INCS)
ASFLAGS	= $(CPU) -Wa,--register-prefix-optional $(DEFS) $(INCS)

# roscompx.o must be first: see kernel.ld and genint in roscompx.S
SOBJ	= roscompx.o roscklib.o
ROBJ	= roscmain.o roscons.o roscrs232.o roscdisk.o roscstub.o
COBJ	= proc.o system.o stshadow.o tty.o clock.o memory.o table.o \
	  dmp.o misc.o pty.o driver.o drvlib.o
OBJ	= $(SOBJ) $(ROBJ) $(COBJ)

# library routines the kernel uses, compiled the same way
LOBJ	= printf.o _sigset.o u64.o m68kmul.o gccmem.o \
	  strcpy.o strcat.o strcmp.o strncmp.o strncpy.o strlen.o \
	  memcpy.o memmove.o memset.o memcmp.o

all:	kernel.bin ROSCODE1.BIN

run:	ROSCODE1.BIN
	$(R68K) -d ROSCODE1.BIN

kernel:	$(OBJ) libk.a kernel.ld
	$(CC) $(CPU) -nostdlib -T kernel.ld -Wl,-Map,kernel.map -o $@ \
		$(OBJ) libk.a

kernel.bin:	kernel
	$(OBJCOPY) -O binary $< $@

ROSCODE1.BIN:	kernel.bin $(LOADER)/loader.bin $(LOADER)/mkrosco
	$(LOADER)/mkrosco -o $@ $(LOADER)/loader.bin kernel.bin

$(LOADER)/loader.bin $(LOADER)/mkrosco:
	$(MAKE) -C $(LOADER) $(@F)

libk.a:	$(LOBJ)
	rm -f $@
	$(AR) rcs $@ $(LOBJ)

# kernel sources
roscompx.o:	$k/roscompx.S
	$(CC) $(ASFLAGS) -c $k/roscompx.S
roscklib.o:	$k/roscklib.S
	$(CC) $(ASFLAGS) -c $k/roscklib.S

clock.c:	$k/clock.c
	cp $k/clock.c $@

clock.o:	clock.c staddr.h stmfp.h
	$(CC) $(CFLAGS) -c clock.c

$(ROBJ) $(filter-out clock.o,$(COBJ)): %.o: $k/%.c
	$(CC) $(CFLAGS) -c $<

# library sources
printf.o:	$l/syslib/kprintf.c
	$(CC) $(CFLAGS) -c -o $@ $l/syslib/kprintf.c

gccmem.o:	../lib/gccmem.c
	$(CC) $(CFLAGS) -fno-tree-loop-distribute-patterns -c ../lib/gccmem.c

m68kmul.o:	../lib/m68kmul.S
	$(CC) $(ASFLAGS) -c ../lib/m68kmul.S

_sigset.o:	$l/posix/_sigset.c
	$(CC) $(CFLAGS) -I$l -c $l/posix/_sigset.c

# ../lib/u64.c: lib/m68000/misc/u64.c with cmp64u() fixed
u64.o:	../lib/u64.c
	$(CC) $(CFLAGS) -c ../lib/u64.c

$(filter-out printf.o _sigset.o u64.o m68kmul.o gccmem.o,$(LOBJ)): %.o: $l/ansi/%.c
	$(CC) $(CFLAGS) -c $<

roscmain.o roscrs232.o:	consbaud.stamp

# too crude, as in makefile.st
$(OBJ) $(LOBJ):	$(ri)/minix/config.h $(ri)/string.h $(ri)/stdarg.h $k/rosco.h $k/roscaddr.h \
		$k/roscduart.h $k/proc.h $k/const.h $k/glo.h $k/proto.h

clean:
	rm -f *.o libk.a clock.c kernel kernel.bin kernel.map ROSCODE1.BIN \
		consbaud.stamp
