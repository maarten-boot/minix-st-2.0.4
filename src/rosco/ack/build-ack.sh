#!/bin/sh
# Fetch the Amsterdam Compiler Kit, apply ack-minix68k.patch and build it
# for the minix68k platform only (Minix-ST: m68k2, 16 bit int), into
# src/rosco/ack/ack/.obj/staging.
#
#	./build-ack.sh
#	ACK_URL=/path/to/ack ./build-ack.sh	(clone from a local checkout)
#	./acc -O -o prog prog.c			(compile, see acc)
#
# The patch:
#  - Makefile: PLATS = minix68k, DEFAULT_PLATFORM = minix68k (the ACK
#    Makefile refuses PLATS on the command line);
#  - mach/m68020/ncg/mach.c: con_mult() printed .data8 for every multi word
#    constant; with WORD_SIZE=2 (m68k2) that includes 4 byte longs, so each
#    initialized long took 8 bytes.  Now .data4/.data8 by size;
#  - plat/minix68k/cv/cv.c: the Minix header was written from long mh[8],
#    64 bytes on a 64 bit host.  Now 32 big endian bytes.
#
# Needs on the host: an ANSI C compiler, GNU make, flex, bison, yacc,
# ninja, python3, lua (lua5.4) with lua-posix.  This is a host program.

set -e

ACK_URL=${ACK_URL:-https://github.com/davidgiven/ack.git}
ACK_REV=7afa32a0a0f13e865fa2e8104e442689005cd627	# 2026-07-18
LUA=${LUA:-lua5.4}

here=$(cd "$(dirname "$0")" && pwd)
dir=$here/ack

if [ ! -d "$dir/.git" ]; then
	git clone -q "$ACK_URL" "$dir"
	git -C "$dir" checkout -q "$ACK_REV"
	git -C "$dir" apply "$here/ack-minix68k.patch"
fi

make -C "$dir" -j"$(nproc)" LUA="$LUA"
echo "built $dir/.obj/staging"
