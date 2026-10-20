#!/usr/bin/env python3
"""Compile the cmds sources again with the warnings that find -mshort traps.

	warnscan.py [-f] [tool ...]		(run in src/rosco/cmds)

The cmds build compiles with -w.  This takes its compile commands
(make -n -B, in the toolchain container), compiles each source again with
-fsyntax-only and, instead of -w,
	-Wimplicit-function-declaration -Wint-conversion
	-Wincompatible-pointer-types -Wint-to-pointer-cast -Wpointer-to-int-cast
or with -f the printf/scanf checks: builtins on and -Wformat (gcc knows
that int is 16 bits under -mshort).  Prints the warnings per tool (all
tools, or the ones named), most first.  Nothing is built or changed; the
work files go to obj/warnscan.* (the container sees the tree, not /tmp).
See ROSCO_M68K_REVIEW.md.  This is a host program.
"""

import collections
import os
import re
import subprocess
import sys
import tempfile

TRAPS = ("-Wimplicit-function-declaration -Wint-conversion "
         "-Wincompatible-pointer-types -Wint-to-pointer-cast "
         "-Wpointer-to-int-cast")
FORMAT = ("-Wformat -Wno-format-zero-length -Wno-format-extra-args "
          "-Wno-builtin-declaration-mismatch")
KEEP = ("[-Wimplicit-function-declaration]", "[-Wint-conversion]",
        "[-Wincompatible-pointer-types]", "[-Wint-to-pointer-cast]",
        "[-Wpointer-to-int-cast]", "[-Wformat", "[-Woverflow]",
        "[-Wreturn-local-addr]")


def main():
    args = sys.argv[1:]
    fmt = args[:1] == ["-f"]
    if fmt:
        args = args[1:]
    want = set(args)
    n = subprocess.run(["../xrun", "make", "-n", "-B", "all"],
                       capture_output=True, text=True).stdout
    cmds = n.replace("\\\n", " ").splitlines()
    tools = collections.defaultdict(set)     # object -> tools
    for c in cmds:
        m = re.search(r"-o obj/(\S+)\.elf", c)
        if m:
            for o in re.findall(r"(obj/\S+\.o)", c):
                tools[o].add(m.group(1))
    os.makedirs("obj", exist_ok=True)
    work = tempfile.mkdtemp(prefix="warnscan.", dir=os.path.abspath("obj"))
    script = ["set +e"]
    objs = []
    for c in cmds:
        m = re.search(r"-o (\S+\.o)", c)
        if not (c.startswith("m68k-elf-gcc") and " -c " in c and m):
            continue
        o = m.group(1)
        if want and not tools[o] & want:
            continue
        c2 = c.replace(" -c ", " -fsyntax-only ")
        c2 = re.sub(r"-o \S+\.o", "", c2)
        if fmt:
            c2 = c2.replace(" -w ", " " + FORMAT + " ")
            c2 = c2.replace(" -fno-builtin ", " ")
        else:
            c2 = c2.replace(" -w ", " " + TRAPS + " ")
        out = os.path.join(work, o.replace("/", "__") + ".txt")
        script.append("%s > %s 2>&1" % (c2, out))
        objs.append((o, out))
    with open(os.path.join(work, "scan.sh"), "w") as f:
        f.write("\n".join(script) + "\n")
    subprocess.run(["../xrun", "sh", os.path.join(work, "scan.sh")])
    per_tool = collections.defaultdict(list)
    for o, out in objs:
        for line in open(out, errors="replace"):
            if "warning:" in line and any(k in line for k in KEEP):
                for t in tools[o]:
                    per_tool[t].append(line.rstrip())
    for t, ws in sorted(per_tool.items(), key=lambda x: -len(x[1])):
        print("== %s: %d" % (t, len(ws)))
        for w in ws:
            print("   " + w)
    print("%d sources scanned, %d tools with warnings (%s)"
          % (len(objs), len(per_tool), work))


if __name__ == "__main__":
    main()
