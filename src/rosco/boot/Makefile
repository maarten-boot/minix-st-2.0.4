# Makefile for the rosco Minix boot programs: MM, FS and INIT.
#
# GNU build (m68k-elf-gcc -mshort) of src/mm, src/fs and sttools/init.c
# with the rosco config, linked with src/rosco/lib and converted to Minix
# executables (src/rosco/tools/mkminix.py).  Objects go into this
# directory.
#
# Run on the host: the cross part re-runs itself in the rosco toolchain
# container (../xrun) unless m68k-elf-gcc is on the PATH.
#
#	make		build mm, fs, init
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

all:	mm fs init

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

mmobj/%.o:	$s/mm/%.c
	@mkdir -p mmobj
	$(CC) $(CFLAGS) -c -o $@ $<

fsobj/%.o:	$s/fs/%.c
	@mkdir -p fsobj
	$(CC) $(CFLAGS) -c -o $@ $<

init.o:	$s/sttools/init.c
	$(CC) $(CFLAGS) -D_MINIX -D_POSIX_SOURCE -c -o $@ $<

clean:
	rm -rf mmobj fsobj mm fs *.o *.elf *.map init

endif
