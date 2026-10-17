#!/usr/bin/env python3
"""Convert an m68k ELF program into a Minix-ST executable.

	mkminix.py [-S stack] [-q] in.elf out

The ELF file must be linked at 0 with src/rosco/tools/prog.ld and
--emit-relocs (-q), so the absolute 32-bit relocations survive.  The output
is what MM's exec (mm/exec.c) loads:

  - the 32 byte a.out header of <a.out.h>: A_M68K, A_EXEC, magic 03 01,
    header length 0x20; text, data and bss sizes, entry 0, total memory
    (text + data + bss + stack), no symbols;
  - text and data;
  - the relocations in GEMDOS format: a long with the offset of the first
    long to relocate, then one byte per further relocation (the distance,
    even, 2..254; 1 means "add 254, no relocation"; 0 ends the list).  A
    long 0 means no relocations.

-S gives the stack size (default 8k; as chmem: k = 1024, w = 2 bytes).
-q is quiet.  This is a host program.
"""

import struct
import sys

R_68K_32 = 1
R_68K_PC32, R_68K_PC16, R_68K_PC8 = 4, 5, 6
R_68K_16, R_68K_8 = 2, 3
R_68K_NONE = 0
SHT_RELA, SHT_REL, SHT_SYMTAB, SHT_NOBITS = 4, 9, 2, 8
SHN_ABS = 0xFFF1


def die(msg):
    sys.exit("mkminix: " + msg)


def size_arg(s):
    s = s.lower()
    mul = 1
    if s.endswith("w"):			# words: 2 bytes on the 68000
        mul, s = 2, s[:-1]
    if s.endswith("k"):
        mul, s = mul * 1024, s[:-1]
    return int(s, 0) * mul


class Elf:
    def __init__(self, data):
        self.d = data
        if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
            die("not a 32 bit big endian ELF file")
        (self.type, self.machine, _, self.entry, _, shoff, _, _, _, _,
         shentsize, shnum, shstrndx) = struct.unpack(">HHIIIIIHHHHHH",
                                                     data[16:52])
        if self.machine != 4:
            die("not an m68k ELF file")
        self.sh = []
        for i in range(shnum):
            o = shoff + i * shentsize
            self.sh.append(struct.unpack(">IIIIIIIIII", data[o:o + 40]))
        strtab = self.sh[shstrndx]
        self.names = [self.cstr(strtab[4] + s[0]) for s in self.sh]

    def cstr(self, off):
        return self.d[off:self.d.index(b"\0", off)].decode()

    def section(self, name):
        for i, n in enumerate(self.names):
            if n == name:
                return i, self.sh[i]
        return None, None

    def contents(self, sh):
        if sh[1] == SHT_NOBITS:
            return b""
        return self.d[sh[4]:sh[4] + sh[5]]

    def symbols(self, idx):
        sh = self.sh[idx]
        out = []
        for o in range(sh[4], sh[4] + sh[5], 16):
            name, value, size, info, other, shndx = struct.unpack(
                ">IIIBBH", self.d[o:o + 16])
            out.append((value, shndx))
        return out


def main():
    args = sys.argv[1:]
    stack = 8 * 1024
    quiet = False
    while args and args[0].startswith("-"):
        a = args.pop(0)
        if a == "-S":
            stack = size_arg(args.pop(0))
        elif a == "-q":
            quiet = True
        else:
            die("unknown option " + a)
    if len(args) != 2:
        die("usage: mkminix.py [-S stack] [-q] in.elf out")
    elf = Elf(open(args[0], "rb").read())

    ti, text = elf.section(".text")
    di, data = elf.section(".data")
    bi, bss = elf.section(".bss")
    if text is None or text[3] != 0:
        die(".text missing or not linked at 0")
    tsize = text[5]
    dsize = data[5] if data else 0
    if data and data[3] != tsize:
        die(".data does not follow .text (addr %#x, text %#x)"
            % (data[3], tsize))
    bsize = bss[5] if bss else 0
    if bss and bss[3] != tsize + dsize:
        die(".bss does not follow .data")
    if elf.entry != 0:
        die("entry point is %#x, not 0 (crtso.o first?)" % elf.entry)

    image = bytearray(elf.contents(text) + (elf.contents(data) if data else b""))

    # Collect the absolute 32 bit relocations in .text and .data.
    relocs = []
    for i, sh in enumerate(elf.sh):
        if sh[1] not in (SHT_RELA, SHT_REL) or sh[7] not in (ti, di):
            continue
        # r_offset is an address in an executable, a section offset in a
        # relocatable file
        base = 0 if elf.type == 2 else elf.sh[sh[7]][3]
        syms = elf.symbols(sh[6])
        ent = 12 if sh[1] == SHT_RELA else 8
        for o in range(sh[4], sh[4] + sh[5], ent):
            off, info = struct.unpack(">II", elf.d[o:o + 8])
            rtype, sym = info & 0xFF, info >> 8
            addr = base + off
            if rtype in (R_68K_NONE, R_68K_PC32, R_68K_PC16, R_68K_PC8):
                continue
            absolute = syms[sym][1] == SHN_ABS if sym else True
            if rtype == R_68K_32:
                if not absolute:
                    relocs.append(addr)
            elif rtype in (R_68K_16, R_68K_8):
                if not absolute:
                    die("%s: 8/16 bit absolute reference at %#x cannot "
                        "be relocated" % (args[0], addr))
            else:
                die("%s: unsupported relocation type %d at %#x"
                    % (args[0], rtype, addr))

    relocs = sorted(set(relocs))
    rel = bytearray()
    if not relocs:
        rel += struct.pack(">I", 0)
    else:
        if relocs[0] == 0:
            die("relocation at offset 0 cannot be expressed")
        rel += struct.pack(">I", relocs[0])
        prev = relocs[0]
        for r in relocs[1:]:
            d = r - prev
            if d & 1:
                die("odd relocation offset %#x" % r)
            if d < 4:
                die("overlapping relocations at %#x" % r)
            while d > 254:
                rel.append(1)
                d -= 254
            rel.append(d)
            prev = r
        rel.append(0)

    total = tsize + dsize + bsize + stack
    hdr = struct.pack(">BB2sHBB6l", 0x0B, 0x10, b"\x03\x01", 0, 0, 0x20,
                      tsize, dsize, bsize, 0, total, 0)
    with open(args[1], "wb") as f:
        f.write(hdr + image + rel)
    if not quiet:
        print("%s: text %d data %d bss %d stack %d total %d, %d relocs"
              % (args[1], tsize, dsize, bsize, stack, total, len(relocs)))


main()
