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
SIMPLE	= basename cal cat chmod chown clr cmp cp cut date df dirname du echo \
	  ed env expr find fsck getty grep head id kill login ls mesg mkdir pwdauth \
	  mkfifo mkfs mknod mkswap mount od passwd printenv printroot pwd rmdir \
	  sed sleep sort stat stty su sum sync tail tee test touch tr tty umount \
	  uname uniq update wc which who whoami write yes \
	  cdiff comm compress crc dd diff fgrep paste prep shar strings tsort uud uue
CMDS	= sh ps aal make patch $(SIMPLE)

all:	$(addprefix bin/,$(CMDS))

-include stacks.mk
# cp (also mv, rm, ln) copies through a 31744 byte buffer on the stack; the
# ACK build's 16kw only just held it, gcc's frames need more.
STACK_cp = 24kw
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

# ps reads the kernel, MM and FS tables (sttools/ps.c, 24kw in the ACK build)
bin/ps:	obj/ps.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/ps.elf $L/crtso.o obj/ps.o $(LIBS)
	$(MKMINIX) -S 24kw obj/ps.elf $@

obj/ps.o:	../../sttools/ps.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -I../../kernel -c -o $@ ../../sttools/ps.c

# aal (ar): the ACK archiver, commands/aal (64k in the ACK build).  aal
# and make call malloc() & co. without prototypes, with sizeof arguments:
# include/rosco_sizeof.h makes sizeof(...) 16 bits as with ACK.
AALOBJ	= $(addprefix obj/aal/,archiver.o print.o rd.o rd_arhdr.o rd_unsig2.o \
	  sprint.o wr_arhdr.o wr_bytes.o wr_int2.o wr_long.o wr_ranlib.o \
	  format.o rd_bytes.o system.o write.o long2str.o)
bin/aal:	$(AALOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/aal.elf $L/crtso.o $(AALOBJ) $(LIBS)
	$(MKMINIX) -S 64k obj/aal.elf $@

obj/aal/%.o:	$c/aal/%.c
	@mkdir -p obj/aal
	$(CC) $(CFLAGS) -include $(ri)/rosco_sizeof.h -I$c/aal -DAAL -DSTB \
		-DNDEBUG -DDISTRIBUTION -c -o $@ $<

# make: commands/make (96k in the ACK build)
MAKEOBJ	= $(addprefix obj/make/,check.o input.o macro.o main.o make.o reader.o \
	  rules.o archive.o)
bin/make:	$(MAKEOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/make.elf $L/crtso.o $(MAKEOBJ) $(LIBS)
	$(MKMINIX) -S 96k obj/make.elf $@

obj/make/%.o:	$c/make/%.c $c/make/h.h
	@mkdir -p obj/make
	$(CC) $(CFLAGS) -include $(ri)/rosco_sizeof.h -Dunix -c -o $@ $<

# patch: commands/patch (32kw in the ACK build), K&R code like aal
PATCHOBJ = $(addprefix obj/patch/,patch.o pch.o inp.o util.o version.o)
bin/patch:	$(PATCHOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/patch.elf $L/crtso.o $(PATCHOBJ) $(LIBS)
	$(MKMINIX) -S 32kw obj/patch.elf $@

obj/patch/%.o:	$c/patch/%.c
	@mkdir -p obj/patch
	$(CC) $(CFLAGS) -include $(ri)/rosco_sizeof.h -I$c/patch -DVOIDSIG -DSMALL \
		-c -o $@ $<

bin/%:	obj/%.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py stacks.mk
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/$*.elf $L/crtso.o $< $(LIBS)
	$(MKMINIX) -S $(or $(STACK_$*),4kw) obj/$*.elf $@

# grep.c defines regerror(char *s) against "const char *" in <regexp.h>
obj/grep.o:	CFLAGS += -Dconst=

obj/%.o:	$c/simple/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin stacks.mk

.PRECIOUS:	obj/%.o

endif
