# Makefile for the Minix C library compiled with ACK (minix68k: m68k2,
# 16 bit int), as the ST port built it with ACK.
#
# Builds, from the sources in src/lib:
#	crtso.o		C start-off (lib/m68000/rts/ncrtso.s; link it first)
#	emrts.o		EM run-time symbols for ACK's libem.a (emrts.s; second)
#	libc.a		ansi, posix, other, stdio, the system call stubs and
#			the 68000 run-time parts, in ACK object format (aal)
# Programs link with ACK's own libem.a and libend.a (code generator support,
# end symbols); ../acc-minix compiles and links that way.
#
# The include path is src/include with only the rosco <minix/config.h> in
# front: the other rosco headers (string.h, stdarg.h) are for gcc.
#
#	make		build crtso.o and libc.a (needs ../build-ack.sh first)
#	make clean

ACC	= ../acc
AAL	= ../ack/.obj/staging/bin/aal

l	= ../../../lib
i	= ../../../../include
rl	= ../../lib

DEFS	= -D__ACK__ -D__minix=1 -D__m68000=1 -D_MINIX -D_POSIX_SOURCE
CFLAGS	= -O $(DEFS) -Iinc -I$i

# as the gcc libc (../../lib/Makefile): files that are not part of the
# ACK build's makefile.all, and those replaced by rosco versions
CDIRS	= ansi posix other stdio
EXCLUDE	= $l/other/regexp.c $l/posix/_exec.c $l/posix/_execn.c \
	  $l/other/execlp.c $l/other/v8regerror.c $l/ansi/system.c \
	  $l/posix/_uname.c
CSRC	= $(filter-out $(EXCLUDE),$(foreach d,$(CDIRS),$(wildcard $l/$d/*.c)))
COBJ	= $(patsubst $l/%.c,obj/%.o,$(CSRC)) \
	  obj/m68000/rts/__intsize.o obj/fphook/fltpr.o obj/fphook/strtod.o \
	  obj/rosco/u64.o obj/rosco/regexp.o obj/rosco/system.o \
	  obj/rosco/_uname.o

# the 68000 run-time in ACK syntax
RTS	= __longjmperr __sigreturn brksz catch initlib sendrec setjmp sndrec
ROBJ	= $(patsubst %,obj/m68000/rts/%.o,$(RTS))

# system call stubs ("_x: jmp __x", ACK syntax)
STUBS	= $(patsubst $l/syscall/%.s,obj/syscall/%.o,$(wildcard $l/syscall/*.s))

all:	crtso.o emrts.o libc.a

inc/minix/config.h:	../../include/minix/config.h
	@mkdir -p $(@D)
	cp ../../include/minix/config.h $@

crtso.o:	$l/m68000/rts/ncrtso.s inc/minix/config.h
	$(ACC) $(DEFS) -Iinc -I$i -c -o $@ $l/m68000/rts/ncrtso.s

emrts.o:	emrts.s
	$(ACC) -c -o $@ emrts.s

libc.a:	$(COBJ) $(ROBJ) $(STUBS)
	rm -f $@
	$(AAL) cr $@ $(COBJ) $(ROBJ) $(STUBS)

obj/%.o:	$l/%.c inc/minix/config.h
	@mkdir -p $(@D)
	$(ACC) $(CFLAGS) -I$(<D) -c -o $@ $<

obj/%.o:	$l/%.s inc/minix/config.h
	@mkdir -p $(@D)
	$(ACC) $(DEFS) -Iinc -I$i -c -o $@ $<

# rosco versions (../../lib): u64.c (cmp64u fixed), regexp.c, system.c
# (waitpid), _uname.c (machine rosco_m68k)
obj/rosco/%.o:	$(rl)/%.c inc/minix/config.h
	@mkdir -p $(@D)
	$(ACC) $(CFLAGS) -I$l/other -c -o $@ $<

# fslib.c includes "fs/const.h"
obj/other/fslib.o:	CFLAGS += -I../../..

clean:
	rm -rf obj inc crtso.o emrts.o libc.a
