#!/usr/bin/env python3
"""Print make variables STACK_<cmd> = <size> from a Minix command Makefile.

	stacks.py Makefile > stacks.mk

Takes the "install -S <size> $@" line of each "<cmd>: ..." rule, so the
rosco build gives a command the same stack as the ACK build (sizes are
in chmem notation: 4kw = 4096 words, 20k = 20480 bytes).
This is a host program.
"""
import re
import sys

target = None
for line in open(sys.argv[1]):
    m = re.match(r"^([A-Za-z0-9_.+-]+):", line)
    if m:
        target = m.group(1)
        continue
    m = re.match(r"^\s+install\s+-S\s+(\S+)\s+\$@", line)
    if m and target:
        print("STACK_%s = %s" % (target, m.group(1)))
