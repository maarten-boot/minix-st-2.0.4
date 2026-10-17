#!/usr/bin/env python3
"""Make an SD card image with an MBR and up to four partitions.

	mksd.py out.img [type:]fs1.img [[type:]fs2.img ...]

Each file system image becomes a primary partition, the first at sector
2048 (1 MB, as card partitioners do), the next directly after the previous
one.  The type is hex, default 81 (Minix, from mkfs.py); 06 is FAT16 (from
mkfat.py, which must be told the start sector with -o).  The MBR entries
are little endian with the CHS fields set to "beyond 1024 cylinders".
r68k uses rosco_sd.bin in its working directory as the SD card; on the
board, write the image to a card with dd.  This is a host program.
"""
import struct
import sys

SECTOR = 512
FIRST = 2048


def main():
    if not 3 <= len(sys.argv) <= 6:
        sys.exit("usage: mksd.py out.img fs1.img [fs2.img ...]")
    parts, types, names = [], [], []
    for a in sys.argv[2:]:
        t, sep, f = a.partition(":")
        if not sep or len(t) > 2:
            t, f = "81", a
        parts.append(open(f, "rb").read())
        types.append(int(t, 16))
        names.append(f)
    mbr = bytearray(SECTOR)
    body = bytearray()
    lba = FIRST
    for i, fs in enumerate(parts):
        n = (len(fs) + SECTOR - 1) // SECTOR
        entry = struct.pack("<B3sB3sII", 0, b"\xFE\xFF\xFF", types[i],
                            b"\xFE\xFF\xFF", lba, n)
        mbr[446 + 16 * i:446 + 16 * (i + 1)] = entry
        body += fs + bytes(n * SECTOR - len(fs))
        print("p%d: type %02x, %d sectors at %d (%s)" % (i, types[i], n, lba,
                                                     names[i]))
        lba += n
    mbr[510:512] = b"\x55\xAA"
    with open(sys.argv[1], "wb") as f:
        f.write(mbr + bytes((FIRST - 1) * SECTOR) + body)


main()
