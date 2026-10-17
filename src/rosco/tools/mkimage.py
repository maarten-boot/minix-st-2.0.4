#!/usr/bin/env python3
"""Build the rosco Minix boot image: kernel + MM + FS + INIT.

	mkimage.py kernel.bin mm fs init image

What src/sttools/build.c does for the Atari ST, for the image that the
rosco loader copies to address 0:

  - kernel.bin (linked at 0) comes first, padded with zeros to the end of
    its bss: sizes[0] + sizes[1] clicks, taken from the kernel's sizes[]
    table (found behind the magic "rosSIZES", see roscompx.S);
  - MM, FS and INIT (Minix executables made by mkminix.py) follow, each
    relocated to its load address, with zeros for bss and stack, padded to
    a click;
  - sizes[2..7] in the kernel get (0, clicks) for each of them: the kernel
    treats it all as data, as build.c does;
  - FS's data space starts with 0xDADA (crtso.S); behind it go INIT's
    origin, text size (0) and data size in clicks, as patch3() in build.c.
    MM's data space must start with 0xDADA too.

This is a host program.
"""

import struct
import sys

CLICK = 256
SIZES_MAGIC = b"rosSIZES"
FS_D_MAGIC = 0xDADA


def die(msg):
    sys.exit("mkimage: " + msg)


def clicks(n):
    return (n + CLICK - 1) // CLICK


def load_minix(path):
    """Return (text+data bytes relocated to 0, text, data, total, relocs)."""
    d = open(path, "rb").read()
    cpu, flags, magic, _, _, hdrlen = struct.unpack(">BB2sHBB", d[:8])
    if magic != b"\x03\x01" or cpu != 0x0B:
        die("%s: not a Minix 68000 executable" % path)
    text, data, bss, entry, total, syms = struct.unpack(">6l", d[8:32])
    if entry != 0:
        die("%s: entry point not 0" % path)
    hl = hdrlen if hdrlen > 32 else 32
    body = bytearray(d[hl:hl + text + data])
    p = hl + text + data + syms
    first = struct.unpack(">I", d[p:p + 4])[0]
    relocs = []
    if first:
        off = first
        relocs.append(off)
        p += 4
        while True:
            c = d[p]
            p += 1
            if c == 0:
                break
            if c == 1:
                off += 254
                continue
            if c & 1:
                die("%s: odd relocation byte" % path)
            off += c
            relocs.append(off)
    return body, text, data, total, relocs


def main():
    if len(sys.argv) != 6:
        die("usage: mkimage.py kernel.bin mm fs init image")
    kernel = bytearray(open(sys.argv[1], "rb").read())

    at = kernel.find(SIZES_MAGIC)
    if at < 0 or kernel.find(SIZES_MAGIC, at + 1) >= 0:
        die("kernel: sizes table magic not found exactly once")
    sizes_off = at + len(SIZES_MAGIC)
    ktext, kdata = struct.unpack(">HH", kernel[sizes_off:sizes_off + 4])
    kclicks = ktext + kdata
    if len(kernel) > kclicks * CLICK:
        die("kernel.bin is larger than sizes[0] + sizes[1]")

    image = kernel + bytes(kclicks * CLICK - len(kernel))
    sizes = [ktext, kdata]
    progs = []
    for name, path in zip(("mm", "fs", "init"), sys.argv[2:5]):
        body, text, data, total, relocs = load_minix(path)
        base = len(image)
        for r in relocs:
            v = struct.unpack(">I", body[r:r + 4])[0]
            body[r:r + 4] = struct.pack(">I", (v + base) & 0xFFFFFFFF)
        n = clicks(total)
        image += body + bytes(n * CLICK - len(body))
        sizes += [0, n]
        progs.append((name, base, text, data, n))
        print("%-6s at %#07x  text %6d  data %5d  total %6d  %4d relocs"
              % (name, base, text, data, n * CLICK, len(relocs)))

    # sizes[] in the kernel
    for i, v in enumerate(sizes):
        struct.pack_into(">H", image, sizes_off + 2 * i, v)

    # MM and FS data magic, INIT's origin for FS
    for name, base, text, data, n in progs[:2]:
        m = struct.unpack(">H", image[base + text:base + text + 2])[0]
        if m != FS_D_MAGIC:
            die("%s data space: no magic 0xDADA (crtso.o first?)" % name)
    _, ibase, _, _, iclicks = progs[2]
    _, fbase, ftext, _, _ = progs[1]
    struct.pack_into(">HHH", image, fbase + ftext + 4,
                     ibase // CLICK, 0, iclicks)

    open(sys.argv[5], "wb").write(image)
    print("image  %d bytes (%d kB), system ends at %#x"
          % (len(image), len(image) // 1024, len(image)))


main()
