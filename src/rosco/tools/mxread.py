#!/usr/bin/env python3
"""Read a file from partition 2 (root) of a rosco SD card image (Minix V2, big endian).

	mxread.py card.img /path/in/fs > out

For checking files made on the board in MAME (the review, Follow-up 89).
This is a host program.
"""
import struct
import sys

B = 1024
card, path = sys.argv[1], sys.argv[2]
f = open(card, 'rb')
mbr = f.read(512)
lba = struct.unpack_from('<I', mbr, 446 + 16 + 8)[0]
base = lba * 512


def blk(n):
    f.seek(base + n * B)
    return f.read(B)


sb = blk(1)
ninodes, _, imap, zmap, first, _, _, magic, _, zones = struct.unpack_from('>HHhhHhIhhI', sb)
assert magic == 0x2468, hex(magic)


def inode(i):
    off = (2 + imap + zmap) * B + (i - 1) * 64
    f.seek(base + off)
    v = struct.unpack('>HHHHIIII10I', f.read(64))
    return v[0], v[4], list(v[8:18])


def data(i):
    mode, size, z = inode(i)
    out = bytearray()
    def zlist():
        for x in z[:7]:
            yield x
        if z[7]:
            for x in struct.unpack('>256I', blk(z[7])):
                yield x
        if z[8]:
            for ind in struct.unpack('>256I', blk(z[8])):
                for x in (struct.unpack('>256I', blk(ind)) if ind else [0] * 256):
                    yield x
    for x in zlist():
        if len(out) >= size:
            break
        out += blk(x) if x else bytes(B)
    return bytes(out[:size])


ino = 1
for comp in [c for c in path.split('/') if c]:
    d = data(ino)
    for k in range(0, len(d), 16):
        n, name = struct.unpack_from('>H14s', d, k)
        if n and name.rstrip(b'\0').decode('latin-1') == comp:
            ino = n
            break
    else:
        sys.exit('not found: ' + comp)
sys.stdout.buffer.write(data(ino))
