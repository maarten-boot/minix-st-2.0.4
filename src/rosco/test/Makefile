# Makefile for the Minix test suite (src/test), rosco GNU build.
#
# Builds test1..test40, t10a, t11a, t11b (and troscorun from this
# directory, a test of the gcc run-time support) with src/rosco/lib as Minix
# executables, with the stack sizes of the ACK build (10kw, 32kw for
# test20 and test24).  They go into the SD card root under /usr/test
# (rootfs/sdproto), with test/run, sh1 and sh2.  test11 and test33 must
# be setuid root there.
#
#	make		build into bin/
#	make clean

ifeq ($(shell command -v m68k-elf-gcc 2>/dev/null),)

all clean:
	../xrun $(MAKE) -j2 $(filter -k,$(MAKEFLAGS:%=-%)) $@

else

CC	= m68k-elf-gcc
TOOLS	= ../tools
MKMINIX	= python3 $(TOOLS)/mkminix.py -q

t	= ../../test
i	= ../../../include
ri	= ../include
L	= ../lib

CPU	= -mcpu=68010 -mshort
DEFS	= -D_EM_WSIZE=2 -D_EM_PSIZE=4 -D_EM_LSIZE=4 -D__m68000=1 -D__minix=1 \
	  -D__MSHORT__ -D_MINIX -D_POSIX_SOURCE
# -O0: the tests wait for signal handlers in loops like "while (x == 0);"
# on variables that are not volatile; written for ACK, which does not
# keep them in registers.  gcc -O2 does, and the loop never ends.
CFLAGS	= $(CPU) -O0 -std=gnu89 -fno-builtin -fno-common \
	  -fno-strict-aliasing -fno-delete-null-pointer-checks \
	  -w $(DEFS) -I$(ri) -I$i
LDFLAGS	= $(CPU) -nostdlib -T $(TOOLS)/prog.ld -Wl,--emit-relocs
LIBS	= $L/libc.a

TESTS	= $(addprefix test,$(shell seq 1 40)) t10a t11a t11b
# rosco specific: the gcc run-time support (this directory)
ROSCO	= troscorun texecl
BIG	= test20 test24

all:	$(addprefix bin/,$(TESTS) $(ROSCO))

bin/%:	obj/%.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/$*.elf $L/crtso.o $< $(LIBS)
	$(MKMINIX) -S $(if $(filter $*,$(BIG)),32kw,10kw) obj/$*.elf $@

obj/%.o:	$t/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o:	%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin

.PRECIOUS:	obj/%.o

endif
