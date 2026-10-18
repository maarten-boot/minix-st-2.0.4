# Conversation log: Minix-ST → rosco_m68k feasibility study

Date: 2026-10-17. Branch: `rosco-m68k`. Assistant: Claude Code (Claude Opus 5.5).

## User request (verbatim)

> inspect the current directory an its files, it is a older minix version once
> adapted to run on the atari st, i would like to see if i can make it run on
> the https://github.com/rosco-m68k/rosco_m68k classic v2.1 motherboard when we
> strip all the hardware that this board does not have and change the rs232 to
> the XR68C681P, dont change any of the existing files just make a log of tyhe
> conversaytion and write a Spec file of what you find and what possible
> actions to take.

## What was done

1. **Repository survey** (read-only):
   * Read `Readme.md` and the top-level layout (`include/`, `src/`, `bin/`).
   * Reviewed `git status` / `git diff`. Local uncommitted Makefile edits
     replace `aal` with `ar` and add `XCFLAGS`. A `bin/aal` wrapper exists.
   * Ran `file` on the objects in `src/lib`. They are **x86-64 ELF**, i.e.
     host builds that are not usable on a 68k target.
2. **Kernel analysis**: read or grepped `src/kernel/makefile.st`,
   `stmain.c`, `stmpx.s` (vector table, CPU detection, ST model detection,
   `genint`, `save`/`restart`, `kreboot`, `.framesizes`), `staddr.h`,
   `stmfp.h`, `stram.h`, `rs232.c`, `rs2s.s`, `clock.c`, `tty.c`, `table.c`,
   `stcon.c`, `stnet.c`, `stshadow.c` and `include/minix/config.h`.
3. **Boot and build path**: read `src/sttools/boot.s`, `build.c`,
   `makefile.st` and `makeconfig`, plus the ACK settings in `makeack.c68`.
4. **MM/FS**: located the `MACHINE == ATARI` code in `mm/` and `fs/`
   (GEMDOS-style relocation in `mm/exec.c`, shadowing support).
5. **Target hardware research**: fetched the rosco_m68k GitHub page, then made
   a shallow clone of `release/version-2.4` into the session scratchpad (not
   into this repo) and read:
   * `code/shared/rosco_m68k_public.asm`: memory map, SDB/EFP addresses,
     DUART base `0xF00001` and register offsets;
   * `code/firmware/rosco_m68k_firmware/duart.asm`: DUART init, 115200
     extended rates, IVR `0x45`, 100 Hz timer;
   * `rev2.asm`: tick handler, LED;
   * `blockdev/dua_spi_asm.asm`: SD SPI pins on DUART OP2/4/6/7 and IP2;
   * `InterfaceReference.md`: TRAP 13/14/15 and the low 4 KB layout;
   * the r2 PLDs: ROM at `0xE00000`, the boot ROM mirror for 4 cycles, and the
     DUART IACK on IPL 4.
6. **68010-specific checks**: found `move.w sr,...` in user-mode
   `src/lib/m68000/rts/setjmp.s:42`. That instruction is privileged on a 68010
   and must be emulated or patched. Confirmed that the 68010 format-8 frame is
   already handled in `stmpx.s` `.framesizes`.

## Key findings (summary; details in the spec)

* The port is feasible. The kernel is already "68000 without MMU" (shadowing),
  and the 68010 is supported by the existing CPU detection.
* Almost all ST device drivers are removed. The new pieces are:
  * a vector table and DUART interrupt entry;
  * a DUART console on channel A and an rs232 driver on channel B;
  * the DUART timer as the clock (60 Hz is exact: N=1920);
  * a loader stub for `ROSCODE1.BIN` at 0x40000;
  * a memory-image root filesystem first, then an SD driver.
* The main blockers are the **toolchain** (an ACK with 16-bit ints is needed;
  the current host-built libs are unusable) and the **1 MB RAM budget**
  (`NR_BUFS` 512 has to be cut drastically).
* Rebooting must jump to the ROM vector at `0xE00004`, because address 4 is RAM
  after boot.

## Files created

* `ROSCO_M68K_PORT_SPEC.md`: findings, gap analysis, design, milestones, risks.
* `ROSCO_M68K_CONVERSATION_LOG.md`: this log.

No existing files were modified.

## Follow-up 1

User:
> ok i cleaned op the build remnants so the .o a,d .a files should be gone now.

Verified with `git status` and `find`: no `.o`, `.a` or `a.out` files remain.
The Makefile edits (`src/Makefile`, `src/lib/**/Makefile`,
`src/lib/makeconfig`) and `bin/aal` are still present. Spec section 2.2 is
updated to match.

## Follow-up 2

User:
> use new rosco files on the rosco-m68k branch

Decision recorded in spec section 9. Work done:

* Confirmed that only new files are needed. The kernel/mm/fs makefiles use
  `-I$i`, so a rosco makefile can put `src/rosco/include` first and override
  `<minix/config.h>`. The shared kernel code reaches the machine layer only
  through `proto.h` names, which rosco files can implement.
* Created `src/rosco/include/minix/config.h` (copy of the original plus rosco
  settings). Checked with host `cpp`: MACHINE=60 (ATARI), CHIP=M68000,
  SHADOWING=1, ATARI_TYPE=6 (ROSCO), NR_BUFS=48, NR_PROCS=16, NR_CONS=1,
  NR_RS_LINES=1, NR_PTYS=0, no FPP.
* Created `src/kernel/roscaddr.h` and `src/kernel/roscduart.h`. Host
  `cc -fsyntax-only` passed. The timer divisor for 60 Hz is 1920.
* No baud table was found in the rosco sources; only 0x88 = 115200 (set 2 +
  extended) is confirmed. The other CSR codes, RTS B and CTS A/B pins are
  marked "verify".
* Still pending: the toolchain choice. It determines whether the M1 loader and
  `roscompx.s` are written in ACK or GNU assembler syntax.

## Follow-up 3

User:
> use the gnu tools from the rosco sdk

While working, the user added:
> there is a docker image with the v13 m68k gcc
> https://github.com/rosco-m68k/rosco_m68k_docker

* No m68k toolchain was installed locally. As a stopgap, Ubuntu's
  `binutils-m68k-linux-gnu` was unpacked into the session scratchpad (no
  system change). It was superseded by the user's docker image.
* Pulled `roscopeco/rosco_m68k`: `m68k-elf-gcc` 13.3.0, binutils 2.40. The
  toolchain is in `/home/linuxbrew` (mode 750), so the container must run
  with `--group-add 1001` to be usable as the calling user.
* Checked that `-mcpu=68010 -mshort` gives 16-bit ints with the same argument
  layout as ACK m68k2.
* Created `src/rosco/xrun` (docker wrapper), `src/rosco/.gitignore`, and
  `src/rosco/loader/` with `loader.S`, `loader.ld`, `hello.S`, `payload.ld`,
  `mkrosco.c` and `Makefile`.
* `make` in `src/rosco/loader` builds `ROSCODE1.BIN` (5056 bytes). Checked:
  * the header fields;
  * vector 0x45 → `tick`;
  * the entry point at 0x1000;
  * the `movec d0,vbr` encoding (`4e7b 0801`);
  * the copier size against its `dbf` count;
  * the keep-range compare logic.
* Confirmed in the firmware source that it loads `/ROSCODE1.BIN` to
  `KERNEL_LOAD_ADDRESS` 0x40000.
* Not yet tested on real hardware (or in an emulator).
* Spec section 6.1 records the toolchain decision and what it implies:
  * GNU rewrites of the ACK assembly;
  * our own mul/div helpers, because there is no `-mshort` libgcc;
  * an ELF → Minix-ST executable converter;
  * a rosco boot-image builder.

## Follow-up 4

User:
> fyi: the simplefied stand alone emulator can run binaries:
> https://github.com/roscopeco/r68k

* Cloned r68k into the scratchpad and read the source:
  * Musashi core in 68010 mode, 1 MB RAM, ROM at 0xE00000;
  * the binary is loaded at 0x40000;
  * firmware TRAPs are handled by the emulator; TRAP 13 uses
    `rosco_sd.bin`;
  * **no DUART registers** (I/O reads 0);
  * a fixed-rate IRQ 4 thread delivers vector 0x45.
* Built it: the ROM with `vasmm68k_mot`/`m68k-elf-gcc` from the rosco docker
  image, the emulator with host g++ (C++20).
* Ran the M1 `ROSCODE1.BIN`: no output. The loader spins on the DUART TxRDY
  bit, as the source suggested.
* Added a minimal XR68C681 model to r68k, behind a new `-d` flag. Without
  `-d`, r68k behaves as upstream. The model covers:
  * channel A on stdin/stdout, channel B TX on stderr;
  * SR, ISR, IMR, IVR;
  * a level-sensitive IRQ 4 with the vector from IVR;
  * the counter/timer in real time.
* With `-d`, M1 passes completely: banner, magic `524F5343`, memory
  `00100000`, firmware SDB intact, one '.' per second from the 60 Hz DUART
  timer, input echo, and `q` back through the ROM reset vector.
* Saved as `src/rosco/emu/r68k-duart.patch` plus
  `src/rosco/emu/build-r68k.sh` (clone at commit `bac2219`, patch, build into
  the git-ignored `src/rosco/emu/r68k/`). Added `make run` to
  `src/rosco/loader/Makefile`. Re-tested end to end from a fresh clone through
  `make run`.
