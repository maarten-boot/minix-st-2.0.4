#!/usr/bin/env python3
"""Make a Minix V2 file system image for the rosco_m68k (big endian).

	mkfs.py [-b blocks] [-i inodes] image spec [spec ...]

The host's mkfs would write little endian structures; FS on the 68000
reads the file system "native", big endian.  The layout is that of
src/fs (super.h, type.h, const.h), block size 1024, zone size = block:

	block 0		boot block (zero)
	block 1		super block (s_magic SUPER_V2)
	2 ...		inode bit map, zone bit map
	...		inodes, 16 per block (64 bytes, d2_inode)
	firstdatazone	data

Bit maps are arrays of 16 bit big endian words (bitchunk_t); bit 0 of both
maps is reserved.  Directory entries are 16 bytes: inode number, 14 name
characters.

The spec has one line per object, '#' starts a comment:

	d  PATH  MODE			directory
	f  PATH  MODE  HOSTFILE		regular file, contents from HOSTFILE
	c  PATH  MODE  MAJOR MINOR	character special
	b  PATH  MODE  MAJOR MINOR	block special
	l  PATH  TARGET			hard link to an existing file

MODE is octal.  Parents must come before their entries; "/" exists.  All
files belong to uid 0, gid 0.  Without -b the image is the used size plus
a quarter, at least 128 blocks.  The image is written sparse.  This is a
host program.
"""

import struct
import sys
import time

BLOCK = 1024
SUPER_V2 = 0x2468
INODE_SIZE = 64
INODES_PER_BLOCK = BLOCK // INODE_SIZE
NR_DZONES = 7
NR_INDIRECTS = BLOCK // 4
# direct, single and double indirect zones: 67378176 bytes with 1k blocks
MAX_FILE = (NR_DZONES + NR_INDIRECTS + NR_INDIRECTS ** 2) * BLOCK
DIRSIZ = 14

S_IFDIR, S_IFREG, S_IFCHR, S_IFBLK = 0o040000, 0o100000, 0o020000, 0o060000


def die(msg):
    sys.exit("mkfs: " + msg)


class Inode:
    def __init__(self, mode, nlinks=1):
        self.mode = mode
        self.nlinks = nlinks
        self.data = b""
        self.dev = 0
        self.entries = []		# directories: [(name, ino)]


def parse(specs):
    inodes = [None, Inode(S_IFDIR | 0o755, 2)]	# inode 1 is "/"
    paths = {"/": 1}
    lines = [(spec, n, l) for spec in specs
             for n, l in enumerate(open(spec), 1)]
    for spec, lineno, line in lines:
        w = line.split("#")[0].split()
        if not w:
            continue
        kind, path = w[0], w[1]
        parent, _, name = path.rpartition("/")
        parent = parent or "/"
        if parent not in paths:
            die("%s:%d: no directory %s" % (spec, lineno, parent))
        if len(name.encode()) > DIRSIZ or not name:
            die("%s:%d: bad name %r" % (spec, lineno, name))
        if path in paths:
            die("%s:%d: %s exists" % (spec, lineno, path))
        pdir = inodes[paths[parent]]
        if kind == "l":
            ino = paths.get(w[2])
            if ino is None:
                die("%s:%d: no %s" % (spec, lineno, w[2]))
            inodes[ino].nlinks += 1
        else:
            mode = int(w[2], 8)
            if kind == "d":
                node = Inode(S_IFDIR | mode, 2)
                pdir.nlinks += 1		# the child's ".."
            elif kind == "f":
                node = Inode(S_IFREG | mode)
                node.data = open(w[3], "rb").read()
                if len(node.data) > MAX_FILE:
                    die("%s:%d: %s: %d bytes, a V2 file holds at most %d"
                        % (spec, lineno, w[3], len(node.data), MAX_FILE))
            elif kind in "cb":
                node = Inode((S_IFCHR if kind == "c" else S_IFBLK) | mode)
                node.dev = (int(w[3]) << 8) | int(w[4])
            else:
                die("%s:%d: unknown type %s" % (spec, lineno, kind))
            inodes.append(node)
            ino = len(inodes) - 1
            if kind == "d":
                node.entries = [(".", ino), ("..", paths[parent])]
        pdir.entries.append((name, ino))
        paths[path] = ino
    inodes[1].entries[:0] = [(".", 1), ("..", 1)]
    for node in inodes[1:]:
        if node.mode & 0o170000 == S_IFDIR:
            node.data = b"".join(struct.pack(">H14s", ino, n.encode())
                                 for n, ino in node.entries)
    return inodes


def nblocks(size):
    n = (size + BLOCK - 1) // BLOCK
    extra = 0
    if n > NR_DZONES:
        extra += 1				# single indirect
    if n > NR_DZONES + NR_INDIRECTS:
        rest = n - NR_DZONES - NR_INDIRECTS
        extra += 1 + (rest + NR_INDIRECTS - 1) // NR_INDIRECTS
    return n + extra


