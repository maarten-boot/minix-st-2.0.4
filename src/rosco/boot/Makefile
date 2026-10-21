# Makefile for the rosco Minix boot programs: MM, FS and INIT, and INET
# (the network server, started by /etc/rc; not in the boot image).
#
# GNU build (m68k-elf-gcc -mshort) of src/mm, src/fs and sttools/init.c
# with the rosco config, linked with src/rosco/lib and converted to Minix
# executables (src/rosco/tools/mkminix.py).  Objects go into this
# directory.
#
# Run on the host: the cross part re-runs itself in the rosco toolchain
# container (../xrun) unless m68k-elf-gcc is on the PATH.
#
#	make		build mm, fs, init, inet
#	make clean

ifeq ($(shell command -v m68k-elf-gcc 2>/dev/null),)

all clean:
	../xrun $(MAKE) -j2 $(filter -k,$(MAKEFLAGS:%=-%)) $@

else

CC	= m68k-elf-gcc
TOOLS	= ../tools
MKMINIX	= python3 $(TOOLS)/mkminix.py -q

s	= ../..
i	= ../../../include
ri	= ../include
L	= ../lib

CPU	= -mcpu=68010 -mshort
DEFS	= -D_EM_WSIZE=2 -D_EM_PSIZE=4 -D_EM_LSIZE=4 -D__m68000=1 -D__minix=1 -D__MSHORT__
INCS	= -I$(ri) -I$i
CFLAGS	= $(CPU) -O2 -std=gnu89 -fno-builtin -fno-common \
	  -fno-strict-aliasing -fno-delete-null-pointer-checks \
	  -w -include $(ri)/rosco_sizeof.h $(DEFS) $(INCS)
LDFLAGS	= $(CPU) -nostdlib -T $(TOOLS)/prog.ld -Wl,--emit-relocs
LIBC	= $L/libc.a
LIBSYS	= $L/libsys.a

MMOBJ	= $(addprefix mmobj/,main.o forkexit.o break.o exec.o signal.o alloc.o \
	  utility.o table.o trace.o getset.o misc.o)
FSOBJ	= $(addprefix fsobj/,main.o open.o read.o write.o pipe.o device.o \
	  path.o mount.o link.o super.o inode.o cache.o cache2.o filedes.o \
	  stadir.o protect.o time.o misc.o utility.o table.o lock.o)

INETOBJ	= $(addprefix inetobj/,buf.o clock.o inet.o inet_config.o mnx_eth.o \
	  mq.o sr.o stacktrace.o version.o) inetobj/inetrosco.o \
	  $(addprefix inetobj/generic/,udp.o arp.o eth.o event.o icmp.o io.o \
	  ip.o ip_ioctl.o ip_lib.o ip_read.o ip_write.o ipr.o tcp.o tcp_lib.o \
	  tcp_recv.o tcp_send.o ip_eth.o ip_ps.o psip.o)

all:	mm fs init inet

# stack sizes: the ST build used 6k (MM), 2k (FS), 1k (INIT)
mm:	$(MMOBJ) $L/crtso.o $(LIBSYS) $(LIBC) $(TOOLS)/mkminix.py
	$(CC) $(LDFLAGS) -Wl,-Map,mm.map -o mm.elf $L/crtso.o $(MMOBJ) \
		$(LIBSYS) $(LIBC) $(LIBSYS)
	$(MKMINIX) -S 6k mm.elf $@

fs:	$(FSOBJ) $L/crtso.o $(LIBSYS) $(LIBC) $(TOOLS)/mkminix.py
	$(CC) $(LDFLAGS) -Wl,-Map,fs.map -o fs.elf $L/crtso.o $(FSOBJ) \
		$(LIBSYS) $(LIBC) $(LIBSYS)
	$(MKMINIX) -S 4k fs.elf $@

init:	init.o $L/crtso.o $(LIBSYS) $(LIBC) $(TOOLS)/mkminix.py
	$(CC) $(LDFLAGS) -Wl,-Map,init.map -o init.elf $L/crtso.o init.o \
		$(LIBSYS) $(LIBC) $(LIBSYS)
	$(MKMINIX) -S 2k init.elf $@

# INET, psip only (inet/inet_config.h: no ethernet, no ARP): SLIP through
# /dev/psip0 and the slip command.  2kw in the ACK build.
inet:	$(INETOBJ) $L/crtso.o $(LIBSYS) $(LIBC) $(TOOLS)/mkminix.py
	$(CC) $(LDFLAGS) -Wl,-Map,inet.map -o inet.elf $L/crtso.o $(INETOBJ) \
		$(LIBSYS) $(LIBC) $(LIBSYS)
	$(MKMINIX) -S 8k inet.elf $@

inetobj/inetrosco.o:	inetrosco.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

# Fixed copy: psip drops a packet too big for slip's read (was EPACKSIZE
# forever, slip exited)
inetobj/generic/psip.o:	inetfix/generic/psip.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-I$s/inet/generic -c -o $@ $<

# Fixed copies: a listen cancelled after answering a SYN keeps the half
# open connection for the next listen (was closed without a RST, the
# client hung)
inetobj/generic/tcp.o:	inetfix/generic/tcp.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-I$s/inet/generic -c -o $@ $<

inetobj/generic/tcp_recv.o:	inetfix/generic/tcp_recv.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-I$s/inet/generic -c -o $@ $<

# Fixed copy (TCP review): an established orphan is not closed, 32-bit
# congestion window, no 64K write after an urgent byte; tcp.c and
# tcp_recv.c above have fixes of the review too (see their headers)
inetobj/generic/tcp_send.o:	inetfix/generic/tcp_send.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-I$s/inet/generic -c -o $@ $<

# Fixed copy: IP reassembly holds at most 16 accessors per packet and no
# longer leaks or loses fragments (a remote host could panic INET with
# fragments: out of accessors, or of buffers)
inetobj/generic/ip_read.o:	inetfix/generic/ip_read.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-I$s/inet/generic -c -o $@ $<

# BUF512_NR 120 (the CRAMPED default is 32): 120 buffers of 512 bytes and
# 180 accessors, 60 of them spare for packets in hand (see the
# ip_read.c fix); 120 keeps the buffer array under 64k
inetobj/buf.o:	$s/inet/buf.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-DBUF512_NR=120 -c -o $@ $<

# TCP_DEF_MSS 256: the TCP segment size INET announces, for the classic
# SLIP MTU of 296 (this INET cannot set an MTU; set 296 at the other end)
inetobj/%.o:	$s/inet/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -DTCP_DEF_MSS=256 -I$s/inet \
		-c -o $@ $<

mmobj/%.o:	$s/mm/%.c
	@mkdir -p mmobj
	$(CC) $(CFLAGS) -c -o $@ $<

fsobj/%.o:	$s/fs/%.c
	@mkdir -p fsobj
	$(CC) $(CFLAGS) -c -o $@ $<

init.o:	$s/sttools/init.c
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -c -o $@ $<

clean:
	rm -rf mmobj fsobj inetobj mm fs inet *.o *.elf *.map init

endif
