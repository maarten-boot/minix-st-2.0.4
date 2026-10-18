#!/usr/bin/env python3
"""Make an SD card image with an MBR and up to four partitions.

	mksd.py out.img [type:]fs1.img [[type:]fs2.img ...]

Each file system image becomes a primary partition, the first at sector
2048 (1 MB, as card partitioners do), the next directly after the previous
one.  The type is hex, default 81 (Minix, from mkfs.py); 06 is FAT16 (from
mkfat.py, which must be told the start sector with -o).  The MBR entries
are little endian with the CHS fields set to "beyond 1024 cylinders".
r68k uses rosco_sd.bin in its working directory as the SD card; on the
board, write the image to a card with dd.  The image is written sparse,
one partition at a time.  This is a host program.
"""
import os
import struct
import sys

SECTOR = 512
FIRST = 2048
CHUNK = 65536


def main():
    if not 3 <= len(sys.argv) <= 6:
        sys.exit("usage: mksd.py out.img fs1.img [fs2.img ...]")
    parts, types, names = [], [], []
    for a in sys.argv[2:]:
        t, sep, f = a.partition(":")
        if not sep or len(t) > 2:
            t, f = "81", a
        parts.append(os.path.getsize(f))
        types.append(int(t, 16))
        names.append(f)
    mbr = bytearray(SECTOR)
    lba = FIRST
    starts = []
    for i, size in enumerate(parts):
        n = (size + SECTOR - 1) // SECTOR
        entry = struct.pack("<B3sB3sII", 0, b"\xFE\xFF\xFF", types[i],
                            b"\xFE\xFF\xFF", lba, n)
        mbr[446 + 16 * i:446 + 16 * (i + 1)] = entry
        print("p%d: type %02x, %d sectors at %d (%s)" % (i, types[i], n, lba,
                                                     names[i]))
        starts.append(lba)
        lba += n
    mbr[510:512] = b"\x55\xAA"
    zero = bytes(CHUNK)
    with open(sys.argv[1], "wb") as out:
        out.write(mbr)
        for i, name in enumerate(names):
            out.seek(starts[i] * SECTOR)
            with open(name, "rb") as f:
                while True:
                    c = f.read(CHUNK)
                    if not c:
                        break
                    if c == zero[:len(c)]:
                        out.seek(len(c), 1)	# leave a hole
                    else:
                        out.write(c)
        out.truncate(lba * SECTOR)


main()
