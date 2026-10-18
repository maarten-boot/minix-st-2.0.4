# Makefile for the C compiler of the rosco Minix: the Amsterdam Compiler
# Kit for minix68k (m68k2, 16 bit int).
#
#	make		build everything below
#	make host	ACK on the host (build-ack.sh), the cross compiler (acc)
#	make lib	the Minix C library compiled with ACK (lib/)
#	make pass	the ACK passes as Minix programs (mkpass.py, pass/bin)
#	make ccproto	the proto lines for the SD root (mkccproto.py); the
#			top level make sdcard adds them when they exist
#	make clean	remove lib, pass and ccproto (not the ACK build)
#
# The host needs: an ANSI C compiler, GNU make, flex, bison, yacc, ninja,
# python3, lua5.4 with lua-posix (build-ack.sh), and the rosco toolchain
# container for m68k-elf-gcc (../xrun).

STAGING	= ack/.obj/staging

# name=target: the programs of a C compile, in the host build of ACK
PASSES	= cpp.ansi=.obj/lang/cem/cpp.ansi/+cpp.ansi/cpp.ansi \
	  ack=.obj/util/ack/+ack/ack \
	  aal=.obj/util/arch/+aal/aal \
	  em_cemcom.ansi=.obj/lang/cem/cemcom.ansi/+cemcom/cemcom \
	  em_opt=.obj/util/opt/+em_opt/em_opt \
	  ncg=.obj/plat/minix68k/+ncg/ncg \
	  as=.obj/plat/minix68k/+as/as \
	  em_led=.obj/util/led/+led/led \
	  cv=.obj/plat/minix68k/cv/+cv/cv

all:	ccproto

host:	$(STAGING)/bin/ack

$(STAGING)/bin/ack:	ack-minix68k.patch build-ack.sh
	./build-ack.sh

lib:	host
	$(MAKE) -C lib

pass/bin/ack:	$(STAGING)/bin/ack mkpass.py $(wildcard passlib/*.c passinc/*.h)
	python3 mkpass.py $(PASSES)

pass:	pass/bin/ack

ccproto:	lib pass mkccproto.py cc.sh
	python3 mkccproto.py > $@

clean:
	$(MAKE) -C lib clean
	rm -rf pass ccproto

.PHONY:	all host lib pass clean
