#!/usr/bin/env python3
"""Write the mkfs.py proto lines for the C compiler on the SD root.

	mkccproto.py > ccproto

Lists, with host paths relative to src/rosco/boot (where mkfs.py runs):
  /usr/ack	the ACK passes built by mkpass.py (bin/ack, bin/aal,
		lib/ack/...), ACK's descr files, libem.a and libend.a, and in
		minix/ the Minix C library built by lib/ (crtso.o, emrts.o,
		libc.a)
  /usr/bin/cc	cc.sh
  /usr/include	src/include, with the rosco <minix/config.h>
This is a host program.
"""
import os

here = os.path.dirname(os.path.abspath(__file__))
rosco = os.path.dirname(here)
top = os.path.dirname(os.path.dirname(rosco))
boot = os.path.join(rosco, "boot")
staging = os.path.join(here, "ack", ".obj", "staging")


def rel(p):
    return os.path.relpath(p, boot)


def main():
    print("# C compiler (src/rosco/ack/mkccproto.py)")
    print("d /usr/ack 755")
    for d in ("bin", "lib", "lib/ack", "lib/ack/minix68k", "share",
              "share/ack", "share/ack/descr", "share/ack/minix68k", "minix"):
        print("d /usr/ack/%s 755" % d)
    passes = os.path.join(here, "pass", "bin")
    for name, dst in (("ack", "bin/ack"), ("aal", "bin/aal"),
                      ("cpp.ansi", "lib/ack/cpp.ansi"),
                      ("em_cemcom.ansi", "lib/ack/em_cemcom.ansi"),
                      ("em_opt", "lib/ack/em_opt"),
                      ("em_led", "lib/ack/em_led"),
                      ("ncg", "lib/ack/minix68k/ncg"),
                      ("as", "lib/ack/minix68k/as"),
                      ("cv", "lib/ack/minix68k/cv"),
                      ("anm", "bin/anm"), ("asize", "bin/asize")):
        print("f /usr/ack/%s 755 %s" % (dst, rel(os.path.join(passes, name))))
    # nm and size for the objects cc makes
    print("l /usr/bin/anm /usr/ack/bin/anm")
    print("l /usr/bin/asize /usr/ack/bin/asize")
    for n in ("fe", "minix68k"):
        print("f /usr/ack/share/ack/descr/%s 644 %s"
              % (n, rel(os.path.join(staging, "share", "ack", "descr", n))))
    for n in ("libem.a", "libend.a"):
        print("f /usr/ack/share/ack/minix68k/%s 644 %s"
              % (n, rel(os.path.join(staging, "share", "ack", "minix68k", n))))
    for n in ("crtso.o", "emrts.o", "libc.a"):
        print("f /usr/ack/minix/%s 644 %s" % (n, rel(os.path.join(here, "lib", n))))
    print("f /usr/bin/cc 755 %s" % rel(os.path.join(here, "cc.sh")))

    inc = os.path.join(top, "include")
    print("d /usr/include 755")
    for root, dirs, files in os.walk(inc):
        dirs.sort()
        r = os.path.relpath(root, inc)
        for d in dirs:
            print("d /usr/include/%s 755" % os.path.normpath(os.path.join(r, d)))
        for f in sorted(files):
            src = os.path.join(root, f)
            if r == "minix" and f == "config.h":
                src = os.path.join(rosco, "include", "minix", "config.h")
            print("f /usr/include/%s 644 %s"
                  % (os.path.normpath(os.path.join(r, f)), rel(src)))


main()