* Spec section 6.2 describes the emulator, the model and its limits.

## Open questions for the user

1. Is a working Minix-ST 2.0.4 installation (Hatari disk image with ACK)
   available for building?
2. Does the rosco board have expansion RAM, or any expansion cards (IDE,
   Xosera)?
3. Which firmware version is on the board?
4. When the implementation starts: in-place edits on the `rosco-m68k` branch,
   or new `rosco*` files with minimal guarded edits?

## Follow-up 5 (new session)

User:
> ok i interrupted a previous session and re arranged files so you also have
> access to all the tools. lets re-investigate the current directory and all
> its files you can find the code for the standalone simple simulator for the
> rosco-m68k at ../r68k and the code of the roso board at ../rosco_m68k

* Re-read the spec, this log, all files in `src/rosco/`, the new kernel
  headers and the uncommitted Makefile diffs. Everything from follow-ups 1–4
  is present. The docker image `roscopeco/rosco_m68k` is available locally.
* `../r68k` is upstream at `bac2219`, the commit `build-r68k.sh` pins, and
  `r68k-duart.patch` applies cleanly. `build-r68k.sh` now takes `R68K_URL`
  from the environment, so it can clone from that local checkout.
* `../rosco_m68k` is on `develop` (`ec229f9`). Compared with the v2.4 facts
  in the spec: DUART base, IVR, SDB/EFP addresses, load address and
  `/ROSCODE1.BIN` are unchanged. Stage 2 can also load `/ROSCODE1.ELF`, but
  it refuses segments below 0x2000, so it is no use for a kernel linked at 0.
* **Correction:** the firmware's .data/.bss sit at 0x1800–0x1FFF (KRES2 in
  `rosco_m68k_firmware_common.ld`; `kram = 0x1800` in v2.4), and user memory
  starts at 0x2000. The keep range 0x400–0x1000 used so far would have let
  the kernel overwrite the firmware's variables, which breaks the EFP SD
  calls planned for M5 (option A). M1 never calls an EFP, so the problem did
  not show. Changed `loader.S`, `mkrosco.c` and `hello.S` (keep range
  0x400–0x2000, payload code at 0x2000), `roscaddr.h` (`AD_FW_END`
  0x2000) and the `config.h` comment.
* Rebuilt and re-ran M1 in `r68k -d`: it passes, with the payload now running
  at 0x2000 (`ROSCODE1.BIN` 9152 bytes).
* Grep for privileged instructions in assembly: only `setjmp.s:42` affects
  user code (`stmpx.s` and `rs2s.s` run in supervisor mode). No C or other
  library sources were checked yet.
* Spec updated: sections 3, 4, 5.1, 5.5, 6.2, the new 6.3 and the 9 file plan.

## Follow-up 6

Committed the rosco files as `b2fd80e` (the user's host-build Makefile edits
and `bin/aal` were left out). M2 started: reading the machine layer
(`stmpx.s`, `klib68k.s`, `stmain.c`, `stcon.c`, `proto.h`, `clock.c`,
`tty.c`, `table.c`).

User:
> intermetzzo i made all branches of the rosco-m68k available so you can
> switch to the proper version

Compared releases 2.1, 2.2, 2.4 and 2.42 and develop. All agree on what the
port depends on: KRAM at 0x1800, load address 0x40000, `/ROSCODE1.BIN`, IVR
0x45, SDB/EFP addresses, DUART at 0xF00001. Switched `../rosco_m68k` to
`release/version-2.42`, the latest release. Spec section 6.3 updated.

## Follow-up 7: M2

Wrote the rosco machine layer and its GNU build (spec section 9 file plan):

* `src/kernel/`: `roscompx.S`, `roscklib.S`, `roscmain.c`, `roscons.c`,
  `roscstub.c`, `rosco.h`;
* `src/rosco/kernel/`: `Makefile`, `kernel.ld`, and the `staddr.h`/`stmfp.h`
  stand-ins for the build-time copy of `clock.c`;
* `src/rosco/include/stdarg.h`.

Config changes: `_WORD_SIZE` is also defined for gcc, and
`OPTIMIZE_FOR_SPEED` is now 0 because of an out-of-bounds read in `tty.c`
with `NR_PTYS 0`. `roscaddr.h` constants now work from assembler too.

Result in `r68k -d`:

* the banner prints;
* the 60 Hz clock is confirmed (uptime 120 → 240 ticks over 2 s, from the
  PF1 debug dump);
* console input is interrupt-driven and echoed;
* all tasks idle in `receive`.

The CPU type is taken from the firmware's `SDB_CPUINFO`. r68k reports a
68020 because of an emulator quirk.

Not committed yet. Not run on hardware.

## Follow-up 8: M3

M2 committed as `87964c0` (branch `rosco-m68k`). User asked which branch is
checked out: `rosco-m68k` here, `release/version-2.42` in `../rosco_m68k`,
`main` in `../r68k`.

M3 work:

* New `src/kernel/roscrs232.c`: the `rs232.c` logic on both DUART
  channels. The console is line 0, on channel A; `/dev/tty00` is line 1,
  on channel B.
* `roscons.c` now only does polled `putk` (made atomic against the TX
  interrupt), the debug keys (PF4 = test line on tty00) and the stubs.
* The temporary `rs_init` in `roscstub.c` is gone.
* `roscduart.h`: clear-extended-rate commands added, and the RTS/CTS
  wiring is now confirmed from the r2 schematic (CTS has no pull resistor,
  so the console ignores CTS).

Result in `r68k -d`:

* the banner and the clock work;
* echo and editing (INTR, KILL) work through interrupt-driven output;
* the tty00 test line comes out on channel B with CR LF.

`stty` waits for M4 (needs MM/FS). Not committed yet. Not run on hardware.

## Follow-up 9: M4 started; console speed

M3 committed as `bd2a449`.

User:
> just to be sure , you keep a running log of the changes yoy make for the
> changelog later ?

Yes: this log (one entry per step), the spec's file plan and results
sections, and one commit per milestone. Also saved as a memory for future
sessions.

User:
> the current hardware board has a defaut firmware with 38400 baud on
> serial 1

* The kernel used to force 115200 on channel A, which would garble the
  console on that board after the loader banner.
* `ROSCO_CONS_BAUD` (rosco `config.h`) now defaults to 38400 and is used.
* `roscrs232.c` has a new `rs_cons_setup()` (called from `initrosco`). It
  programs channel A from the shared speed table, which now also holds the
  numeric baud rates; the 38400 entry still needs checking on the board.
* The console line reports and keeps that speed.
* r68k run still OK (it ignores baud rates).

M4 analysis so far:

* **Executable format:** 32-byte a.out header (`A_M68K`, `A_EXEC`), text
  and data linked at 0, symbols, then a GEMDOS relocation stream.
* **Boot image (`build.c`):** each program is relocated to its absolute
  address and padded to a click; `sizes[]` holds (0, total clicks) per
  program; FS data starts with `0xDADA`, then INIT's origin and size.
* **Root device:** a rosco disk task, hooked in as `acsi_task`
  (`NR_ACSI_DRIVES 1`, `c0=ACSI`), will serve the appended filesystem image
  from RAM now and the SD card in M5.
* **Memory:** the kernel moves that image to the top of RAM, out of MM's
  way.

## Follow-up 10: M4, Minix boots to a shell (r68k)

New in this step:

* **Library** (`src/rosco/lib`):
  * `libc.a`, `libsys.a` and `crtso.o`, built from `src/lib` with gcc;
  * GNU versions of the ACK assembly runtime: `crtso`, `sndrec`,
    `setjmp`/`longjmp` (with `move ccr`), the `__sigreturn` trampoline,
    `_brksize`;
  * the mul/div helpers, moved here from `roscklib.S` and shared with the
    kernel;
  * the 97 syscall stubs, generated from `lib/syscall/*.s`.
* **Tools** (`src/rosco/tools`):
  * `prog.ld`;
  * `mkminix.py`, an ELF to Minix a.out converter with GEMDOS
    relocations;
  * `mkimage.py`, the boot image builder (what `build.c` does);
  * `mkfs.py`, a big-endian Minix V2 file system builder;
  * `stacks.py`.
* **Builds:** `src/rosco/boot` (MM, FS, INIT); `src/rosco/cmds` (`sh` and
  20 simple commands); the top-level `src/rosco/Makefile`.
* **Root filesystem:** `src/rosco/rootfs` (`proto`, `etc/rc`,
  `etc/ttytab`, ...).
* **Kernel:**
  * `roscdisk.c`, the disk driver registered as `acsi_task`, which serves
    the appended root image;
  * `place_image()` moves the image to the top of RAM and keeps it out of
    MM's memory;
  * `rootdev=DEV_HD0`, `c0=ACSI`;
  * `panic()` now dumps the process table;
  * the sizes magic is now `rosSIZES` (the 4-byte `SIZE` also occurred in
    strings).
* **Config:** `NR_ACSI_DRIVES 1`.
* **Headers:**
  * the rosco `<stdarg.h>` changed (char* `va_list`,
    `__builtin_va_start`);
  * new rosco `<string.h>` (renames the 16-bit mem* functions).

Bugs found and fixed along the way:

* **gcc's struct copies call `memcpy` with a 32-bit length;** the Minix
  `memcpy` read 16 bits and copied nothing. Symptom: MM panic "MM can't
  reply to 3" on the first fork. Found by temporarily instrumenting
  `do_fork` and reading the generated code.
