# Makefile for the rosco C compiler tools (see README).
#
# Builds the host versions of the compiler chain into host/:
#	c68	the C compiler (c68 4.5 from xtc68), writes ACK syntax
#	jas	the Sozobon assembler, built for the ACK syntax (-DMINIX)
#	ld	the Sozobon linker, writes Minix executables (-DMINIX)
#	ar	the Sozobon archiver
#
#	make		build host/c68, host/jas, host/ld, host/ar
#	make clean
#
# These are host programs (cc).  The Sozobon sources assume a 32 bit long;
# sozobon/host32.h maps it on an LP64 host.

HOSTCC	= cc

C68SRC	= analyze.c cglbdef.c cmain.c decl.c expr.c extern.c genstmt.c \
	  genutil.c getsym.c init.c intexpr.c list.c memmgt.c msgout.c \
	  optimize.c outgen.c stmt.c symbol.c system.c types.c gen68k.c \
	  genffp.c genieee.c peep68k.c out68k_a.c out68k_q.c reg68k.c
C68FLAGS = -O2 -DXTC68 -DNDEBUG -w -Ic68

JASSRC	= cbuf.c cpy.c gen.c hdr.c lex.c main.c ops.c opt.c parse.c pass.c \
	  scan.c sym.c cross.c
LDSRC	= main.c pass1.c pass2.c bio.c syms.c rel.c utls.c cross.c
SOZFLAGS = -std=gnu89 -O -DUNIXHOST -DLITTLE_ENDIAN \
	  -include sozobon/host32.h -w

all:	host/c68 host/jas host/ld host/ar

host/c68:	$(addprefix c68/,$(C68SRC)) $(wildcard c68/*.h)
	@mkdir -p host
	$(HOSTCC) $(C68FLAGS) -o $@ $(addprefix c68/,$(C68SRC))

host/jas:	$(addprefix sozobon/jas/,$(JASSRC)) $(wildcard sozobon/jas/*.h) \
		sozobon/host32.h
	@mkdir -p host
	$(HOSTCC) $(SOZFLAGS) -DMINIX -o $@ $(addprefix sozobon/jas/,$(JASSRC))

host/ld:	$(addprefix sozobon/ld/,$(LDSRC)) $(wildcard sozobon/ld/*.h) \
		sozobon/host32.h
	@mkdir -p host
	$(HOSTCC) $(SOZFLAGS) -DMINIX -DNHASH=64 -o $@ \
		$(addprefix sozobon/ld/,$(LDSRC))

host/ar:	sozobon/tools/ar.c sozobon/tools/crossar.h sozobon/host32.h
	@mkdir -p host
	$(HOSTCC) $(SOZFLAGS) -o $@ sozobon/tools/ar.c

clean:
	rm -rf host
