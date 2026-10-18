# Top level Makefile of the rosco_m68k Minix port.
#
#	make		build everything and ROSCODE1.BIN in this directory:
#			loader, kernel, C library, MM/FS/INIT, commands,
#			root file system image
#	make run	boot ROSCODE1.BIN in r68k with the DUART model
#			(build it first with emu/build-r68k.sh); r68k uses
#			rosco_sd.bin here as the SD card
#	make sdcard	make rosco_sd.bin, an SD card image to dd to a card:
#			p0 FAT16 with ROSCODE1.BIN (the system without root
#			image, boot/sd/ROSCODE1.BIN), p1 the Minix root
#			(rootfs/proto + rootfs/sdproto: all commands and the
#			test suite in /usr/test, SDROOTBLOCKS blocks).  The kernel takes
#			the first Minix partition as root when no root image
#			came with ROSCODE1.BIN.  With the C compiler built
#			(make cc) the card also gets cc, /usr/ack and
#			/usr/include (ack/ccproto).
#	make cc		build the C compiler: ACK for minix68k on the host,
#			the Minix libc compiled with it and the ACK passes
#			as Minix programs (ack/Makefile; needs lua5.4,
#			lua-posix, flex, bison, ninja on the host)
#	make runsd	boot boot/sd/ROSCODE1.BIN in r68k, root on rosco_sd.bin
#	make runmame	boot rosco_sd.bin in the rosco-emulator (MAME, full
#			hardware and the real firmware, MAMEDIR), console on
#			a pty; needs make CONSBAUD=115200 sdcard.  Scripted:
#			tools/mamesession.py
#	make CONSBAUD=115200 ...  console speed (default 38400, the target
#			board's firmware; the stock firmware and the MAME based
#			rosco-emulator use 115200).  Rebuild the kernel when
#			changing it: make -C kernel clean
#	make clean
#
# ROSCODE1.BIN is what the rosco firmware loads from the SD card (or over
# Kermit): the loader stub, the boot image (kernel, MM, FS, INIT, see
# tools/mkimage.py) and the root file system (tools/mkfs.py, contents in
# rootfs/proto).  The cross compiling runs in the rosco toolchain container
# (xrun); the image tools are host programs (python3, cc).

R68K	= emu/r68k/r68k
MAMEDIR	= ../../../rosco-emulator
CONSBAUD =
PARTS	= loader kernel lib boot cmds test

# root file system size in 1k blocks; it lives in RAM next to the system
ROOTBLOCKS = 400
# SD card: FAT partition in 512 byte sectors, Minix root in 1k blocks
# (64MB: room for a compiler and sources; i-node numbers are 16 bits, so
# at most 65535 i-nodes)
SDFATSECTORS = 16384
SDROOTBLOCKS = 65536
SDROOTINODES = 8192

all:	ROSCODE1.BIN

test:
	$(MAKE) -C test

parts:
	$(MAKE) -C loader
	$(MAKE) -C kernel kernel.bin CONSBAUD=$(CONSBAUD)
	$(MAKE) -C lib
	$(MAKE) -C boot
	$(MAKE) -C cmds

# the build time, for /etc/rc to set the clock (no RTC): MMDDYYhhmmss GMT
boot/stamp:	FORCE
	date -u +%m%d%y%H%M%S > $@

boot/root.img:	parts boot/stamp rootfs/proto rootfs/etc/*
	cd boot && python3 ../tools/mkfs.py -b $(ROOTBLOCKS) root.img ../rootfs/proto

boot/image.bin:	parts tools/mkimage.py
	cd boot && python3 ../tools/mkimage.py ../kernel/kernel.bin mm fs init image.bin

ROSCODE1.BIN:	boot/image.bin boot/root.img
	loader/mkrosco -x boot/root.img -o $@ loader/loader.bin boot/image.bin

boot/sd/ROSCODE1.BIN:	boot/image.bin
	mkdir -p boot/sd
	loader/mkrosco -o $@ loader/loader.bin boot/image.bin

boot/sdfat.img:	boot/sd/ROSCODE1.BIN tools/mkfat.py
	python3 tools/mkfat.py -s $(SDFATSECTORS) -o 2048 $@ boot/sd/ROSCODE1.BIN

# the C compiler, if built (make cc)
CCPROTO	= $(wildcard ack/ccproto)

boot/sdroot.img:	parts test boot/stamp rootfs/proto rootfs/sdproto rootfs/etc/* \
			$(CCPROTO)
	cd boot && python3 ../tools/mkfs.py -b $(SDROOTBLOCKS) -i $(SDROOTINODES) \
		sdroot.img ../rootfs/proto ../rootfs/sdproto $(CCPROTO:%=../%)

cc:
	$(MAKE) -C ack

rosco_sd.bin:	boot/sdfat.img boot/sdroot.img tools/mksd.py
	python3 tools/mksd.py $@ 06:boot/sdfat.img 81:boot/sdroot.img

sdcard:	rosco_sd.bin

runsd:	rosco_sd.bin
	$(R68K) -d boot/sd/ROSCODE1.BIN

runmame:	rosco_sd.bin
	cd $(MAMEDIR) && ./rosco rosco_m68k_010 -hard1 $(CURDIR)/rosco_sd.bin \
		-terminal pty

run:	ROSCODE1.BIN
	$(R68K) -d ROSCODE1.BIN

clean:
	for d in $(PARTS); do $(MAKE) -C $$d clean; done
	rm -rf ROSCODE1.BIN boot/stamp boot/root.img boot/image.bin boot/sd boot/sdfat.img \
		boot/sdroot.img

.PHONY:	all parts test sdcard cc run runsd runmame clean FORCE