* **mkminix used section offsets for relocations in an executable,** where
  they are already addresses.
* **mkminix stack sizes:** the "kw" suffix was not parsed.
* **Stale library objects:** objects built before the `-D_MINIX` change
  had to be rebuilt.

Result in `r68k -d`:

* `/etc/rc` runs and a shell prompt appears on the console;
* tested: `ls`, `cat`, pipes, redirection, `mkdir`, `stty` (38400), `df`,
  `date` (1970, no RTC), `sleep`, `cd`/`pwd`.

Not committed yet. Not run on hardware.

## Follow-up 11: M5, SD card storage and root on SD (r68k)

M4 committed as `b56e2af`.

New and changed:

* **Kernel:**
  * `roscdisk.c` reworked: `c0d0` is the SD card through the firmware
    EFP SD routines (whole card plus the four MBR primary partitions,
    parsed in the driver); `c0d1` (minor 5) is the RAM root image;
    `sd_root_minor()` finds the first Minix partition;
  * `roscklib.S`: `sd_efp_init/read/write` call the firmware with its
    register convention;
  * `roscmain.c`: `rootdev` is `c0d1` with an image, else the SD
    partition.
* **`src/rosco/lib/u64.c`:** replaces `lib/m68000/misc/u64.c`. Its
  `cmp64u()` was wrong (operator precedence), and `mul64u()` was
  truncated.
* **Tools:** `tools/mkfat.py` (FAT16 writer), and `tools/mksd.py` now has
  typed partitions.
* **Makefile:** `make sdcard` builds `rosco_sd.bin` (p0 FAT with
  `ROSCODE1.BIN`, p1 Minix root); `make runsd` boots with the root on SD.
* **Root filesystem:** device nodes `c0d0p0..p3` and `c0d1`, and `/mnt`.
* **Emulator patch:**
  * the ROM memory probe no longer overwrites the loaded program;
  * debugging aids: SIGUSR1 register/disassembly dump with a RAM
    snapshot, `R68K_TRACE`, `R68K_WATCH`, `R68K_DUMPAT`.

Debugging story:

* After adding device nodes, boot crashed with the kernel overwritten.
* Bisecting showed the inode count (filesystem layout) and the kernel size
  were only triggers.
* The new emulator tools showed `/bin/sh`'s `getc` recursing forever and
  its stack running over the kernel.
* That was caused by four bytes of `sh` reading `0x12345678`: written into
  the loaded file by r68k's ROM RAM probe at 0x80000. This is an emulator
  artifact.
* Separately, `mount /dev/c0d0p0` failed with EINVAL because of the
  `cmp64u` bug.

Result in `r68k -d`:

* mount, read, write, umount and remount of SD partitions work, and data
  persists;
* boot with the root on SD works, with 816 KB free for processes.

Not done in M5: card size, subpartitions, swap, native SPI driver. Not
committed yet. Not run on hardware.

## Follow-up 12: M6 started

M5 committed as `7317275`.

M6 per spec: tuning, expansion RAM, `rs2s.s` fast path, the `move sr` fix,
the `src/test` suite; done when the tests pass and the system is usable
multi-user.

Done so far:

* **Commands:** 77 now (`cmds/Makefile`).
  * New: getty, login, passwd, su, ps (`sttools/ps.c`), grep, sed, ed,
    find, od, test, expr, sort, ... plus the tools the shell tests use.
  * `rm`/`mv`/`ln`/`cpdir`/`clone` are hard links to `cp`, and `[` to
    `test`.
  * `grep.c` is compiled with `-Dconst=` (K&R definition against a
    `const` prototype).
  * `doscan.c` is built with `-DNOFLOAT` (there is no soft-float runtime).
* **Test suite build** (`src/rosco/test`): `test1..40`, `t10a`, `t11a`,
  `t11b`, plus a new `troscorun` that tests the gcc runtime helpers,
  struct-copy `memcpy`, varargs, and the kernel's `move sr` emulation.
* **SD root:** `rootfs/proto` plus the new `rootfs/sdproto`, holding all
  commands and `/usr/test`; `mkfs.py` now takes several spec files.
* **Clock:** `/etc/rc` sets it from `/etc/stamp`, the image build time
  (there is no RTC). Without this, test2 failed on a 1970 date.
* **User:** `ast` added.
* **Bug: `__divsi3`/`__modsi3`** (`lib/m68kmul.S`). After `move.l d0,a1`
  the sign test used stale flags (a move to an address register doesn't
  set them), so a negative dividend with a positive divisor gave a huge
  quotient. Symptom: the clock read back 2^32/60 seconds off after `date`.
