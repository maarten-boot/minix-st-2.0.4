# Makefile for Minix commands, rosco GNU build.
#
# Builds the old Minix shell (commands/sh) and single file commands from
# commands/simple with src/rosco/lib, as Minix executables
# (src/rosco/tools/mkminix.py).  Each command gets the stack size of the
# ACK build (commands/simple/Makefile, see tools/stacks.py).
#
#	make		build $(CMDS) into bin/
#	make clean

ifeq ($(shell command -v m68k-elf-gcc 2>/dev/null),)

all clean:
	../xrun $(MAKE) -j2 $(filter -k,$(MAKEFLAGS:%=-%)) $@

else

CC	= m68k-elf-gcc
TOOLS	= ../tools
MKMINIX	= python3 $(TOOLS)/mkminix.py -q

c	= ../../commands
i	= ../../../include
ri	= ../include
L	= ../lib

CPU	= -mcpu=68010 -mshort
DEFS	= -D_EM_WSIZE=2 -D_EM_PSIZE=4 -D_EM_LSIZE=4 -D__m68000=1 -D__minix=1 -D__MSHORT__ \
	  -D_MINIX -D_POSIX_SOURCE
CFLAGS	= $(CPU) -O2 -std=gnu89 -fno-builtin -fno-common \
	  -fno-strict-aliasing -fno-delete-null-pointer-checks \
	  -w $(DEFS) -I$(ri) -I$i -I../..
LDFLAGS	= $(CPU) -nostdlib -T $(TOOLS)/prog.ld -Wl,--emit-relocs
LIBS	= $L/libc.a

# commands from commands/simple
SIMPLE	= cat cp date df echo kill ls mkdir mknod mount printroot pwd rmdir \
	  sleep stty sync touch umount uname wc
CMDS	= sh $(SIMPLE)

all:	$(addprefix bin/,$(CMDS))

-include stacks.mk
stacks.mk:	$c/simple/Makefile $(TOOLS)/stacks.py
	python3 $(TOOLS)/stacks.py $c/simple/Makefile > $@

# the shell: 11kw in the ACK build
SHOBJ	= $(addprefix obj/sh/,sh1.o sh2.o sh3.o sh4.o sh5.o sh6.o)
bin/sh:	$(SHOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/sh.elf $L/crtso.o $(SHOBJ) $(LIBS)
	$(MKMINIX) -S 11kw obj/sh.elf $@

obj/sh/%.o:	$c/sh/%.c $c/sh/sh.h
	@mkdir -p obj/sh
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%:	obj/%.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py stacks.mk
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/$*.elf $L/crtso.o $< $(LIBS)
	$(MKMINIX) -S $(or $(STACK_$*),4kw) obj/$*.elf $@

obj/%.o:	$c/simple/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin stacks.mk

.PRECIOUS:	obj/%.o

endif