def main():
    args = sys.argv[1:]
    blocks = inodes_wanted = 0
    while args and args[0].startswith("-"):
        a = args.pop(0)
        if a == "-b":
            blocks = int(args.pop(0))
        elif a == "-i":
            inodes_wanted = int(args.pop(0))
        else:
            die("unknown option " + a)
    if len(args) < 2:
        die("usage: mkfs.py [-b blocks] [-i inodes] image spec [spec ...]")
    nodes = parse(args[1:])

    used = sum(nblocks(len(n.data)) for n in nodes[1:])
    ninodes = max(inodes_wanted, len(nodes) - 1 + 32)
    ninodes = (ninodes + INODES_PER_BLOCK) // INODES_PER_BLOCK \
        * INODES_PER_BLOCK - 1		# fill the inode blocks (+1: bit 0)
    inode_blocks = (ninodes + INODES_PER_BLOCK - 1) // INODES_PER_BLOCK
    imap_blocks = (ninodes + 1 + BLOCK * 8 - 1) // (BLOCK * 8)
    if not blocks:
        blocks = max(128, 2 + imap_blocks + 1 + inode_blocks + used * 5 // 4)
    zmap_blocks = (blocks + BLOCK * 8 - 1) // (BLOCK * 8)
    first = 2 + imap_blocks + zmap_blocks + inode_blocks
    if first + used > blocks:
        die("%d blocks needed, %d given" % (first + used, blocks))

    img = bytearray(blocks * BLOCK)
    now = int(time.time())
    nextzone = first

    def alloc():
        nonlocal nextzone
        z = nextzone
        nextzone += 1
        return z

    for ino, node in enumerate(nodes):
        if node is None:
            continue
        zones = [0] * 10
        if node.mode & 0o170000 in (S_IFCHR, S_IFBLK):
            zones[0] = node.dev
        else:
            data = node.data
            n = (len(data) + BLOCK - 1) // BLOCK
            zlist = []
            for i in range(n):
                z = alloc()
                img[z * BLOCK:z * BLOCK + len(data[i * BLOCK:(i + 1) * BLOCK])] \
                    = data[i * BLOCK:(i + 1) * BLOCK]
                zlist.append(z)
            zones[:min(n, NR_DZONES)] = zlist[:NR_DZONES]
            rest = zlist[NR_DZONES:]
            if rest:
                ind = alloc()
                zones[7] = ind
                part, rest = rest[:NR_INDIRECTS], rest[NR_INDIRECTS:]
                struct.pack_into(">%dI" % len(part), img, ind * BLOCK, *part)
            if rest:
                dbl = alloc()
                zones[8] = dbl
                k = 0
                while rest:
                    ind = alloc()
                    struct.pack_into(">I", img, dbl * BLOCK + 4 * k, ind)
                    part, rest = rest[:NR_INDIRECTS], rest[NR_INDIRECTS:]
                    struct.pack_into(">%dI" % len(part), img, ind * BLOCK,
                                     *part)
                    k += 1
        off = (2 + imap_blocks + zmap_blocks) * BLOCK + (ino - 1) * INODE_SIZE
        size = 0 if node.mode & 0o170000 in (S_IFCHR, S_IFBLK) \
            else len(node.data)
        struct.pack_into(">HHHHIIII10I", img, off, node.mode, node.nlinks,
                         0, 0, size, now, now, now, *zones)

    def setbit(mapstart, bit):
        w = mapstart * BLOCK + (bit // 16) * 2
        v = struct.unpack_from(">H", img, w)[0] | (1 << (bit % 16))
        struct.pack_into(">H", img, w, v)

    for bit in range(len(nodes)):		# bit 0 and the used inodes
        setbit(2, bit)
    zmap = 2 + imap_blocks
    for bit in range(0, nextzone - first + 1):
        setbit(zmap, bit)
    # Bits beyond the last i-node and zone stay clear, as with Minix mkfs:
    # fsck expects that, and the FS never looks past them (alloc_bit()).

    max_size = (NR_DZONES + NR_INDIRECTS + NR_INDIRECTS ** 2) * BLOCK
    struct.pack_into(">HHhhHhIhhI", img, BLOCK, ninodes,
                     min(blocks, 0xFFFF), imap_blocks, zmap_blocks,
                     min(first, 0xFFFF), 0, min(max_size, 0x7FFFFFFF),
                     SUPER_V2, 0, blocks)
    zero = bytes(65536)			# written sparse: skip zero chunks
    with open(args[0], "wb") as f:
        for o in range(0, len(img), 65536):
            c = img[o:o + 65536]
            if c == zero[:len(c)]:
                f.seek(len(c), 1)
            else:
                f.write(c)
        f.truncate(len(img))
    print("%s: %d blocks, %d inodes (%d used), %d data blocks used, "
          "first data zone %d" % (args[0], blocks, ninodes, len(nodes) - 1,
                                  nextzone - first, first))


main()