* **Kernel `prv_emulate()`:** user-mode `move sr,<ea>` (68000 code, ACK
  binaries' `setjmp`) is emulated for Dn, (An), (An)+, -(An), d16(An),
  abs.W and abs.L.
* **Multi-user setup:** getty on the console and on tty00; bring-up
  accounts without passwords.
* **Expansion RAM:** the firmware's probe covers contiguous expansion RAM
  up to 14 MB, and the loader passes it to the kernel, so nothing to
  change.

First test run, as `ast` via `su` (RAM-limited emulator, slow):

* troscorun, 1, 2, 4, 5, 6: ok;
* 3: fails only because there is no `LOGNAME`/login session (environment,
  needs login);
* 7: the FD_CLOEXEC child re-runs the whole test (being investigated);
* 8 (signals): very slow or hanging (being investigated).

## Follow-up 13: M6 continued; full-hardware emulator

User:
> fyi i am building the mame emulator that emulates the whole hardware, i
> just have to figure out how we can load the data there.
> there is also a docker image: docker pull solderdemon/rosco-emulator
> emulator code is at ../rosco-emulator
> currently building

* **`../rosco-emulator`** (MAME 0.289 cut down to the rosco machines,
  `rosco_m68k_010`):
  * full DUART, bit-banged SPI SD card, IDE, bus errors on unmapped
    addresses;
  * an SD image goes on `-hard1`;
  * the serial ports default to 115200.
* **How to run Minix there:** `make CONSBAUD=115200 sdcard`, then
  `./rosco rosco_m68k_010 -hard1 rosco_sd.bin -terminal pty`. The firmware
  boots `ROSCODE1.BIN` from the card's FAT partition, and the kernel takes
  the root from the Minix partition.
* **New make option `CONSBAUD`:** the console speed, default 38400 (target
  board).

Test-suite work:

* **Tests built with `-O0`.** test8 spun forever in
  `while (x == 0);`: `x` is not `volatile`, and gcc -O2 keeps it in a
  register, so the store in the signal handler is never seen. The tests
  were written for ACK, which doesn't do this. Library and commands stay
  `-O2`; loops that call functions are fine.
* **The library built files the ACK build doesn't.** My wildcard picked
  up `posix/_exec.c`/`_execn.c` (old exec versions whose `execl()` passes
  `&arg2` as argv) and `other/execlp.c`/`v8regerror.c`. The wrong `_execl`
  won in the archive, so execl lost argv[0]. Symptoms: getty's
  `execl(login, login, name)` lost the name, and test7's FD_CLOEXEC child
  re-ran the whole test. Now excluded, matching `makefile.all`.
* **New test `texecl`:** execl directly and after fork.
* **Login:** getty starts `/usr/bin/login` (now a link); login checks
  passwords through `crypt()`, which runs `/usr/lib/pwdauth` (now built,
  setuid root); the bring-up accounts have no passwords. Login as `ast`
  works, with LOGNAME/HOME set (test3 needs those).
* **`move sr` emulation verified by `troscorun`:** the user SR shows IPL 2,
  because Minix runs user processes at SPL2, which is correct.

## Follow-up 14: M6 test results

User:
> ok sorry, i added more cpu and mem to your host now that mame is also
> running there, leas continue
> the mame build is done, try it there
> ok, let me know when the tests are done

* **MAME boot:** `make CONSBAUD=115200` card image, firmware loads
  `ROSCODE1.BIN` from FAT, root on the SD Minix partition, 68010 detected,
  login works. Scripted runs with the new `tools/mamesession.py`;
  `make runmame` for interactive use.
* **First MAME suite run:** 1–19 ok except test10. test10 failed on
  `cp`'s 31744-byte stack buffer: the stack is now 24kw instead of 16kw.
* **r68k suite** (before the fixes): 36/40, failing 10, 14, 27 and 28.
  * test27: `/dev/ram` must be a block device.
  * test28: libc `system()` reaped other children; now fixed in the rosco
    `lib/system.c` with `waitpid()`.
* **Second MAME run:** 37/40, failing 14, 29 and 33.
  * test29 and test33 hit their alarms: exec from SD was too slow with a
    48-block buffer cache, now 128 blocks.
  * test14 builds file names from the pid, and they contain `/` when
    pid & 31 == 14.
* **Final MAME run: 39/40, only test14.** test17 printed one diagnostic.
* **Root fs additions:** `true`/`false` (`commands/scripts/true.sh`) and
  `arch` (a link to `uname`), all used by the shell tests.
* **`CONSBAUD`:** a change now rebuilds the kernel objects that depend on
  it (`kernel/consbaud.stamp`).

User asked to commit M6, then look at test17 and uname.

## Follow-up 15: test17 and uname

M6 committed as `67db42e`.

* **test17's "cannot stat f7":**
  * Seen once in the full MAME run; not reproduced in 12 more runs
    (test17 ×5, and tests 14–17 ×3 on the card state after the suite).
  * Ruled out: the firmware SD block cache (`BBSD_read_block` always
    reads the card), the SPI pins (atomic OPR set/reset), task
    preemption, the driver's sector loop.
  * Recorded as an intermittent anomaly in the spec.
* **uname:** new `src/rosco/lib/_uname.c` (replaces
  `lib/posix/_uname.c` in the rosco libc). Machine `rosco_m68k`; arch
  stays `m68000`, because scripts use it to pick the CPU family. New
  `/etc/hostname.file` (`rosco`). Result: `uname -a` gives
  `Minix rosco 2 0.4 rosco_m68k`, and login shows `rosco login:`.
* **Shell tests:** with `true`/`false`/`arch` added, the `[` errors are
  gone; `crc` added. `sh1`/`sh2` still need `ar`, `make`, `cc` (no
  compiler on the target).
* **Default card rebuilt** at 38400 baud; checked in r68k.

Not committed yet.

## Follow-up 16: ar, make (and patch, diff)

Committed `5e8e21b` (uname, hostname, crc).

User:
> commit this first, then port ar and make

* **`cmds/Makefile` builds multi-file programs:**
  * `aal` (the ACK archiver, 16 files, 64k stack), installed as
    `/usr/bin/aal` with `ar` as a link;
  * `make` (8 files, 96k stack);
  * `patch` (5 files, 32kw stack), needed by sh1's cdiff test;
  * `diff` (single file), which `cdiff` runs through `popen()`.
* **Bug:** `aal` said "Out of core". `archiver.c` declares
  `char *malloc();` without a prototype, so
  `malloc(512 * sizeof(struct ranlib))` passed a 32-bit `sizeof` (gcc's
  `size_t`) where malloc reads 16 bits, and malloc got 0. New
  `include/rosco_sizeof.h` makes `sizeof(...)` an unsigned int as with ACK;
  it is used via `-include` for aal, make and patch.
* **Root fs:** `/usr/tmp` (1777, ar's temporary files); `/usr/bin/cp` and
  `/usr/bin/cat` as links (sh1 archives them).
* **Result in MAME:**
  * ar: create, list, extract and verbose list work;
  * make: builds, sees "up to date", rebuilds after touch;
  * sh1 ok; sh2 fails only on what needs `cc` (no compiler on the target).
* **Default card rebuilt** (38400).

Not committed yet.

## Follow-up 17: rosco_sizeof.h everywhere

Committed `fbba4cf` (ar/aal, make, patch, diff).

User:
> commit this first, then apply rosco_sizeof.h everywhere

* **`include/rosco_sizeof.h`:** `sizeof(x)` becomes
  `(unsigned) (sizeof(x) + 0 * sizeof(char [sizeof(x) <= 0xFFFF ? 1 : -1]))`.
  It has the ACK type (16-bit unsigned int), and an object of 64K or more
  is a compile error ("size of unnamed array is negative") instead of a
  silently truncated size.
* **Applied** via `-include $(ri)/rosco_sizeof.h` in the CFLAGS of
  `kernel/`, `lib/`, `boot/`, `cmds/` and `test/Makefile`. The per-rule
  includes for aal/make/patch were removed. A clean rebuild of everything
  gave no errors.
* **Result in MAME (full suite):**
  * 39 passed, 1 failed (test14, the known pid artifact);
  * troscorun and texecl pass.
* **sh1 in the full run:** 3× "compress: program too big" and "Error on
  uue/uud test 3".
  * Not a regression. suite3.log from before the change shows the same
    lines, and sh1 run alone passes ("Shell test 1 ok"). Compress and
    uncompress by hand also work.
  * Cause: exec ENOMEM. compress needs 470K contiguous out of 733K free,
    and memory is fragmented after the 40 tests. Recorded in the spec
    under "Known and not fixed".
* **Default card rebuilt** (38400) and checked in r68k.

Not committed yet.

## Follow-up 18: first boot on the real board, console at 19200

Committed `3e713ba` (rosco_sizeof.h). The user committed the rest
(`8a7b10f`).

User:
> ok i tested the image, one issue after the minix image loads it switches
> to 19200, i lgged in as root (no pass that is fine) ls and du both work
> fins , great progress

* **First run on the Classic v2 board:** `rosco_sd.bin` boots from the SD
  card, root login works, and `ls` and `du` work.
* **Bug:** the firmware and loader print at 38400, but the Minix console
  ran at 19200.
  * The kernel selected 38400 as CSR code C with the XR extended bit under
    rate set 2 (ACR[7] = 1).
  * MAME's XR68C681 table gives 38400 for that. The real chip gave 19200,
    the set 2 value without the extended bit.
  * The CR command sequence is the same as the firmware's for 115200.
* **Fix:** use rate set 1 (ACR[7] = 0).
  * 38400 is then the standard MC68681 code C, without the extended bit.
  * New speed table in `roscrs232.c` (set 1, X = 0/1 from MAME's table);
    `ACR_BRG1` in `roscduart.h`; `initrosco()` writes `ACR_BRG1`.
  * The unused `CSR_*` constants were removed.
  * 115200 (code 8, extended) retested in MAME: login, `stty`, `ls`.
* **Default card rebuilt** (38400) and checked in r68k.
Committed `8de1d42`.

* **Board test (user):** "console stays at 38400 now, works on the board".
* **Write test on the board (user):** a new empty file, `sync` and a reset;
  the file is still there.
* **Test suite on the board (user, as root, partial):** tests 1, 2, 4, 5,
  7–10, 12–16, 22–24, 26, 27 ok. 3, 6, 11, 17–21 and 25 refuse to run as
  root; run them as `ast`. Rest of the run: 28–32 and 34–40 ok; 33 needs
  a non-root login too. Score 30 passed, 10 failed, all 10 being the
  "as root" refusals, so no real failure. Shell tests as in MAME: sh1
  "compress: program too big" (memory) and uue/uud test 3; sh2 only the
  cc parts. `texecl` (fork and direct) and `troscorun` ok on the board.
* **`clr: Unknown terminal`** (from `run`): there was no `/etc/termcap`.
  * `rootfs/proto` now installs the Minix `src/etc/termcap`.
  * `etc/ttytab` gives the console type `vt100` instead of `minix`: the
    console is a serial terminal (emulator).
  * Checked in MAME: `TERM=vt100`, and `clr` sends the VT100 clear
    sequence.
* **Hang on logout (board):** after the root test run, `exit` gave no
  response and no new prompt; the board needed a reset. Not reproduced in
  MAME (repeated login and `exit` as root and ast, also after tests).
  * Suspect: on the last close, tty.c resets the termios and the driver
    reprogrammed MR1/MR2, the extended-rate bits and the CSR as soon as its
    output buffer was empty, while the transmitter could still be sending
    the last characters. MAME does not care about that; a real XR68C681
    may.
  * Change in `roscrs232.c`: `rs_config()` remembers what it last
    programmed (`mr1`, `mr2`, `speed`) and only writes what changes, so the
    console (fixed speed and format) is not touched at all. Before a change
    it waits for TXEMT (bounded, in case CTS holds the transmitter).
  * MAME: `stty 9600` on the console, tests 1 and 5, and three logins with
    `exit`, all fine; the console stays at 115200.
  * To check on the board: `exit` brings back the login prompt.
* **Shell tests as ast on the board:** sh1 ok (it failed only after the
  40 tests, memory). sh2: only the cc parts, plus
  `df: /dev/c0d0p1: Permission denied`.
  * The original install has df, mount and umount setuid root (`install -o
    root -m 4755`, commands/simple/Makefile); the rosco protos had 755.
    `ps` (reads /dev/kmem, "Permission denied" for ast) likewise.
  * Now 4755: `/bin/df`, `/bin/mount`, `/bin/umount` (proto) and `/bin/ps`
    (sdproto). MAME as ast: `df` and `ps` work.
* **4 MB expansion not used:** the board has the 4 MB RAM expansion. The
  firmware banner says "MC68010 CPU @ 10.0MHz with 5242880 bytes RAM",
  but the kernel says "1024kB RAM" and MM "Memory size = 1024K".
  * The chain looks right on paper: the firmware stores the size of the
    first contiguous RAM block at SDB 0x414 (`INITMEMCOUNT`, bootstrap.asm),
    the loader reads it if the SDB magic 0xB105D47A is there (else 1 MB),
    and passes it in d1 with d0 = 'ROSC'; the kernel uses it (else 1 MB).
    Clicks are 256 bytes and 16 bits, so up to 16 MB fits.
  * Diagnostic: the loader now prints `RAM xxxxxxxx` (hex bytes from the
    SDB) or `RAM? xxxxxxxx` (SDB magic missing, default used). That shows
    whether the loader or the kernel loses the size.
  * User: "can we recompile one of the emulators with 2mb for example".
    r68k now takes `R68K_RAM` (e.g. `R68K_RAM=5M`, multiple of 512K, 512K
    to 14M; default 1M); `r68k-duart.patch` regenerated (checked against a
    fresh checkout of bac2219), documented in `build-r68k.sh`.
  * r68k results: 2M gives `RAM 00200000`, "2048kB RAM", Available 1757K;
    5M gives `RAM 00500000`, "5120kB RAM", Available 4829K. With 5M,
    test1, sh1 and compress/uncompress of /bin/sh work. So loader, kernel
    and MM handle the larger size; on the board the size is lost before or
    in the loader's SDB read (the new `RAM` line will tell).
* **Default card rebuilt** (38400).

Not committed yet.

## Follow-up 19: MAME with firmware 2.42 and RAM expansion; the 2.42 SDB

Committed `22c8bde` (logout, termcap, setuid, loader RAM print, r68k RAM).

User:
> whet about mame does that run the 2.42 firmware ?
> yes do both, commit this first

* **MAME (../rosco-emulator), not yet committed there:**
  * Its ROM was 2.50.DEV; RAM was a fixed 1MB.
  * `src/rosco/drivers/rosco_m68k.cpp`:
    * a RAM slot (`-ram 2m` ... `-ram 14m`, default 1m); `machine_start()`
      maps the part above 1MB at 0x100000 over the bus error area (the
      on-board 1MB share is renamed `sysram`);
    * BIOS choice: `v250` (default, 2.50.DEV) and `v242`.
  * Firmware 2.42 built unmodified from `release/version-2.42` (scratch
    copy, roscopeco/rosco_m68k image): 43792 bytes, CRC32 86950afb, added
    as `rosco_m68k_242.rom` to the four rosco_m68k ROM zips. Documented in
    `README.md` and `roms/README.md`. `-verifyroms`: 4 of 4 good.
  * `tools/mamesession.py` takes `-a 'mame options'`.
* **Cause of the 1MB on the board:** firmware 2.42 has a small kernel whose
  `api_init()` (called from `main1()` after the banner) stores its API
  pointer at `KERNEL_API_ADDRESS` = 0x400, over the SDB magic. The loader
  then saw no valid SDB (MAME with `-bios v242 -ram 5m`:
  `RAM? 00100000, SDB magic 00001DF8`, 0x1DF8 being the firmware's `api`)
  and fell back to 1MB. The memory size at 0x414 is intact.
* **Fix in `loader/loader.S`:** without the magic, the SDB memory size is
  still used if it is plausible for the firmware's RAM count (a multiple of
  512K, 1MB to 14MB); otherwise 1MB as before. The loader prints
  `RAM xxxxxxxx` and, without the magic, `, SDB magic xxxxxxxx`.
* **MAME results:**

  | firmware | RAM | loader | Minix |
  |---|---|---|---|
  | 2.42 | 5m | `RAM 00500000, SDB magic 00001DF8` | 5120K, Available 4829K |
  | 2.42 | 1m | `RAM 00100000, SDB magic 00001DF8` | 1024K |
  | 2.50 | 5m | `RAM 00500000` | 5120K |
  | 2.50 | 1m | `RAM 00100000` | 1024K |

* **Full suite in MAME, firmware 2.42, 5MB, as ast:** troscorun and texecl
  ok; Score 39 passed, 1 failed (test14, the pid artifact); sh1 ok, now
  also within the full run (compress has room with 4.8MB free); sh2 only
  the cc parts; `exit` brings back the login prompt.
* **Default card rebuilt** (38400); r68k with `R68K_RAM=1M`/`5M` shows
  1024K/5120K.
* **Board, full suite as ast (card from before the termcap, setuid and
  loader fixes):** "All 40 tests completed without error." Shell tests:
  sh1 "compress: program too big" (1MB seen, see above), sh2 the cc parts
  and `df: Permission denied`; plus `clr: Unknown terminal`. All three
  are fixed on the new card.

Not committed yet.

## Follow-up 20: new card on the board

Committed `680eca7` (loader, mamesession -a); emulator branch
`rosco-m68k-ram-bios` (`de3ef70395b`) in ../rosco-emulator.

* **Memory:** "Memory size = 5120K   MINIX = 292K   RAM disk = 0K
  Available = 4829K", the full 5MB with the expansion, firmware 2.42.
* **Logout:** `exit` brings back the login prompt (the `rs_config()`
  change in roscrs232.c).
* **clr:** the screen clear at the start of `run` works (termcap, vt100).
* **Interrupt:** the Minix interrupt key is DEL (`intr = ^?`); it works.
  Ctrl-C is not mapped (`stty intr '^C'` per session).
* **Shell tests:** sh1 ok on the board (also compress now). sh2 only the
  parts that need a C compiler (`cc` via make, `a.out`, `strings a.out`);
  `df` works as ast. Everything that can pass without a compiler passes.

Committed `fc51618`.

User:
> commit this first, then add the /etc/profile for ctrl-c

* **`rootfs/etc/profile`** (new, in `proto`, so on the RAM root and the SD
  root): `TZ` as in `src/etc/profile`, plus `stty intr '^C'`. sh reads it
  for login shells; the tty resets termios on the last close, so every
  login sets it again. DEL is then an ordinary character.
* **MAME:** ast and root get `intr = ^C`; Ctrl-C interrupts `sleep 60`
  (the `echo` after it does not run); `TZ=GMT0`.
* **Default card rebuilt** (38400).

Committed `978aea9`.

* **Board (user):** "ctrl-c works on the board".

Not committed yet.

## Follow-up 21: 64MB SD root

User:
> i cannot currently test the other tty ; first do 3 and then 2

(3 = a bigger SD root, 2 = a C compiler on the target.)

* **Limits checked:** `block_t`, `zone_t` and `bit_t` are 32 bits; the FS
  has no fixed bitmap slots (bitmap blocks go through the cache); the SD
  driver keeps positions in 32/64 bits. Only `ino_t` is 16 bits (at most
  65535 i-nodes).
* **Makefile:** `SDROOTBLOCKS = 65536` (64MB), `SDROOTINODES = 8192`
  (mkfs makes 8207, a whole number of i-node blocks). `rosco_sd.bin` is
  now 73MB (1MB + 8MB FAT + 64MB Minix).
* **Fill test, MAME, firmware 2.42, 5MB:** 34 copies of a 1MB file
  (`cp`), all compared ok (`cmp`); `df`: 39848 of 65536 blocks used, so
  well past block 32768 (where 16-bit block or sector numbers would wrap).
  Writing 34MB took about 50 minutes in unthrottled MAME, comparing too.
* **fsck** then reported "zone 65536 is missing" ... and "inode 8208 is
  missing" ...: exactly the bitmap bits beyond the last zone and i-node,
  which `tools/mkfs.py` set. Minix mkfs leaves them clear, fsck expects
  them clear, and the FS never looks past them (`alloc_bit()` stops at
  `map_bits`). `mkfs.py` now leaves them clear (also for the RAM root).
  This was already so on the 8MB card; fsck had not been run before.
* **ACK** (user: the old Minix site has the 16-bit ACK ANSI compilers):
  http://download.minix3.org/previous-versions/ANSI-compilers/ has only
  binaries (plus the `ncc.c` driver), the 32-bit directory is empty, and
  the binaries are i8086 (a.out CPU byte 0x04, code `89 e3 8b 0f`). Not
  usable on the 68010.
* **Rerun with the fixed mkfs.py (MAME, 2.42, 5MB):**
  * fsck on the new card: no errors (62975 free zones);
  * fill with 34 x 1MB (51 minutes), `df` 39844 used; fsck: no errors;
  * after a restart: `/usr/x34` (the highest blocks) compares ok; after
    `rm /usr/x*` `df` shows 4858 used, and fsck: no errors, free zones
    matching `df`.
* **Default card rebuilt** (38400), 73MB.

Not committed yet.

Committed `1a4a11a`.

## Follow-up 22: compiler plan; Minix in the flash ROM (note)

User:
> commit this first, then go ahead with the plan; make a note if we cand
> use the unused space in the 1M flash rom to host parts of minix

* **ROM note:** spec section 11.1. Firmware 2.42 uses 43792 bytes of the
  1MB ROM; ideas: the boot image in ROM (no SD load), a read-only ROM
  disk as another disk minor; execute in place not worth it. Open: how
  the ROMs are programmed, the even/odd split.
* **Compiler survey** (downloads in the scratchpad only, nothing imported):
  * c68 V4 on FUNET (`/minix/lang/c68/V4/src.tar.Z`) is damaged: it
    unpacks to five headers. V3 (+patch1, 1991) is intact: 16-bit int by
    default, ACK/gas/CP-M output, but no ANSI prototypes (4.0 added them,
    "to compile the ANSI'fied code of Minix").
  * xtc68 (codeberg.org/stronnag/xtc68, MIT for its changes): the last
    c68 code base (4.x), building on modern hosts; ACK output by default
    (`out68k_a.c`), `int=16`/`int=32`, plus `src/cpp` (old GNU cccp, GPL).
    c68 itself: "may be redistributed as long there is no commercial
    interest ... must not be redistributed without its full sources".
  * Sozobon 2.0 sources (sourceforge.net/projects/sozobon, "Source
    code"): hcc, top, jas, ld, ar, nm, size, cc, make. jas 1.2 has
    "changes for minix: input compatibility with ack"; ld built with
    `-DMINIX` writes the 32-byte Minix header with the stack size.
    License: free, no charge beyond reproduction, modified versions
    must be marked.
  * SozobonX on FUNET: binaries only. ACK: i8086 only (see above).
* **Plan:** c68 (ACK syntax, int=16) -> jas -> Sozobon ld (-DMINIX) ->
  Minix a.out; cpp from xtc68; the Minix libc compiled with c68 into
  Sozobon objects; a `cc` driver. First a host prototype (hello world in
  r68k/MAME), then the import as new files under `src/rosco/cc/`, then
  the target builds of the tools with m68k-elf-gcc.
* **Host prototype (scratchpad):**
  * xtc68 is now on Codeberg (user: "now on codeberg:
    https://codeberg.org/stronnag/xtc68"); the snapshot used is its HEAD,
    commit 206699274d70 (2026-06-28).
  * c68 4.5 from xtc68 builds with the host cc as is (`-DXTC68`,
    `src/include/xtc68.h`). `c68 -int=16 x.c x.s` writes ACK syntax with
    16-bit int.
  * Sozobon jas/ld: the sources have DOS line ends (converted to LF), and
    are K&R code that assumes a 32-bit `long` (written to the object and
    executable files). No `-m32` on this host or in the rosco image, so a
    force-included `host32.h`: the system headers the sources need
    (stdio, ctype, setjmp, fcntl, unistd, string), then `#define long
    int`, and `link`, `strnlen`, `bcopy` renamed (they clash with libc).
    Two K&R `lseek` declarations removed (marked "rosco:"). Built with
    -Wint-to-pointer-cast etc.: no pointer truncation reported.
  * jas needs `-DMINIX` for the ACK syntax (`.sect`, `!` comments).
  * ld `-DMINIX` wrote a_cpu 0x04 (i8086, the old Minix-ST tools; MM
    accepts it); changed to 0x0B (A_M68K) like our executables, marked.
  * c68 calls `.Xldiv`, `.Xlrem`, `.Xlmul` (and unsigned and assigning
    `.Xas*` forms) for long arithmetic, operands on the stack, the callee
    pops them, result in d0. Written in ACK syntax (`sup.s`, prototype).
  * Test: a crt0 and sendrec in ACK syntax, write()/_exit()/strlen() in C,
    and a hello program, all through c68 -> jas -> ld (=8192 stack); put
    on a MAME card with mkfs.py. Output in MAME (2.42, 5MB):
    "hello from c68 on rosco", "0300007" (long arithmetic), exit status 7.

Not committed yet.

Committed `b4f7a35`.

## Follow-up 23: compiler step 1, sources imported

User:
> commit this first, then go ahead with step 1

* **`src/rosco/cc/`** (new):
  * `c68/`: xtc68 `src/c68` complete (c68 must not be redistributed
    without its full sources, so also the other code generators) and
    `src/include/xtc68.h`, `LICENSE.xtc68`; unchanged.
  * `cpp/`: xtc68 `src/cpp` (GNU cccp, GPL), unchanged, not built yet.
  * `sozobon/`: Sozobon 2.0 `jas`, `ld`, `tools`, `readme.src`, LF line
    ends, `host32.h` (new), the changes from the prototype; the three
    changed files (`ld/pass2.c`, `ld/utls.c`, `tools/ar.c`) carry a
    MODIFIED note as the Sozobon license asks.
  * `README`: provenance (xtc68 commit 206699274d70, the SourceForge
    archive), licenses, the list of changes, the support routine
    convention.
  * `Makefile`: host builds into `host/` (c68, jas -DMINIX, ld -DMINIX,
    ar); `cc/host/` added to `src/rosco/.gitignore`.
* **Check:** clean build, no errors; the test program from the prototype
  compiled, assembled, put into an archive with `host/ar` and linked
  from it with `host/ld` runs in MAME ("hello from c68 on rosco",
  "0300007", exit 7).

Not committed yet.

## Follow-up 24: evaluating the modern ACK (minix68k)

User:
> is this helpful? https://github.com/davidgiven/ack has a minix minix68k
> produces Minix executables for m68000 Minix systems

* **ACK `plat/minix68k`:** `ARCH=m68k2`, `NAME=minixST`, `-D_ATARI_ST`,
  int 2 bytes, long and pointers 4, alignment 2; the ANSI C front end,
  optimizer, m68k2 code generator and assembler, and `cv` to the Minix
  a.out; its own Minix libraries. BSD license; last touched July 2025.
* **Host build** (scratchpad clone, HEAD 7afa32a, `PLATS = minix68k` in
  the Makefile, `make LUA=lua5.4`): needed Lua with lua-posix, flex,
  yacc, plus the interpreter `lua5.4`, `ninja` and `bison` (not in the
  README); the user installed them. Build time 48 s. `ack` finds its
  passes with `ACKDIR=<staging>` (the compiled-in prefix is
  /opt/pkg/ack).
* **Two ACK bugs found and fixed in the clone:**
  * `plat/minix68k/cv/cv.c`: the header is `long mh[8]`, written with
    `sizeof(mh)`, so on a 64-bit host it was 64 bytes (each field padded
    to 8). Now 32 big endian bytes.
  * `mach/m68020/ncg/mach.c` `con_mult()`: always printed `.data8`. With
    `WORD_SIZE=2` (m68k2) it is also called for 4-byte constants, so every
    initialized `long` took 8 bytes and shifted the data behind it. Now
    `.data4`/`.data8` by size. (Not a host problem: a bug in the current
    ACK for this target.)
* **Result in MAME (2.42, 5MB):** a test with stdio, qsort, struct
  initializers (auto and static), long multiply and divide, argv: all
  correct; `sizeof(int)=2`, exit status passed. The executable has a_cpu
  0x04 like the old Minix-ST tools (MM accepts it).

Not committed yet.

Committed `b61212e` (c68/Sozobon import, kept as a fallback).

## Follow-up 25: switch to ACK

User:
> commit this first, then switch to ACK

* **`src/rosco/ack/`** (new), as for r68k: no ACK sources in the repo.
  * `build-ack.sh`: clones https://github.com/davidgiven/ack at the pinned
    commit 7afa32a0a0f1 (2026-07-18) into `src/rosco/ack/ack/`
    (git-ignored), applies `ack-minix68k.patch` and runs `make LUA=lua5.4`
    (`ACK_URL=` for a local clone). Lists the host tools it needs.
  * `ack-minix68k.patch`: PLATS/DEFAULT_PLATFORM minix68k in the
    Makefile, and the two fixes (ncg `con_mult()`, cv header).
  * `acc`: `ack -mminix68k` with `ACKDIR` set to the staging tree.
* **Check:** a clean build with the script (from the local clone) and the
  two test programs compiled with `acc`: correct output in MAME (2.42,
  5MB). The binary differs slightly from the one of the evaluation build,
  which was rebuilt incrementally across the fixes; the clean one is the
  reference.

Not committed yet.

Committed `ccc531e`.

## Follow-up 26: the Minix C library built with ACK

User:
> commit this first, then do step 1

* **ACK's own libc does not fit:** `plat/minix68k/libsys` is the Minix
  1.x system call layer (`_gtty`/`_stty`, old `signal`, no `sigaction`,
  `waitpid`, termios). The commands here and our MM/FS are Minix 2.0.4.
  So the library is this tree's `src/lib`, built with ACK as the ST port
  did; its system call stubs and the 68000 run-time are in ACK syntax.
* **ACK predefines** (minix68k): `__STDC__`, `_EM_WSIZE/PSIZE/LSIZE/
  FSIZE`, `__unix`, `_ATARI_ST`, `m68k2`, `__m68k2`; not `__ACK__`,
  `__minix`, `__m68000`, so those are passed with -D (plus `_MINIX`,
  `_POSIX_SOURCE`). The rosco `<minix/config.h>` already handles ACK; the
  rosco `string.h`/`stdarg.h` are for gcc, so only a copy of config.h
  goes in front of `src/include`.
* **New `src/rosco/ack/lib/`:**
  * `Makefile`: `crtso.o` (lib/m68000/rts/ncrtso.s), `emrts.o`, and
    `libc.a` (aal) from ansi, posix, other, stdio (same exclusions as the
    gcc libc, same rosco replacements u64/regexp/system/_uname), the
    syscall stubs and the rts files (incl. initlib). About 7 s.
  * `emrts.s`: the EM symbols ACK's `libem.a` expects from ACK's own
    start-off (`hol0`, `.lino`, `.filn`, `.reghp`, `.limhp`, `.ignmask`,
    the error numbers, `BRK`/`WRITE` jumping to `_brk`/`_write`).
* **New `src/rosco/ack/acc-minix`:** compile and link a Minix program:
  `crtso.o emrts.o files libc.a libem.a libend.a`; `-S stack` sets the
  a.out total like chmem (cv only takes `=N`, which `ack -R` cannot pass).
* **Results in MAME (2.42, 5MB):** cat, echo, ls, sort, wc from
  commands/simple built with acc-minix work like the gcc ones (pipes,
  `sort -r`, `sort +4n`, `ls -l`); much smaller (echo 1020 bytes, cat
  3744). The test suite (tests 1-40, t10a, t11a, t11b) builds without
  errors; run in /usr/acktest: see below.
* **ACK test suite in MAME (2.42, 5MB, ast):** "All 40 tests completed
  without error", sh1 ok, sh2 only the cc parts (no cc on the target yet).
* **The intermittent create/stat anomaly again:** during the run,
  "stat: cannot stat f4" and "creat: incorrect mode set while creating
  many files" (test18 `test02`: creat f0..f16, stat each right after the
  successful creat). Before: once in test17 with the gcc build (f7, same
  code). So not compiler related: now and then a file just created is not
  found by name by the following stat. Open; to investigate in FS, the
  buffer cache and the SD driver.

Not committed yet.

Committed `be411d9`.

## Follow-up 27: cc on Minix (the ACK passes as Minix programs)

User:
> ok continue

* **Approach:** the passes of a C compile (ack, cpp.ansi, em_cemcom.ansi,
  em_opt, ncg, as, em_led, cv, and aal) built with m68k-elf-gcc -mshort
  and the rosco gcc libc, like the other commands. The ACK sources are C89
  plus `<stdbool.h>`/`<stdint.h>`.
* **New `src/rosco/ack/mkpass.py`:** reads the host build's
  `.obj/build.ninja` (`ninja -t commands -s`, also the long commands kept
  in `rule-*.sh` files): for each program the link inputs, for every
  object (and the members of the module `.a`s) its source file (often
  generated by the host build) and -I/-D flags. The host's sandboxes only
  limit what a step can see, so the compiles run from the ACK root into
  `pass/obj` (no copy of the 556MB `.obj`). It writes `pass/build.sh`
  and runs it once in the toolchain container; links with crtso.o,
  `passlib.a` and the rosco libc, converts with mkminix.py, heap/stack
  per pass (em_cemcom.ansi 256k, ncg/em_opt/as/em_led 128k).
* **Added for the passes:** `passinc/stdint.h` (Minix has none; for the
  -mshort ABI), `passinc/passcompat.h` (forced include: declarations of
  the passlib functions), `passlib/strdup.c`, `passlib/di.c` (the 64 bit
  long long routines gcc calls; libgcc has no -mshort variant). The
  compiles use `-Werror=implicit-function-declaration
  -Werror=int-conversion`.
* **ack-minix68k.patch:** also `modules/src/system/maketempfile.c` for
  `__minix` (no `clock_gettime()`; seed from `time()` and `getpid()`, the
  small `rand_r()` the file has for Windows).
* **Bugs found on the way (MAME, 2.42, 5MB):**
  * `cpp.ansi` crashed when a `-D` macro was expanded: `strdup()` had no
    declaration, so gcc took its result as a 16-bit int and truncated the
    pointer. Hence passcompat.h and the -Werror flags (strdup was the only
    case).
  * Programs compiled on Minix printed garbage ("eell,,ccmmii..."): the
    target's front end, optimizer and code generator gave output identical
    to the host's (.m and .s compared); the object differed: every byte of
    a multi-byte value was its lowest byte. gcc passes the shift count of
    `__ashldi3`/`__ashrdi3`/`__lshrdi3` as a 32-bit word (libgcc's shift
    count mode) even with -mshort; di.c took an int, read the zero upper
    half and did not shift. Now a long count. (di.c's logic itself checked
    on the host against native 64-bit arithmetic, 200000 cases.)
  * Test scripts: `expect EXIT=` matched the echo of the typed command;
    markers are now split (`EX''IT`). The console takes at most 255
    characters per line, so long commands go into a script file.
* **On the target:** `cc.sh` as `/usr/bin/cc` (compile with the Minix
  headers, link crtso.o emrts.o files libc.a libem.a libend.a; -c, -o,
  -O, -D, -U, -I, -l); `/usr/ack` (passes, descr, libem/libend, the Minix
  libc in `minix/`); `/usr/include` (src/include with the rosco
  config.h). `src/rosco/ack/mkccproto.py` writes these proto lines.
* **emrts.s:** its zero words now come last. Before, the last byte of its
  data was the low byte of `__end` (here '<'), right before the program's
  data, and `strings a.out` printed "<MS-DOS: Just say no" (sh2).
* **Build:** `src/rosco/ack/Makefile` (host, lib, pass, ccproto); top
  level `make cc`; `make sdcard` adds ack/ccproto when it exists.
* **Results in MAME:** hello.c compiled on Minix runs ("41152263",
  exit 8); a program typed in with cat: "12! = 479001600"; **sh2: "Shell
  test 2 ok"** (make, cc, a.out, strings). So now tests 1-40, sh1 and sh2
  all pass.
* **Default card rebuilt** (38400) with cc.

Not committed yet.

Committed `f9f60de`.

* **Board (user):** "cc works on the board, sh2 ok". So on the real
  Classic v2 (68010, 5MB, firmware 2.42): tests 1-40, sh1 and sh2 pass,
  and programs are compiled on the board itself.
* **Intermezzo questions (no changes):**
  * Brandy 1.19 (BBC BASIC, GPL): portable with moderate work: its
    `int32` is `int` (needs `long` here, plus an audit of int/int32
    mixing), it needs double floating point and libm (soft-float libgcc
    or ACK), and `select()` for INKEY (use termios VMIN/VTIME).
  * Second SPI: no second port, but a second chip select: `SPICS2` on
    DUART OP7 goes to header J7; clock/MOSI/MISO are shared on J4 with the
    SD card. The firmware supports it (BBSPI_assert_cs1); the kernel leaves
    OP7 alone.

Not committed yet.

Committed `ea7fbf9`.

## Follow-up 28: status; the patches as branches in the sibling repos

User (among others):
> what is the current fastest serial speed we can realistically use
> ... i was thinking of using SLIP on a serial line at a later stage.
> ok what is the current status on minix
> apply the patch to ../r68k in a new branch / commit it on the branch
> ack is now at ../ack you can do the same on a new branch for your
> patches of ack
> update the log with the r68k and ack branches

* **Serial speed (answer, no changes):** 115200 is the hardware ceiling
  with the 3.6864 MHz DUART crystal (the counter/timer as clock source
  tops out there too). Output at 115200 is safe; input bursts at 115200
  are untested on the board (small RX FIFO, `overrun_errors` counted but
  not reported). Console speed must match the firmware (38400 on the
  board, 115200 with the stock 2.42 build); tty00 can be set with stty.
* **SLIP (noted for later):** Minix 2.0.4 has it all: `src/inet` with
  `psip`, `commands/simple/slip.c`, ifconfig, add_route, ping,
  telnet(d), ftp(d), rlogind. Needs ENABLE_NETWORKING, the INET server in
  the boot image, /dev/psip0, inet.conf; PC side slattach. The rosco
  config already reserves "SLIP over DUART B".
* **../r68k**, branch `rosco-minix-duart` from `main` at bac2219: commit
  a3d2f10, the content of `src/rosco/emu/r68k-duart.patch` (XR68C681
  model, non-destructive memory probe, R68K_RAM, debug aids). `main`
  unchanged; not pushed.
* **../ack**, branch `rosco-minix68k` from `default` at 7afa32a0a, the
  content of `src/rosco/ack/ack-minix68k.patch` as four commits. First
  made in a checkout of davidgiven/ack; the user then replaced ../ack
  with a checkout of the fork (git@github.com:maarten-boot/ack.git, same
  commit: "it should have been my fork") and the branch was made again
  there:
  * eaa025860 m68020 ncg: `con_mult()` emits `.data4` for 4-byte
    constants (for upstream);
  * 8625a2877 minix68k cv: 32-byte header on 64-bit hosts (for
    upstream);
  * 19b989ceb `sys_maketempfile()` for Minix (no clock_gettime);
  * dae8173bc Makefile: build only minix68k (local configuration, not
    for upstream).
  `default` unchanged; not pushed (push it to the fork from a machine
  with GitHub access: `git push -u origin rosco-minix68k`).
* Both committed as Maarten Boot <11140490+maarten-boot@users.noreply.
  github.com> (the repos have no identity of their own). The patch files
  in this repo stay the source for build-r68k.sh and build-ack.sh.

Not committed yet.

Committed `cdcf745`.

## Follow-up 29: the create/stat anomaly (in progress); tty00; editors

User:
> look at the create/stat anomaly
> can we remove the yellow color from the rosco bootup screen
> is the second serial by default on 9600 and it expects a password on
> login as it is not the console it seems ...
> we have no editor other than ed in minix i expect, ... minix 1 had mine

* **create/stat anomaly:**
  * A stress program (creat f0..f16 with modes n*0100, stat each, also
    with a child calling sync() nonstop): 5100 create/stat pairs, no
    failure. test17/test18 alone 30 times each, the full suite followed by
    cstress and test17/18 x5, and 10 more full rounds with instrumented
    test17/18 (stat wrapper printing errno): no failure. `ps` after the
    suite: no leftover processes (and no `update` daemon runs).
  * MAME runs are deterministic (each round took exactly 47 emulated
    minutes). Replaying the exact card of the ACK test run where it last
    happened (its unmodified root image was still in the scratchpad)
    reproduced it exactly: test18, "cannot stat f4". With only test18
    replaced by the instrumented one (changing the file layout) it was
    gone. So it depends on the disk/FS state, not on input timing.
  * `stat()` passes short names inside the message, so FS does not fetch
    the name from user memory.
  * Debugging: MAME `-debugscript` is ignored with `-debugger none`; the
    gdbstub had no `m68010` entry ("cpuname m68010 not found"): added in
    ../rosco-emulator (`src/osd/modules/debugger/debuggdbstub.cpp`, the
    m68000 register map; not committed there yet). A small gdb remote
    client (scratchpad, host gdb has no m68k) works (target.xml handshake,
    Z0 accepted), but the breakpoint on FS `reply()` (0x18498, checked in
    the image) does not fire yet. Open.
* **Yellow banner:** firmware, `SZ_BANNER0` = ESC[1;33m in
  `code/firmware/rosco_m68k_firmware/bootstrap.asm`; needs a firmware
  rebuild and flash (or a terminal that maps the colours).
* **tty00:** 9600 is the tty default (TSPEED_DEF); `/etc/ttytab` can run
  `stty` on the line (4th field, e.g. "stty 38400"). login treats only the
  console as secure (`securetty()`: minor 0-7) and refuses empty passwords
  elsewhere, so set passwords with `passwd` to log in on tty00.
* **Editors:** `mined` (commands/mined, 64k) and `elvis` (commands/elvis,
  18kw, the Minix-ST settings) added to `cmds/Makefile`; `/usr/bin/mined`,
  `/usr/bin/elvis` with links vi, ex, view (sdproto).
  * The gcc-built elvis quit at once (status 1) while the ACK-built one
    worked: `tio.c` calls `ttyread(..., sizeof keybuf - cend, ...)`;
    `sizeof keybuf` without parentheses is not caught by rosco_sizeof.h,
    so a 32-bit value went to the K&R ttyread(), which read a length of 0;
    read() returned 0 and elvis took it as the end of an ex script.
    `cmds/elvisproto.h` (forced into the elvis compile) gives prototypes
    for ttyread() and vgets(), so gcc converts the argument.
  * MAME: vi (insert, :wq) and mined (^W, ^X) write their files.
  * Other unparenthesised `sizeof` uses in the gcc-built commands go to
    prototyped functions or comparisons; a latent risk for unprototyped
    calls.
* **Default card rebuilt** (38400) with mined and elvis.

Not committed yet.

## Follow-up 30: four 150MB partitions; FAT tools; partition limits; ROM use

User:
> ok we currently have on partition of 73M but the disk has 4 partitions
> in /dev/ could we build a image with 4 partitions all 150M
> minix currently cannot read or write fat filesystems right ?
> what is the max partition size minix can handle
> how are you currently reading from the sd drive in minix ...
> any other rom routines you are using

* **Card layout** (Makefile): p0 FAT16 150MB (boot, file exchange), p1 the
  Minix root 150MB (153600 blocks, 16384 i-nodes), p2 and p3 empty Minix
  file systems 150MB (`SDEXTRA`, `rootfs/emptyproto`; leave SDEXTRA empty
  for two partitions). p0 must stay FAT: the firmware boots from it.
* **Tools:**
  * `mkfat.py`: picks the smallest cluster size (1..64 sectors) that
    keeps FAT16 within 65524 clusters (150MB: 8 sectors = 4kB, 38358
    clusters); total sectors above 65535 in the 32-bit BPB field; up to
    2GB.
  * `mkfat.py`, `mkfs.py`, `mksd.py` write sparse images; mksd.py streams
    the partitions instead of building the card in memory. rosco_sd.bin is
    601MB but takes about 4MB on disk; built in about 2 s.
  * Test in MAME (firmware 2.42 booting from 4kB clusters, root on 150MB,
    mounting p2/p3, blocks above 65535 with a pattern file, fsck): running
    at the time of this commit.
  * Found: the pattern file (71680000 bytes) is larger than a Minix V2
    file can be with 1k blocks (7 + 256 + 256^2 zones = 67383296 bytes);
    mkfs.py did not refuse it. To fix.
* **FAT on Minix (answer):** FS cannot mount FAT; user level tools are in
  the tree: `commands/m68000/tos.c` (Atari TOS disks, big endian),
  `commands/ibm/dosread.c` (dosread/doswrite/dosdir), mtools 3.9.7
  (`commands/i386`). Not built yet; large partitions need checking.
* **Limits (answer):** a file system up to about 2GB (device positions as
  block * 1024 in a signed 32-bit off_t), at most 65535 i-nodes, a file up
  to about 64MB (V2 inode, 1k blocks).
* **ROM use (answer):** only the firmware's SD routines are called (EFP
  0x44C/0x450/0x454, via `roscklib.S`); data read from the SDB (memory
  size 0x414, CPU info, which firmware 2.42 makes unusable, so the kernel
  probes the CPU); the firmware loads ROSCODE1.BIN; reboot jumps through
  the ROM reset vectors. Console, tty00, clock and interrupts drive the
  DUART directly; no TRAP calls.

Not committed yet.

Committed `d643368`.

## Follow-up 31: halt/reboot, more commands; the four partitions tested

User:
> is there alorady a halt or reboot command available (i saw reboot in
> the minix code) and can we add some of the other commands in the minix
> command tree to this image

* **halt, reboot, shutdown** (commands/reboot; halt.c acts by its name):
  `/usr/bin/halt` (+ link reboot), `/usr/bin/shutdown`. The kernel already
  had `wreboot()` (halt: "System halted"; reboot: `kreboot`).
  * `reboot` printed "Rebooting ..." and then nothing: the firmware detects
    the DUART by IVR == 0x0F (its reset value; we set 0x45) and writes
    MR1/MR2 assuming the MR pointers at MR1. `kreboot` (roscompx.S) now
    resets both channels (MR pointer, receiver, transmitter, errors,
    extended rate bits), sets IVR 0x0F, ACR 0 and stops the counter.
    MAME: reboot goes through the firmware banner and SD boot back to the
    login prompt.
* **More commands** (commands/simple): tar, xargs, printf, pr, split,
  join, rev, fold, expand, unexpand, file, cksum, time, chmem, strip,
  size, nm, install, mkproto, inodes, ncheck, dhrystone, gomoku, life,
  ttt, factor (in /bin). life and gomoku need curses: `libcurses.a` from
  lib/curses added to the rosco lib Makefile. 113 commands now.
  * MAME: tar c/t, xargs, factor 360 = 2 2 2 3 3 5, file, dhrystone 3280
    Dhrystones/s.
  * printf printed 0 for %d/%x: printf.c has `#define atoi(a)
    strtoul((a), NULL, 0)` and passes the unsigned long to printf("%d"),
    a 16-bit int here (same with ACK: a fault of the original source).
    `cmds/printfint.h` (forced into printf.c) makes strtoul() give an
    unsigned int there. Now "42|ff|10|    7|3   |ok".
  * Minix sh drops a backslash inside double quotes (`echo "q\nq"` gives
    qnq); single quotes keep it. Original Minix behaviour.
* **Four partitions, test in MAME (2.42, 5MB):** the firmware boots from
  the 150MB FAT (4kB clusters); root on c0d0p1 (150MB).
  * First run: p2 had a 71680000 byte pattern file, more than a V2 file
    can hold (65799 zones = 67378176 bytes); mkfs.py wrote it anyway and
    FS panicked at block 69999 ("Illegal zone number in indirect block").
    The blocks 65534..65537 of that file (physical ~66600) read correctly.
    mkfs.py now refuses files over the V2 maximum.
  * Second run (big1 60000 blocks + big2 15000 blocks, so big2 lies at
    physical ~61000..76000): reads across 65535 correct; 8 copies of
    elvis written above ~76000 compare ok; fsck of c0d0p2: no errors; p3
    mounts, a file written and read back.
* **Default card rebuilt** (38400, four 150MB partitions).

Not committed yet.

Committed `3d5a8fd`.

## Follow-up 32: FAT access from Minix (dosdir, dosread, doswrite)

User:
> as i cannot mount the partition 0 but i can p1, p2, p3 so all is fine
> 4 is done and 1 is currently the most helpfull for importing data in
> and out of minix

(4 = the gdbstub change in ../rosco-emulator, committed by the user; 1 =
a FAT tool for p0.)

* **Choice:** `commands/m68000/tos.c` (the ST tool) knows only 16-bit
  sector counts and computes the FAT size as unsigned short * unsigned
  short (overflows at 150 sectors per FAT). `commands/ibm/dosread.c`
  handles the 32-bit sector count, FAT16 with a partial FAT cache,
  clusters up to 4kB and 512 root entries (as mkfat.py makes them), and
  checks the byte order at run time for the FAT16 entries. Taken.
* **Built** as /usr/bin/dosread with links dosdir, doswrite (cmds/Makefile
  copies the source into obj/ibm and applies `cmds/dosread.patch`).
* **dosread.patch** (big endian fixes; it never ran on a big endian Minix):
  * the boot sector magic was compared as a native short (0x55AA here);
  * directory entries (d_time, d_date, d_cluster, d_size) were used as
    read from disk: `dir_conv()` after reading the root and subdirectory
    clusters and on a copy before writing an entry;
  * `malloc(fat_size)` with a 76800 byte FAT: malloc takes a 16-bit
    size_t, so 11264 bytes were allocated and the cache overran the heap
    (bus error at a wild PC); FATs over 64k now use the cache window;
  * `flush_fat()` converted the cache in place with two pointers to the
    same word, so each entry was written as its low byte twice (e.g.
    0x4747 for 0x0047), which also broke ROSCODE1.BIN's chain (the card
    would not have booted); the value is now taken first, and the cache
    converted back to native order after writing.
* **Tests in MAME (2.42, 5MB):** dosdir lists ROSCODE1.BIN and the free
  space; dosread ROSCODE1.BIN gives 298976 bytes; doswrite RC.TXT (346)
  and ELVIS.BIN (122750, 30 clusters) read back identical. Checked on the
  host with an independent reader: both FAT copies equal, ROSCODE1.BIN,
  RC.TXT and ELVIS.BIN match their sources.
* **Default card rebuilt** (38400).

Not committed yet.
