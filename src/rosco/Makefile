# Top level Makefile of the rosco_m68k Minix port.
#
#	make		build everything and ROSCODE1.BIN in this directory:
#			loader, kernel, C library, MM/FS/INIT, commands,
#			root file system image
#	make run	boot ROSCODE1.BIN in r68k with the DUART model
#			(build it first with emu/build-r68k.sh)
#	make clean
#
# ROSCODE1.BIN is what the rosco firmware loads from the SD card (or over
# Kermit): the loader stub, the boot image (kernel, MM, FS, INIT, see
# tools/mkimage.py) and the root file system (tools/mkfs.py, contents in
# rootfs/proto).  The cross compiling runs in the rosco toolchain container
# (xrun); the image tools are host programs (python3, cc).

R68K	= emu/r68k/r68k
PARTS	= loader kernel lib boot cmds

# root file system size in 1k blocks; it lives in RAM next to the system
ROOTBLOCKS = 400

all:	ROSCODE1.BIN

parts:
	$(MAKE) -C loader
	$(MAKE) -C kernel kernel.bin
	$(MAKE) -C lib
	$(MAKE) -C boot
	$(MAKE) -C cmds

boot/root.img:	parts rootfs/proto rootfs/etc/*
	cd boot && python3 ../tools/mkfs.py -b $(ROOTBLOCKS) root.img ../rootfs/proto

boot/image.bin:	parts tools/mkimage.py
	cd boot && python3 ../tools/mkimage.py ../kernel/kernel.bin mm fs init image.bin

ROSCODE1.BIN:	boot/image.bin boot/root.img
	loader/mkrosco -x boot/root.img -o $@ loader/loader.bin boot/image.bin

run:	ROSCODE1.BIN
	$(R68K) -d ROSCODE1.BIN

clean:
	for d in $(PARTS); do $(MAKE) -C $$d clean; done
	rm -f ROSCODE1.BIN boot/root.img boot/image.bin

.PHONY:	all parts run clean
