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
	  -w -include $(ri)/rosco_sizeof.h $(DEFS) -I$(ri) -I$i -I../..
LDFLAGS	= $(CPU) -nostdlib -T $(TOOLS)/prog.ld -Wl,--emit-relocs
LIBS	= $L/libc.a

# commands from commands/simple
SIMPLE	= basename cal cat chmod chown clr cmp cp cut date df dirname du echo \
	  ed env expr find fsck getty grep head id kill login ls mesg mkdir pwdauth \
	  mkfifo mkfs mknod mkswap mount od passwd printenv printroot pwd rmdir \
	  sed sleep sort stat stty su sum sync tail tee test touch tr tty umount \
	  uname uniq update wc which who whoami write yes \
	  cdiff comm compress crc dd diff fgrep paste prep shar strings tsort uud uue \
	  tar xargs printf pr split join rev fold expand unexpand file cksum \
	  time chmem strip size nm install mkproto inodes ncheck \
	  dhrystone gomoku life ttt factor \
	  at mail man tget \
	  banner calendar cgrep ci co cleantmp decomp16 fix fortune ifdef \
	  intr last leave look proto readall readfs remsync synctree sysenv \
	  term termcap backup finger badblocks isoread \
	  slip ifconfig add_route pr_routes hostaddr ping rdate nonamed \
	  rsh rcp rlogin in.rshd
CMDS	= sh ps aal make patch mined elvis halt shutdown dosread $(SIMPLE) \
	  awk bc yap m4 cron crontab kermit rz sz cawf asktime gzip ash \
	  advent elle ellec de indent ic yacc flex urlget ftp in.ftpd \
	  tcpd tcpdp telnet in.telnetd in.rlogind nc

all:	$(addprefix bin/,$(CMDS))

-include stacks.mk
# cp (also mv, rm, ln) copies through a 31744 byte buffer on the stack; the
# ACK build's 16kw only just held it, gcc's frames need more.
STACK_cp = 24kw
# at keeps three stdio buffers (stdin, stdout, the job file) on its heap; in
# the ACK build's 4kw the stack ran into them ("Memory fault").
STACK_at = 8kw
# in.rshd ran out of its ACK 4kw ("Stack low" from the kernel) and failed
STACK_in.rshd = 16kw
# passwd forks and execs /usr/lib/pwdauth for crypt(): with 4kw the child's
# execl had no room to build the new stack ("Arg list too long")
STACK_passwd = 8kw
# who: "Memory fault" on every run with the ACK build's 4kw
STACK_who = 8kw
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

# aal (ar): the ACK archiver, commands/aal (64k in the ACK build).
AALOBJ	= $(addprefix obj/aal/,archiver.o print.o rd.o rd_arhdr.o rd_unsig2.o \
	  sprint.o wr_arhdr.o wr_bytes.o wr_int2.o wr_long.o wr_ranlib.o \
	  format.o rd_bytes.o system.o write.o long2str.o)
