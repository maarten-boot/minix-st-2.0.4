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
