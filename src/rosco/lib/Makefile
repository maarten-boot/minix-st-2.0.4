# Makefile for the Minix C libraries, rosco GNU build.
#
# Builds, from the sources in src/lib, compiled with m68k-elf-gcc -mshort:
#	crtso.o		C start-off (put first when linking a program)
#	libc.a		ansi, posix, other, stdio, syscall stubs, runtime
#	libsys.a	syslib: the system library for MM, FS and INET
#	libcurses.a	curses (lib/curses), for life and gomoku
# The ACK assembly parts are replaced by the GNU files in this directory;
# the system call stubs (lib/syscall/*.s, "_x: jmp __x") are generated.
#
# Run on the host: the build re-runs itself in the rosco toolchain
# container (../xrun) unless m68k-elf-gcc is on the PATH.
#
#	make		build crtso.o, libc.a, libsys.a
#	make clean

ifeq ($(shell command -v m68k-elf-gcc 2>/dev/null),)

all clean:
	../xrun $(MAKE) -j2 $(filter -k,$(MAKEFLAGS:%=-%)) $@

else

CC	= m68k-elf-gcc
AR	= m68k-elf-ar

l	= ../../lib
i	= ../../../include
ri	= ../include

CPU	= -mcpu=68010 -mshort
# what ACK predefines for the 68000 (gcc defines __GNUC__ and __mc68010__)
DEFS	= -D_EM_WSIZE=2 -D_EM_PSIZE=4 -D_EM_LSIZE=4 -D__m68000=1 -D__minix=1 \
	  -D_MINIX -D_POSIX_SOURCE -D__MSHORT__
INCS	= -I$(ri) -I$i
CFLAGS	= $(CPU) -O2 -std=gnu89 -fno-builtin -fno-common \
	  -fno-strict-aliasing -fno-delete-null-pointer-checks \
	  -w -include $(ri)/rosco_sizeof.h $(DEFS) $(INCS)
ASFLAGS	= $(CPU) -Wa,--register-prefix-optional $(DEFS) $(INCS)

# C library directories; files that do not belong in the rosco libc
# (ACK runtime, floating point) are listed in EXCLUDE.
CDIRS	= ansi posix other stdio
# Not in the ACK build's makefile.all: posix/_exec.c and _execn.c are old
# versions of the exec family (their execl() passes the wrong argv, and
# whichever _execl comes first in the archive wins), other/execlp.c
# duplicates posix/_execlp.c.  regexp.c, system.c and _uname.c are built
# from ./
EXCLUDE	= $l/other/regexp.c $l/posix/_exec.c $l/posix/_execn.c \
	  $l/other/execlp.c $l/other/v8regerror.c $l/ansi/system.c \
	  $l/posix/_uname.c
CSRC	= $(filter-out $(EXCLUDE),$(foreach d,$(CDIRS),$(wildcard $l/$d/*.c)))
# u64.c replaces lib/m68000/misc/u64.c (cmp64u() fixed).
# fphook/fltpr.c and strtod.c: printf and strtod without floating point,
# as the ACK libc without the float library
COBJ	= $(patsubst $l/%.c,obj/%.o,$(CSRC)) obj/u64.o \
	  obj/m68000/rts/__intsize.o obj/regexp.o obj/system.o obj/_uname.o \
	  obj/fphook/fltpr.o obj/fphook/strtod.o

# GNU replacements of lib/m68000/rts
ROBJ	= obj/sndrec.o obj/setjmp.o obj/sigret.o obj/brksz.o obj/m68kmul.o \
	  obj/gccmem.o

# system call stubs
STUBS	= $(patsubst $l/syscall/%.s,obj/syscall/%.o,$(wildcard $l/syscall/*.s))

SYSSRC	= $(wildcard $l/syslib/*.c)
SYSOBJ	= $(patsubst $l/%.c,obj/%.o,$(SYSSRC))

CURSRC	= $(wildcard $l/curses/*.c)
CUROBJ	= $(patsubst $l/%.c,obj/%.o,$(CURSRC))

all:	crtso.o libc.a libsys.a libcurses.a

crtso.o:	crtso.S
	$(CC) $(ASFLAGS) -c -o $@ crtso.S

libc.a:	$(COBJ) $(ROBJ) $(STUBS)
	rm -f $@
	$(AR) rcs $@ $(COBJ) $(ROBJ) $(STUBS)

libsys.a:	$(SYSOBJ)
	rm -f $@
	$(AR) rcs $@ $(SYSOBJ)

libcurses.a:	$(CUROBJ)
	rm -f $@
	$(AR) rcs $@ $(CUROBJ)

obj/%.o:	$l/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(<D) -c -o $@ $<

obj/%.o:	%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$l/other -c -o $@ $<

obj/gccmem.o:	CFLAGS += -fno-tree-loop-distribute-patterns

# no floating point in scanf (no soft float runtime for -mshort)
obj/stdio/doscan.o:	CFLAGS += -DNOFLOAT

# fslib.c includes "fs/const.h"
obj/other/fslib.o:	CFLAGS += -I../..

obj/%.o:	%.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c -o $@ $<

# "_x: jmp __x" in ACK syntax becomes "x: jmp _x"
obj/syscall/%.S:	$l/syscall/%.s
	@mkdir -p $(@D)
	sed -n -e 's/^\.define[ 	]*_\([A-Za-z0-9_]*\).*/	.text\n	.globl	\1\n\1:/p' \
	       -e 's/^[ 	]*jmp[ 	]*_\([A-Za-z0-9_]*\).*/	jmp	\1/p' $< > $@

obj/syscall/%.o:	obj/syscall/%.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -rf obj crtso.o libc.a libsys.a libcurses.a

.PRECIOUS:	obj/syscall/%.S

endif
