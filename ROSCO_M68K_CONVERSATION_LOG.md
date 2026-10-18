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

Not committed yet.