bin/aal:	$(AALOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/aal.elf $L/crtso.o $(AALOBJ) $(LIBS)
	$(MKMINIX) -S 64k obj/aal.elf $@

obj/aal/%.o:	$c/aal/%.c
	@mkdir -p obj/aal
	$(CC) $(CFLAGS) -I$c/aal -DAAL -DSTB \
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
	$(CC) $(CFLAGS) -Dunix -c -o $@ $<

# patch: commands/patch (32kw in the ACK build), K&R code like aal
PATCHOBJ = $(addprefix obj/patch/,patch.o pch.o inp.o util.o version.o)
bin/patch:	$(PATCHOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/patch.elf $L/crtso.o $(PATCHOBJ) $(LIBS)
	$(MKMINIX) -S 32kw obj/patch.elf $@

obj/patch/%.o:	$c/patch/%.c
	@mkdir -p obj/patch
	$(CC) $(CFLAGS) -I$c/patch -DVOIDSIG -DSMALL \
		-c -o $@ $<

# halt (also reboot, by its name) and shutdown: commands/reboot
bin/halt:	obj/reboot/halt.o obj/reboot/log.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/halt.elf $L/crtso.o obj/reboot/halt.o obj/reboot/log.o $(LIBS)
	$(MKMINIX) -S 4kw obj/halt.elf $@

bin/shutdown:	obj/reboot/shutdown.o obj/reboot/sh_wall.o obj/reboot/log.o \
		$L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/shutdown.elf $L/crtso.o obj/reboot/shutdown.o \
		obj/reboot/sh_wall.o obj/reboot/log.o $(LIBS)
	$(MKMINIX) -S 4kw obj/shutdown.elf $@

obj/reboot/%.o:	$c/reboot/%.c
	@mkdir -p obj/reboot
	$(CC) $(CFLAGS) -I$c/reboot -c -o $@ $<

# mined: commands/mined, the Minix screen editor (64k in the ACK build)
MINEDOBJ = $(addprefix obj/mined/,mined1.o mined2.o)
bin/mined:	$(MINEDOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/mined.elf $L/crtso.o $(MINEDOBJ) $(LIBS)
	$(MKMINIX) -S 64k obj/mined.elf $@

obj/mined/%.o:	$c/mined/%.c
	@mkdir -p obj/mined
	$(CC) $(CFLAGS) -I$c/mined -c -o $@ $<

# elvis: commands/elvis, a vi clone (18kw in the ACK build); the Minix-ST
# settings of its Makefile: the libc termcap, no feature reductions
ELVISOBJ = $(addprefix obj/elvis/,blk.o cmd1.o cmd2.o ctype.o curses.o \
	   cut.o ex.o input.o main.o misc.o modify.o move1.o move2.o move3.o \
	   move4.o move5.o opts.o recycle.o redraw.o regexp.o regsub.o \
	   system.o tio.o tmp.o unix.o vars.o vcmd.o vi.o)
bin/elvis:	$(ELVISOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/elvis.elf $L/crtso.o $(ELVISOBJ) $(LIBS)
	$(MKMINIX) -S 18kw obj/elvis.elf $@

obj/elvis/%.o:	$c/elvis/%.c elvisproto.h
	@mkdir -p obj/elvis
	$(CC) $(CFLAGS) -include elvisproto.h -I$c/elvis -c -o $@ $<

# dosread (also dosdir, doswrite by its name): commands/ibm, FAT12/16 file
# access through the raw device, e.g. dosdir /dev/c0d0p0 (the boot partition).
# Written for the PC: dosread.patch adds the big endian conversion of the
# boot sector magic and the directory entries (the FAT16 entries already
# had it).  The patched copy is made in obj/ibm.
bin/dosread:	obj/ibm/dosread.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/dosread.elf $L/crtso.o obj/ibm/dosread.o $(LIBS)
	$(MKMINIX) -S 16kw obj/dosread.elf $@

obj/ibm/dosread.c:	$c/ibm/dosread.c dosread.patch
	@mkdir -p obj/ibm
	cp $c/ibm/dosread.c $@
	cd obj/ibm && patch -s -p1 < ../../dosread.patch

obj/ibm/dosread.o:	obj/ibm/dosread.c
	$(CC) $(CFLAGS) -c -o $@ obj/ibm/dosread.c

# Floating point programs: printf %e/%f/%g and strtod (fpprintf.o, before
# libc.a), libm, and the soft float runtime of the 68000 libgcc.  Its
# routines take and return long and double (and the int of a compare in
# d0, as a long), so the missing -mshort multilib does not matter.
LIBGCC	= $(shell $(CC) -mcpu=68000 -print-libgcc-file-name)
# (libc's ext_comp.c needs frexp and ldexp from libm, hence the group).
FPLIBS	= $L/fpprintf.o -Wl,--start-group $L/libm.a $L/libc.a $(LIBGCC) \
	  -Wl,--end-group

# awk: commands/awk (32kw in the ACK build).  The sources declare functions
# extern ("CELL *Arith();") and define them static, which gcc rejects; the
# copies in obj/awk have the "static" of the definitions removed, then get
# fixes/awk/<file>.patch if there is one (the code review).
AWKOBJ	= $(addprefix obj/awk/,m.o e.o n.o l.o r.o v.o y.o regexp.o k.o)
bin/awk:	$(AWKOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/awk.elf $L/crtso.o $(AWKOBJ) $(FPLIBS)
	$(MKMINIX) -S 32kw obj/awk.elf $@

obj/awk/%.c:	$c/awk/%.c $(wildcard fixes/awk/*.patch)
	@mkdir -p obj/awk
	sed -e 's/^static\([^;=(]*\)$$/\1/' \
	    -e 's/^static \([A-Za-z_0-9]*(\)/\1/' $< > $@
	if [ -f fixes/awk/$*.c.patch ]; then patch -s $@ fixes/awk/$*.c.patch; fi

# -I$c/awk first: the copies are in obj/awk, and "regexp.h" must be awk's,
# not the one in the system include directories
obj/awk/%.o:	obj/awk/%.c $c/awk/awk.h $c/awk/regexp.h
	$(CC) -I$c/awk $(CFLAGS) -c -o $@ $<

# bc: commands/bc (32k in the ACK build).  It loads the math library for
# -l from /usr/lib/libmath.b (BC_MATH_FILE in its config.h).
BCOBJ	= $(addprefix obj/bc/,bc.o scan.o util.o main.o number.o storage.o \
	  load.o execute.o global.o)
bin/bc:	$(BCOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/bc.elf $L/crtso.o $(BCOBJ) $(LIBS)
	$(MKMINIX) -S 32k obj/bc.elf $@

# bc.c and scan.c are the distributed yacc and flex output
$c/bc/bc.c $c/bc/scan.c: ;

# proto.h declares getopt() with const, <stdlib.h> without
obj/bc/%.o:	$c/bc/%.c
	@mkdir -p obj/bc
	$(CC) $(CFLAGS) -Dconst= -I$c/bc -c -o $@ $<

# yap (also more, by its name): commands/yap, a pager (16kw in the ACK build)
YAPOBJ	= $(addprefix obj/yap/,assert.o commands.o display.o getcomm.o \
	  getline.o help.o keys.o machine.o main.o options.o output.o \
	  pattern.o process.o prompt.o term.o)
bin/yap:	$(YAPOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/yap.elf $L/crtso.o $(YAPOBJ) $(LIBS)
	$(MKMINIX) -S 16kw obj/yap.elf $@

# __CC68__: no K&R malloc() and lseek() declarations in getline.c;
# pattern.c defines regerror(char *) against const in <regexp.h>
obj/yap/%.o:	$c/yap/%.c
	@mkdir -p obj/yap
	$(CC) $(CFLAGS) -D__CC68__ -Dconst= -I$c/yap -c -o $@ $<

# m4: commands/m4 (4kw in the ACK build)
M4OBJ	= $(addprefix obj/m4/,main.o eval.o serv.o look.o misc.o expr.o)
bin/m4:	$(M4OBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/m4.elf $L/crtso.o $(M4OBJ) $(LIBS)
	$(MKMINIX) -S 4kw obj/m4.elf $@

obj/m4/%.o:	$c/m4/%.c $c/m4/mdef.h $c/m4/extr.h
	@mkdir -p obj/m4
	$(CC) $(CFLAGS) -I$c/m4 -DEXTENDED -c -o $@ $<

# cron and crontab: commands/cron (8kw and 4kw in the ACK build)
bin/cron:	obj/cron/cron.o obj/cron/tab.o obj/cron/misc.o $L/crtso.o $(LIBS) \
		$(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/cron.elf $L/crtso.o $(filter obj/%,$^) $(LIBS)
	$(MKMINIX) -S 8kw obj/cron.elf $@

bin/crontab:	obj/cron/crontab.o obj/cron/tab.o obj/cron/misc.o $L/crtso.o \
		$(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/crontab.elf $L/crtso.o $(filter obj/%,$^) $(LIBS)
	$(MKMINIX) -S 4kw obj/crontab.elf $@

# tab.h defines crontabs in every file that includes it (a common symbol)
obj/cron/%.o:	$c/cron/%.c $c/cron/misc.h $c/cron/tab.h
	@mkdir -p obj/cron
	$(CC) $(CFLAGS) -fcommon -I$c/cron -c -o $@ $<

# kermit: commands/kermit, C-Kermit 4E (40k in the ACK build)
KERMITOBJ = $(addprefix obj/kermit/,ckcmai.o ckutio.o ckufio.o ckcfns.o \
	    ckcfn2.o ckcpro.o ckucmd.o ckuus2.o ckuus3.o ckuusr.o ckucon.o \
	    ckudia.o ckuscr.o)
bin/kermit:	$(KERMITOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/kermit.elf $L/crtso.o $(KERMITOBJ) $(LIBS)
	$(MKMINIX) -S 40k obj/kermit.elf $@

obj/kermit/%.o:	$c/kermit/%.c
	@mkdir -p obj/kermit
	$(CC) $(CFLAGS) -I$c/kermit -DV7 -DMINIX -c -o $@ $<

# rz and sz: commands/zmodem, one source file each that includes the rest
# (5kw in the ACK build)
bin/rz bin/sz:	bin/%:	obj/zmodem/%.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/$*.elf $L/crtso.o $< $(LIBS)
	$(MKMINIX) -S 5kw obj/$*.elf $@

obj/zmodem/%.o:	$c/zmodem/%.c $c/zmodem/rbsb.c $c/zmodem/zm.c $c/zmodem/zmodem.h
	@mkdir -p obj/zmodem
	$(CC) $(CFLAGS) -I$c/zmodem -DPOSIX -c -o $@ $<

# cawf (also nroff, by its install link): commands/cawf, for man (56k in
# the ACK build); its macro files go to /usr/lib/cawf
CAWFOBJ	= $(addprefix obj/cawf/,cawf.o device.o error.o expand.o expr.o \
	  getopt.o macsup.o nreq.o output.o pass2.o pass3.o regerror.o \
	  regexp.o store.o string.o)
bin/cawf:	$(CAWFOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/cawf.elf $L/crtso.o $(CAWFOBJ) $(FPLIBS)
	$(MKMINIX) -S 56k obj/cawf.elf $@

obj/cawf/%.o:	$c/cawf/%.c
	@mkdir -p obj/cawf
	$(CC) $(CFLAGS) -I$c/cawf -DUNIX -DUSG -DSTDLIB -c -o $@ $<

# gzip (also gunzip and zcat, by their names): GNU gzip 1.2.4 in
# ../gzip-1.2.4, as distributed.  gzip.patch adds a ROSCO section to
# tailor.h: the 16-bit (MAXSEG_64K, DYN_ALLOC) setup of its MS-DOS port,
# with fcalloc() and 16K read/write from gzrosco.c.  The patched copy is
# made in obj/gzip.  304K of buffers: window, hash tables, distances,
# input and output.
GZ	= ../gzip-1.2.4
GZSRC	= bits crypt deflate getopt gzip inflate lzw trees unlzh unlzw \
	  unpack unzip util zip
GZOBJ	= $(GZSRC:%=obj/gzip/%.o) obj/gzrosco.o
bin/gzip:	$(GZOBJ) $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/gzip.elf $L/crtso.o $(GZOBJ) $(LIBS)
	$(MKMINIX) -S 400k obj/gzip.elf $@

obj/gzip/stamp:	$(wildcard $(GZ)/*.[ch]) gzip.patch
	rm -rf obj/gzip
	mkdir -p obj/gzip
	cp $(GZ)/*.[ch] obj/gzip
	cd obj/gzip && patch -s -p1 < ../../gzip.patch
	touch $@

obj/gzip/%.o:	obj/gzip/stamp
	$(CC) $(CFLAGS) -DROSCO -c -o $@ obj/gzip/$*.c

obj/gzrosco.o:	gzrosco.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ gzrosco.c

# ash: commands/ash, the Minix 2.0.4 /bin/sh (12kw in the ACK build), with
# line editing (libedit.a).  The sources are copied to obj/ash, where the
# generators run on the build host: mkbuiltins, mktokens, bltin/mkexpr
# (scripts), mknodes, mksyntax, mkinit (host gcc; char is signed on both
# sides, which mksyntax depends on) and mksignames, which gets the Minix
# signal numbers in host/signal.h (the SIG macros of include/signal.h,
# through the host cpp with the target's defines).  mknodes initializes a
# global with stdin, which is not a constant in glibc; host/mknodes.c
# does without (infp is always set by fopen).  -Dconst= for main.c's
# getenv(char *), -D__CC68__ for bltin/regexp.c's malloc declaration.
# 32kw, not 12kw: with 12kw a 2000-character variable value or a 15K
# here document already gave "Out of space"; 32kw holds a 20K one.
HOSTCC	= gcc -std=gnu89 -w
ASHSRC	= builtins cd dirent error eval exec expand input jobs mail main \
	  memalloc miscbltin mystring nodes options parser redir show \
	  signames syntax trap output var
ASHOBJ	= $(ASHSRC:%=obj/ash/%.o) obj/ash/init.o \
	  $(addprefix obj/ash/bltin/,echo.o expr.o operators.o regexp.o)
bin/ash:	$(ASHOBJ) $L/crtso.o $L/libedit.a $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/ash.elf $L/crtso.o $(ASHOBJ) $L/libedit.a $(LIBS)
	$(MKMINIX) -S 32kw obj/ash.elf $@

obj/ash/stamp:	$(wildcard $c/ash/* $c/ash/bltin/* $c/ash/sys/*) $i/signal.h ash.patch
	rm -rf obj/ash
	mkdir -p obj/ash/host
	cp -r $c/ash/. obj/ash
	cd obj/ash && patch -s -p1 < ../../ash.patch
	gcc -E -dM -undef -nostdinc $(DEFS) -D__STDC__=1 -I$i $i/signal.h | \
		grep -E '^#define (SIG[A-Z0-9]+|_?NSIG) ' > obj/ash/host/signal.h
	cd obj/ash && $(HOSTCC) -Ihost -o host/mksignames mksignames.c && \
		host/mksignames
	sed 's/^FILE \*infp = stdin;/FILE *infp;/' obj/ash/mknodes.c > obj/ash/host/mknodes.c
	cd obj/ash && $(HOSTCC) -o host/mknodes host/mknodes.c && \
		host/mknodes nodetypes nodes.c.pat
	cd obj/ash && $(HOSTCC) -o host/mksyntax mksyntax.c && host/mksyntax
	cd obj/ash && sh mktokens && sh mkbuiltins shell.h builtins.table
	cd obj/ash/bltin && sh mkexpr
	cd obj/ash && $(HOSTCC) -o host/mkinit mkinit.c && \
		host/mkinit true $(ASHSRC:%=%.c)
	touch $@

obj/ash/%.o obj/ash/bltin/%.o:	obj/ash/stamp
	$(CC) $(CFLAGS) -DSHELL -Dconst= -D__CC68__ -Iobj/ash -c -o $@ $(@:.o=.c)

# The larger packages from commands/: one rule per program, with the stack
# of the ACK build.  pkgbin links obj/<pkg>/*.o of the listed sources.
define pkgbin
bin/$(1):	$(addprefix obj/$(2)/,$(3:=.o)) $$L/crtso.o $$(LIBS) $$(TOOLS)/mkminix.py
	@mkdir -p bin
	$$(CC) $$(LDFLAGS) -o obj/$(1).elf $$L/crtso.o $(addprefix obj/$(2)/,$(3:=.o)) $$(LIBS)
	$$(MKMINIX) -S $(4) obj/$(1).elf $$@
endef

# advent: the text files are indexed by setup (a host program, in obj/advent),
# which writes advtext.h and advent1-4.dat (/usr/lib/advent).
ADVSRC	= advent database english initial itverb score travel turn utility \
	  verb vocab
$(eval $(call pkgbin,advent,advent,$(ADVSRC),8kw))
obj/advent/stamp:	$(wildcard $c/advent/*)
	rm -rf obj/advent
	mkdir -p obj/advent
	cp $c/advent/*.txt obj/advent
	$(HOSTCC) -o obj/advent/setup $c/advent/setup.c
	cd obj/advent && ./setup
	touch $@
obj/advent/%.o:	$c/advent/%.c obj/advent/stamp
	$(CC) $(CFLAGS) -Iobj/advent -I$c/advent -DTEXTDIR='"/usr/lib/advent/"' \
		-c -o $@ $<

# elle, an Emacs-like editor, and ellec, its profile compiler (defprf.c,
# eefdef.h and eefidx.h are distributed already made by ellec)
ELLESRC	= eemain eecmds eesite eevini eedisp eeterm eeerr eeques eebuff \
	  eefile eefed eeedit eebit eef1 eef2 eefd eehelp eekmac eef3 eesrch \
	  eequer eefill eediag sbstr sbm sberr sbbcpy
$(eval $(call pkgbin,elle,elle,$(ELLESRC),64k))
$(eval $(call pkgbin,ellec,elle,ellec,16kw))
# Built without _MINIX, as its Makefile does: <string.h> would declare
# bcopy(), which elle has its own of (sbbcpy.c).  eefd.c declares
# moveborder() extern in a block and defines it static, which gcc rejects:
# the copy in obj/elle has the static removed.
ELLECF	= $(filter-out -D_MINIX,$(CFLAGS)) -DIGN_JOB_CONTROL -I$c/elle
obj/elle/%.o:	$c/elle/%.c
	@mkdir -p obj/elle
	$(CC) $(ELLECF) -c -o $@ $<
obj/elle/eefd.c:	$c/elle/eefd.c
	@mkdir -p obj/elle
	sed '/^static$$/{N;s/^static\nmoveborder(/moveborder(/;}' $< > $@
obj/elle/eefd.o:	obj/elle/eefd.c
	$(CC) $(ELLECF) -c -o $@ obj/elle/eefd.c

# de, the Minix disk (file system) editor.  de.patch: room for 32 zone map
# blocks (a 256MB file system) instead of 128 (128K, over the 64K object
# limit, and "128 * 1024" overflows a 16-bit int); de refuses a file system
# with more.  The patched copy is made in obj/de.
DESRC	= de de_stdin de_stdout de_diskio de_recover
$(eval $(call pkgbin,de,de,$(DESRC),4kw))
obj/de/stamp:	$(wildcard $c/de/*.[ch]) de.patch
	rm -rf obj/de
	mkdir -p obj/de
	cp $c/de/*.[ch] obj/de
	cd obj/de && patch -s -p1 < ../../de.patch
	touch $@
$(DESRC:%=obj/de/%.o):	obj/de/stamp
	$(CC) $(CFLAGS) -D_POSIX_1_SOURCE=2 -c -o $@ $(@:.o=.c)

# indent, the C program formatter
$(eval $(call pkgbin,indent,indent,args comment lexi indent parse io,32kw))
obj/indent/%.o:	$c/indent/%.c
	@mkdir -p obj/indent
	$(CC) $(CFLAGS) -I$c/indent -c -o $@ $<

# ic, the integer calculator
$(eval $(call pkgbin,ic,ic,ic ic_input ic_output,4kw))
obj/ic/%.o:	$c/ic/%.c
	@mkdir -p obj/ic
	$(CC) $(CFLAGS) -I$c/ic -c -o $@ $<

# yacc: Berkeley yacc (commands/byacc)
YACCSRC	= closure error lalr lr0 main mkpar output reader skeleton symtab \
	  verbose warshall
$(eval $(call pkgbin,yacc,byacc,$(YACCSRC),384k))
obj/byacc/%.o:	$c/byacc/%.c
	@mkdir -p obj/byacc
	$(CC) $(CFLAGS) -DNDEBUG -I$c/byacc -c -o $@ $<

# flex 2.3.7; flex.skel goes to /usr/lib/flex.  parse.y is turned into
# parse.c by a host build of byacc (obj/flex/host).
FLEXSRC	= ccl dfa ecs gen main misc nfa parse scan sym tblcmp yylex
$(eval $(call pkgbin,flex,flex,$(FLEXSRC),512k))
obj/flex/host/yacc:	$(YACCSRC:%=$c/byacc/%.c)
	@mkdir -p obj/flex/host
	$(HOSTCC) -DNDEBUG -o $@ $(YACCSRC:%=$c/byacc/%.c)
obj/flex/parse.c obj/flex/parse.h:	$c/flex-2.3.7/parse.y obj/flex/host/yacc
	cd obj/flex && host/yacc -d ../../$c/flex-2.3.7/parse.y && \
		mv y.tab.c parse.c && mv y.tab.h parse.h
obj/flex/%.o:	$c/flex-2.3.7/%.c obj/flex/parse.h
	$(CC) $(CFLAGS) -DUSG -DDEFAULT_SKELETON_FILE='"/usr/lib/flex/flex.skel"' \
		-Iobj/flex -I$c/flex-2.3.7 -c -o $@ $<
obj/flex/parse.o:	obj/flex/parse.c
	$(CC) $(CFLAGS) -DUSG -Iobj/flex -I$c/flex-2.3.7 -c -o $@ $<

# urlget (also httpget, ftpget by its name): commands/urlget, 4kw
$(eval $(call pkgbin,urlget,urlget,urlget net,4kw))
obj/urlget/%.o:	$c/urlget/%.c
	@mkdir -p obj/urlget
	$(CC) $(CFLAGS) -I$c/urlget -c -o $@ $<

# ftp, the client, and in.ftpd, the server (started per connection by
# tcpd): commands/ftp and commands/ftpd, 8kw each
$(eval $(call pkgbin,ftp,ftp,ftp local file other net,8kw))
obj/ftp/%.o:	$c/ftp/%.c
	@mkdir -p obj/ftp
	$(CC) $(CFLAGS) -I$c/ftp -c -o $@ $<
$(eval $(call pkgbin,in.ftpd,ftpd,ftpd access file net,8kw))
obj/ftpd/%.o:	$c/ftpd/%.c
	@mkdir -p obj/ftpd
	$(CC) $(CFLAGS) -I$c/ftpd -c -o $@ $<

# telnet (commands/telnet, "ttn"), 16kw (it died with "Memory fault" after
# connecting with the ACK 4kw), and in.telnetd (commands/telnetd, needs the
# ptys), 8kw
$(eval $(call pkgbin,telnet,telnet,ttn ttn_conf,16kw))
obj/telnet/%.o:	$c/telnet/%.c
	@mkdir -p obj/telnet
	$(CC) $(CFLAGS) -I$c/telnet -c -o $@ $<
# telnetd.patch: when a pty is not in /etc/ttytab, it printed its message
# without the name and looped forever.  The patched copy is in obj/telnetd.
TDSRC	= main telnet term pty wtmp
$(eval $(call pkgbin,in.telnetd,telnetd,$(TDSRC),8kw))
obj/telnetd/stamp:	$(wildcard $c/telnetd/*.[ch]) telnetd.patch
	rm -rf obj/telnetd
	mkdir -p obj/telnetd
	cp $c/telnetd/*.[ch] obj/telnetd
	cd obj/telnetd && patch -s -p1 < ../../telnetd.patch
	touch $@
$(TDSRC:%=obj/telnetd/%.o):	obj/telnetd/stamp
	$(CC) $(CFLAGS) -c -o $@ $(@:.o=.c)

# in.rlogind (commands/rlogind), on the ptys, 8kw (4kw in the ACK build,
# like in.rshd too little here)
$(eval $(call pkgbin,in.rlogind,rlogind,rlogind setup,8kw))
obj/rlogind/%.o:	$c/rlogind/%.c
	@mkdir -p obj/rlogind
	$(CC) $(CFLAGS) -I$c/rlogind -c -o $@ $<

# tcpd listens on a TCP port and runs a server for each connection; with
# flags, or when /etc/serv.access exists, it runs tcpdp, the same source
# built with PARANOID=1 (access checks, -d, -m)
$(eval $(call pkgbin,tcpd,tcpd,tcpd,4kw))
$(eval $(call pkgbin,tcpdp,tcpd,tcpdp,8kw))
obj/tcpd/tcpd.o:	$c/simple/tcpd.c
	@mkdir -p obj/tcpd
	$(CC) $(CFLAGS) -DPARANOID=0 -c -o $@ $<
obj/tcpd/tcpdp.o:	$c/simple/tcpd.c
	@mkdir -p obj/tcpd
	$(CC) $(CFLAGS) -DPARANOID=1 -c -o $@ $<

# nc: rosco's own small netcat (TCP connect and listen), 4kw
bin/nc:	obj/nc.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/nc.elf $L/crtso.o obj/nc.o $(LIBS)
	$(MKMINIX) -S 4kw obj/nc.elf $@

obj/nc.o:	nc.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ nc.c

# Source fixes (the code review, ROSCO_M68K_REVIEW.md).  The files in FIX0
# (paths below src/commands) are compiled from copies in obj/fix: through
# execl0.sed (execl() lists that ended with a bare 0, a 2-byte int with
# -mshort), then fixes/<path>.patch if there is one.  Each file also needs
# a rule that compiles its object from obj/fix/<path>.
FIX0	= simple/in.rshd.c make/make.c elle/eemain.c zmodem/rz.c \
	  reboot/halt.c reboot/shutdown.c reboot/log.c reboot/sh_wall.c \
	  simple/grep.c simple/readfs.c flex-2.3.7/misc.c flex-2.3.7/dfa.c \
	  kermit/ckcmai.c kermit/ckcpro.c kermit/ckucon.c kermit/ckudia.c \
	  kermit/ckufio.c kermit/ckuscr.c kermit/ckutio.c kermit/ckuus2.c \
	  kermit/ckuus3.c kermit/ckuusr.c simple/diff.c simple/cdiff.c \
	  simple/login.c simple/getty.c simple/su.c
.SECONDEXPANSION:
$(FIX0:%=obj/fix/%):	obj/fix/%:	$c/% execl0.sed $$(wildcard fixes/$$*.patch)
	@mkdir -p $(@D)
	sed -f execl0.sed $< > $@
	if [ -f fixes/$*.patch ]; then patch -s $@ fixes/$*.patch; fi
obj/in.rshd.o:	obj/fix/simple/in.rshd.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/readfs.o:	obj/fix/simple/readfs.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/make/make.o:	obj/fix/make/make.c $c/make/h.h
	@mkdir -p obj/make
	$(CC) $(CFLAGS) -Dunix -I$c/make -c -o $@ $<
obj/elle/eemain.o:	obj/fix/elle/eemain.c
	@mkdir -p obj/elle
	$(CC) $(ELLECF) -c -o $@ $<
obj/zmodem/rz.o:	obj/fix/zmodem/rz.c $c/zmodem/rbsb.c $c/zmodem/zm.c $c/zmodem/zmodem.h
	@mkdir -p obj/zmodem
	$(CC) $(CFLAGS) -I$c/zmodem -DPOSIX -c -o $@ $<
obj/reboot/halt.o obj/reboot/shutdown.o obj/reboot/log.o obj/reboot/sh_wall.o: \
		obj/reboot/%.o:	obj/fix/reboot/%.c
	@mkdir -p obj/reboot
	$(CC) $(CFLAGS) -I$c/reboot -c -o $@ $<
obj/grep.o:	obj/fix/simple/grep.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/diff.o:	obj/fix/simple/diff.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/cdiff.o:	obj/fix/simple/cdiff.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/login.o:	obj/fix/simple/login.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/getty.o:	obj/fix/simple/getty.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/su.o:	obj/fix/simple/su.c
	$(CC) $(CFLAGS) -c -o $@ $<
obj/flex/misc.o obj/flex/dfa.o: \
		obj/flex/%.o:	obj/fix/flex-2.3.7/%.c obj/flex/parse.h
	$(CC) $(CFLAGS) -DUSG -DDEFAULT_SKELETON_FILE='"/usr/lib/flex/flex.skel"' \
		-Iobj/flex -I$c/flex-2.3.7 -c -o $@ $<
obj/kermit/ckcmai.o obj/kermit/ckcpro.o obj/kermit/ckucon.o \
obj/kermit/ckudia.o obj/kermit/ckufio.o obj/kermit/ckuscr.o \
obj/kermit/ckutio.o obj/kermit/ckuus2.o obj/kermit/ckuus3.o \
obj/kermit/ckuusr.o:	obj/kermit/%.o:	obj/fix/kermit/%.c
	@mkdir -p obj/kermit
	$(CC) $(CFLAGS) -I$c/kermit -DV7 -DMINIX -c -o $@ $<

# asktime: rosco's own, asks for the time at boot (/etc/rc)
bin/asktime:	obj/asktime.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/asktime.elf $L/crtso.o obj/asktime.o $(LIBS)
	$(MKMINIX) -S 4kw obj/asktime.elf $@

obj/asktime.o:	asktime.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ asktime.c

# finger without the network part (NONET 1: netfinger() is a stub); the
# rosco libc has no lib/ip (gethostbyname, inet_addr)
obj/finger.c:	$c/simple/finger.c
	@mkdir -p obj
	sed 's/^#define NONET	0$$/#define NONET	1/' $< > $@
obj/finger.o:	obj/finger.c
	$(CC) $(CFLAGS) -c -o $@ obj/finger.c

# printf.c passes strtoul() results to printf("%d"), see printfint.h
obj/printf.o:	CFLAGS += -include printfint.h
obj/printf.o:	printfint.h

# the curses programs
bin/life bin/gomoku:	LIBS = $L/libcurses.a $L/libc.a

bin/%:	obj/%.o $L/crtso.o $(LIBS) $(TOOLS)/mkminix.py stacks.mk
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o obj/$*.elf $L/crtso.o $< $(LIBS)
	$(MKMINIX) -S $(or $(STACK_$*),4kw) obj/$*.elf $@

# grep.c, calendar.c and cgrep.c define regerror(char *s) against
# "const char *" in <regexp.h>
obj/grep.o obj/calendar.o obj/cgrep.o:	CFLAGS += -Dconst=

obj/%.o:	$c/simple/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin stacks.mk

.PRECIOUS:	obj/%.o obj/awk/%.c

endif
