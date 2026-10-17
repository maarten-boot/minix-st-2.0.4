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
#			(rootfs/proto, SDROOTBLOCKS blocks).  The kernel takes
#			the first Minix partition as root when no root image
#			came with ROSCODE1.BIN.
#	make runsd	boot boot/sd/ROSCODE1.BIN in r68k, root on rosco_sd.bin
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
# SD card: FAT partition in 512 byte sectors, Minix root in 1k blocks
SDFATSECTORS = 16384
SDROOTBLOCKS = 8192
SDROOTINODES = 1024

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

boot/sd/ROSCODE1.BIN:	boot/image.bin
	mkdir -p boot/sd
	loader/mkrosco -o $@ loader/loader.bin boot/image.bin

boot/sdfat.img:	boot/sd/ROSCODE1.BIN tools/mkfat.py
	python3 tools/mkfat.py -s $(SDFATSECTORS) -o 2048 $@ boot/sd/ROSCODE1.BIN

boot/sdroot.img:	parts rootfs/proto rootfs/etc/*
	cd boot && python3 ../tools/mkfs.py -b $(SDROOTBLOCKS) -i $(SDROOTINODES) \
		sdroot.img ../rootfs/proto

rosco_sd.bin:	boot/sdfat.img boot/sdroot.img tools/mksd.py
	python3 tools/mksd.py $@ 06:boot/sdfat.img 81:boot/sdroot.img

sdcard:	rosco_sd.bin

runsd:	rosco_sd.bin
	$(R68K) -d boot/sd/ROSCODE1.BIN

run:	ROSCODE1.BIN
	$(R68K) -d ROSCODE1.BIN

clean:
	for d in $(PARTS); do $(MAKE) -C $$d clean; done
	rm -rf ROSCODE1.BIN boot/root.img boot/image.bin boot/sd boot/sdfat.img \
		boot/sdroot.img

.PHONY:	all parts sdcard run runsd clean
