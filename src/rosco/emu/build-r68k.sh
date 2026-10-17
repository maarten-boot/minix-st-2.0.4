#!/bin/sh
# Fetch the r68k rosco_m68k emulator, add the XR68C681 DUART model from
# r68k-duart.patch and build it into src/rosco/emu/r68k/.
#
#	./build-r68k.sh
#	R68K_URL=/path/to/r68k ./build-r68k.sh	(clone from a local checkout)
#	r68k/r68k -d ../loader/ROSCODE1.BIN
#
# Without -d r68k behaves as upstream (firmware TRAP calls only, fixed 100Hz
# tick).  With -d the DUART registers at 0xF00001 are emulated: channel A on
# stdin/stdout, channel B output on stderr, the counter/timer in real time
# and a level-4 interrupt with the vector from IVR, so code that drives the
# DUART directly (loader, Minix kernel) can be run.  The SD card is the
# file rosco_sd.bin in the working directory (firmware EFP SD routines).
#
# The patch also makes the ROM's memory probe non-destructive (r68k loads
# the program before the ROM runs; the probe wrote 0x12345678 at 0x80000,
# into the loaded file) and adds debugging aids:
#	kill -USR1 <pid>	print registers and a disassembly, write the
#				RAM to r68k-ram.bin
#	R68K_TRACE=1		keep a PC trace, shown by the SIGUSR1 dump,
#				frozen when the PC runs into empty memory
#	R68K_WATCH=lo-hi[,plo-phi]  report writes into [lo, hi) (hex), not
#				those done by code in [plo, phi)
#	R68K_DUMPAT=pc		dump (as SIGUSR1) when pc is first reached
#
# The emulator is a host C++20 program; its small ROM is built with the
# rosco toolchain (m68k-elf-gcc, vasm) through ../xrun.

set -e

R68K_URL=${R68K_URL:-https://github.com/roscopeco/r68k.git}	# or a local clone
R68K_REV=bac2219bf6d1aca92b9ada77cb8d640ec7181940	# 2023-10-25

here=$(cd "$(dirname "$0")" && pwd)
dir=$here/r68k

if [ ! -d "$dir/.git" ]; then
	git clone -q "$R68K_URL" "$dir"
	git -C "$dir" checkout -q "$R68K_REV"
	git -C "$dir" apply "$here/r68k-duart.patch"
fi

(cd "$dir/firmware" && "$here/../xrun" make rosco_m68k.rom)
make -C "$dir" r68k
echo "built $dir/r68k"
