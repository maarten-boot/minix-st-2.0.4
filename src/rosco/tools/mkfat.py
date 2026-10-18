#!/usr/bin/env python3
"""Make a small FAT16 file system image with files in its root directory.

	mkfat.py [-s sectors] [-o lba] image file...

For the SD card image: the rosco firmware boots ROSCODE1.BIN from a FAT
partition.  The image has 512 byte sectors, two FATs and a 512 entry root
directory; each file is stored contiguously under its upper case 8.3 name.
The cluster size is the smallest power of two (1..64 sectors) that keeps
the cluster count within FAT16 (<= 65524).  -s is the size in sectors
(default 16384, 8 MB; 4200 up to 2 GB), -o the partition's start sector
(written into the boot sector as "hidden sectors", default 2048).  The
image is written sparse.  This is a host program.
"""
import os
import struct
import sys
import time

SECTOR = 512
ROOT_ENTRIES = 512


def die(msg):
    sys.exit("mkfat: " + msg)


def name83(path):
    base = os.path.basename(path).upper()
    stem, _, ext = base.partition(".")
    if not stem or len(stem) > 8 or len(ext) > 3:
        die("%s: not an 8.3 name" % base)
    return stem.ljust(8).encode() + ext.ljust(3).encode()


def main():
    args = sys.argv[1:]
    sectors, lba = 16384, 2048
    while args and args[0].startswith("-"):
        a = args.pop(0)
        if a == "-s":
            sectors = int(args.pop(0))
        elif a == "-o":
            lba = int(args.pop(0))
        else:
            die("unknown option " + a)
    if len(args) < 2:
        die("usage: mkfat.py [-s sectors] [-o lba] image file...")
    if sectors < 4200 or sectors > 4194304:
        die("size must be 4200..4194304 sectors (2 GB)")

    root_secs = ROOT_ENTRIES * 32 // SECTOR
    spc = 1
    while True:
        # FAT size for the clusters there would be without the FATs
        fat_secs = (((sectors - 1 - root_secs) // spc + 2) * 2
                    + SECTOR - 1) // SECTOR
        data_start = 1 + 2 * fat_secs + root_secs
        clusters = (sectors - data_start) // spc
        if clusters <= 65524 or spc == 64:
            break
        spc *= 2
    if not 4085 <= clusters <= 65524:
        die("cluster count %d is not FAT16" % clusters)
    csize = spc * SECTOR

    img = bytearray(sectors * SECTOR)
    bpb = struct.pack("<3s8sHBHBHHBHHHII", b"\xEB\x3C\x90", b"MINIXRSC",
                      SECTOR, spc, 1, 2, ROOT_ENTRIES,
                      sectors if sectors < 65536 else 0, 0xF8,
                      fat_secs, 32, 64, lba,
                      sectors if sectors >= 65536 else 0)
    ebpb = struct.pack("<BBBI11s8s", 0x80, 0, 0x29, int(time.time()) & 0xFFFFFFFF,
                       b"ROSCO MINIX", b"FAT16   ")
    img[0:len(bpb)] = bpb
    img[len(bpb):len(bpb) + len(ebpb)] = ebpb
    img[510:512] = b"\x55\xAA"

    fat = [0xFFF8, 0xFFFF]
    t = time.localtime()
    ftime = (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)
    fdate = ((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday
    root = bytearray()
    for path in args[1:]:
        data = open(path, "rb").read()
        n = (len(data) + csize - 1) // csize
        first = len(fat) if n else 0
        if first + n > clusters + 2:
            die("%s does not fit" % path)
        for i in range(n):
            fat.append(first + i + 1 if i < n - 1 else 0xFFFF)
        o = data_start * SECTOR + (first - 2) * csize
        img[o:o + len(data)] = data
        root += struct.pack("<11sBBBHHHHHHHI", name83(path), 0x20, 0, 0,
                            ftime, fdate, fdate, 0, ftime, fdate, first,
                            len(data))
        print("%s: %d bytes in clusters %d-%d"
              % (os.path.basename(path).upper(), len(data), first,
                 first + n - 1))
    fatbytes = struct.pack("<%dH" % len(fat), *fat)
    for k in range(2):
        o = (1 + k * fat_secs) * SECTOR
        img[o:o + len(fatbytes)] = fatbytes
    o = (1 + 2 * fat_secs) * SECTOR
    img[o:o + len(root)] = root
    write_sparse(args[0], img)
    print("%s: %d sectors, %d sectors per cluster, %d clusters"
          % (args[0], sectors, spc, clusters))


def write_sparse(path, data, chunk=65536):
    """Write data, skipping all-zero chunks (they read back as zeros)."""
    zero = bytes(chunk)
    with open(path, "wb") as f:
        for o in range(0, len(data), chunk):
            c = data[o:o + chunk]
            if c == zero[:len(c)]:
                f.seek(len(c), 1)
            else:
                f.write(c)
        f.truncate(len(data))


main()
