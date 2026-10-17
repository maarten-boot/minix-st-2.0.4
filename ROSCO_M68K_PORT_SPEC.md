# SPEC: Porting Minix-ST 2.0.4 to the rosco_m68k Classic v2.1

Status: analysis / proposal only. No existing file in this repository has been
modified. Date: 2026-10-17.

---

## 1. Goal and constraints

* **Goal:** run this Minix 2.0.4 tree (the Atari ST port) on the
  rosco_m68k **Classic v2.1** mainboard
  (<https://github.com/rosco-m68k/rosco_m68k>).
* **Approach requested:** remove every Atari ST device the rosco board does not
  have, and move serial I/O (and therefore the console) to the on-board
  **XR68C681P DUART**.
* **Constraint for this phase:** do not change existing files. This document
  and `ROSCO_M68K_CONVERSATION_LOG.md` are the only new files.

---

## 2. What is in this repository

### 2.1 General

* Minix 2.0.4 sources (`src/`, `include/`) as adapted for the Atari ST/STe/TT
  (see `Readme.md`, which links the atari-forum thread).
* `include/minix/config.h` selects `MACHINE = ATARI`, so `CHIP = M68000` and
  `SHADOWING = 1`. Shadowing is the no‑MMU `fork()` solution; see
  `src/kernel/stshadow.c`.
* The tree is built with the **ACK** compiler on a running Minix-ST system:
  * `src/sttools/makeconfig`: `CC = cc`, `LIBDIR = /usr/lib/m68000`, linker
    `/usr/lib/em_led`, converter `/usr/lib/cv`.
  * The assembler files use ACK syntax: `!` comments, `.sect`, `.data4`,
    `.define`, `.extern`.
  * Ints are **16 bits** (`_EM_WSIZE=2`, see `src/sttools/makeack.c68`). The
    kernel's assembly relies on this: `sys:` in `src/kernel/stmpx.s` pushes
    int arguments as `.w`.
  * User executables are relocatable, with **GEMDOS-style relocation**
    (`src/mm/exec.c`, `relocate()`). MM relocates each program at exec time;
    shadowing handles fork.

### 2.2 Local working-tree state (uncommitted, observed at session start)

* The `src/Makefile`, `src/lib/Makefile`, `src/lib/*/Makefile` and
  `src/lib/makeconfig` have been edited to build on a Linux host: `aal` was
  replaced by `ar`, `$(XCFLAGS)` was added, and some subdirs are commented out.
* There is a `bin/aal` wrapper (`ar $@`).
* **The resulting `src/lib/*.a` and `src/lib/libp/*.o` were x86-64 ELF
  objects** (checked with `file`). They are host objects and cannot be used on
  any 68k target. A cross toolchain is required (section 6).
* Update 2026-10-17: the user removed these build remnants. No `.o`, `.a` or
  `a.out` files remain in the tree. The Makefile edits and `bin/aal` are still
  present.

### 2.3 Kernel files: ST-specific vs generic

ST-only files, built by `src/kernel/makefile.st`:

| File(s) | Purpose on the ST | rosco Classic v2 equivalent |
|---|---|---|
| `stmpx.s` | Vector table at address 0, CPU/FPU/MMU detection, ST model detection, interrupt entry (`genint`), `save`/`restart`, `lock`/`unlock`, `get_mem_size`, `kreboot` | **Keep the core, rewrite the vector table and hardware parts** (section 5.2) |
| `klib68k.s` | phys_copy etc. (generic 68k) | Keep |
| `stmain.c` | `main()`, `initst()` (PSG, MFP, video RAM carve-out, kernel env), `aciaint`, `mdiint`, panic/reboot | **Rewrite `initst()`**, drop ACIA/MIDI; keep the rest |
| `stshadow.c` | Shadowing (fork without MMU) | Keep, unchanged |
| `stcon.c` | Console glue, debug dump keys, `putk` | Rework to a serial console |
| `stvdu.c`, `stfnt.c`, `stvdu.h`, `stvideo.h` | Bitmapped video console, fonts, VBL | **Remove** (no video on Classic) |
| `stkbd.c`, `stacia.h`, `keymaps/` | IKBD keyboard via 6850 ACIA | **Remove** |
| `stfloppy.c`, `stfdc.h`, `stdma.c`, `stdma.h` | WD1772 floppy + DMA chip | **Remove** |
| `stacsi.c`, `sthdc.h` | ACSI hard disk | **Remove** |
| `stscsi.c`, `stscsi.h`, `stscsidma.h` | TT/Falcon NCR5380 SCSI | **Remove** |
| `stdskclk.c`, `stdskclks.s` | Real-time clock on an ACSI host adapter | **Remove** (no RTC on rosco) |
| `stprint.c` | Centronics printer through the PSG and MFP | **Remove** (`ENABLE_PRINTER 0`) |
| `stsound.h` | YM2149 PSG: also drives floppy select and RS232 RTS/DTR | **Remove** |
| `stmfp.h` | MC68901 MFP: timers, USART, interrupt controller | **Replace** with an XR68C681 header |
| `staddr.h`, `stram.h` | ST address map and RAM config register | **Replace** with a rosco address map |
| `rs232.c` (ATARI branch), `rs2s.s` | Serial over the MFP USART, with RTS/DTR on the PSG | **Rewrite** the hardware branch for the DUART (section 5.4) |
| `pmmu.c`, `pmmus.s`, `fpp.c`, `fpps.s` | 68030 MMU / FPU (TT only) | Exclude: a 68010 has no MMU or FPU; `SHADOWING = 1` |
| `stnet.c` | Dummy dp8390 network task | Exclude initially (`ENABLE_NETWORKING 0`) |

Generic kernel files with `#if (MACHINE == ATARI)` / `CHIP == M68000` code that
need review: `clock.c`, `tty.c`, `table.c`, `system.c`, `proc.c`, `dmp.c`,
`memory.c`, `driver.c`, `drvlib.c`, `pty.c`.

Notable specifics:

* `clock.c` (M68000) calls `kb_timer()` and `fd_timer()` on every tick. Both
  belong to drivers that will be removed, so they must be stubbed or removed.
* `tty.c` prints the "Atari Type" banner and handles `VDU_LOADFONT`.
* `table.c` lists `floppy_task` in the task table and `acsi_task` as the
  `c0` driver.

`mm/` and `fs/` also contain `MACHINE == ATARI` code: `mm/exec.c`
(relocation buffer alignment), `mm/break.c`, `mm/signal.c`, `mm/mproc.h`
(shadow + swap) and `fs/table.c`. This code is "68k without MMU", not
"Atari hardware", and must stay enabled for rosco.

### 2.4 Boot path on the ST (for reference)

1. `src/sttools/boot.s` is a TOS boot sector. It loads the image with TOS BIOS
   `trap #13` / `Rwabs` to 0x40000, 0x80000 or 0x100000.
2. It then copies a small copy routine past the end of the image and moves the
   image down to **address 8**. The kernel is linked at 0 and its first longs
   are the vector table.
3. It jumps to `start` in `stmpx.s`.
4. `src/sttools/build.c` concatenates the boot block, kernel, mm, fs, (inet),
   init and menu. It patches the sizes table into the kernel's data (`_sizes`)
   and init's origin into the fs data.

---

## 3. Target hardware: rosco_m68k Classic v2.x

Sources: the rosco_m68k repo, branch `release/version-2.4` (firmware equates
`code/shared/rosco_m68k_public.asm`, `code/firmware/.../duart.asm`, `rev2.asm`,
`InterfaceReference.md`) and the r2 PLD equations
(`code/pld/r2/address_decoder/ic2_address_decoder.pld`,
`code/pld/r2/glue/ic5_glue_logic.pld`).

| Item | Value |
|---|---|
| CPU | **MC68010** @ 10 MHz. No MMU, no FPU. Has VBR, `move ccr`, and 68010 exception stack frames |
| RAM | 1 MB on board at `0x000000–0x0FFFFF`. Optional expansion RAM via the expansion connector (up to `0xDFFFFF`) |
| ROM | `0xE00000–0xEFFFFF` (decoder: A23·A22·A21·/A20). ROM is mirrored at 0 for the **first 4 bus cycles after reset only**, so the reset SSP/PC are fetched from ROM and RAM at 0 is writable afterwards |
| I/O | `0xF00000–0xFFFFFF` |
| DUART | XR68C681 at **`0xF00001`**, odd bytes, registers at a stride of 2 (`0xF00001 + 2*n`). Crystal 3.6864 MHz |
| DUART IRQ | **IPL 4, vectored**. The glue logic generates DUART IACK for level 4 (A3=1, A2=0, A1=0). The firmware sets `IVR = 0x45` |
| Timer | DUART counter/timer. The firmware runs a 100 Hz tick (ACR=`0xF0`, timer XCLK/16, CTUR/CTLR = 0x0480) |
| Storage | **SD card through bit-banged SPI on DUART GPIO**: CS=OP2, SCK=OP4, COPI=OP6, CIPO=IP2, CS1=OP7. ATA/IDE is only possible through expansion hardware (the firmware has `ATA_*` calls for it) |
| Other DUART GPIO | OP0/OP1 = RTS A/B (the firmware asserts OP0). OP3 and OP5 = LEDs (red LED / heartbeat) |
| Firmware | Low 8 KB of RAM reserved: vectors `0x000–0x3FF`, SDB `0x400–0x41F` (`SDB_MEMSIZE` at `0x414`, `SDB_UARTBASE` at `0x418`, `SDB_CPUINFO` at `0x41C`), EFP table `0x420–0x4FF` (SD/SPI/ATA function pointers), VDA `0x500–0x117F` (VDB at `0x1140`), KRES1 `0x1180–0x17FF`, and the firmware's own **.data/.bss at `0x1800–0x1FFF`** (`rosco_m68k_firmware_common.ld`; v2.4 has the same `kram = 0x1800`). User memory starts at `0x2000`. TRAP 13 = block device, TRAP 14 = char I/O, TRAP 15 = Easy68k |
| Program loading | The firmware loads `ROSCODE1.BIN` from an SD card (FAT) or over Kermit/serial to **`0x40000`** and jumps there |
| Video, keyboard, floppy, ACSI, SCSI, RTC, printer, sound, MIDI | **None** on the Classic mainboard |

---

## 4. Gap analysis

| Area | Atari ST implementation | rosco Classic v2 | Required action |
|---|---|---|---|
| CPU | 68000/010/020/030 auto-detect in `stmpx.s` | 68010 | Detection already handles a 68010 (`_proctyp = 1`). The 68010 format‑8 bus-error frame is already in `.framesizes` (`62` bytes including the return address). Could be fixed with `FIXED_CPU=1` |
| **Privileged `move sr`** | Legal in user mode on a 68000 | **Privileged on a 68010** | `src/lib/m68000/rts/setjmp.s:42` does `move.w sr,...` in user mode, so every `setjmp` would trap. Either emulate it in the privilege-violation handler (`prvtrp`) or change the library to `move.w ccr,...` |
| Vector table | Kernel linked at 0; MFP vectors `0x40–0x4F`; autovectors 2 (HBL) and 4 (VBL) used | DUART on level 4 with IVR vector; no HBL/VBL | New vector table. Set the DUART IVR to a free user vector (keep `0x45` or use `0x40`). Point all autovectors at `rupt`/spurious. Set `VBR=0` explicitly (`movec`) |
| Low memory | `.space 0x200` gap after the vectors keeps a RAM disk alive | Firmware SDB/EFP/data in `0x400–0x2000` | Decide whether Minix keeps or overwrites the firmware area (section 5.1). Overwriting means firmware TRAPs are lost |
| ST model detection | Writes to `0xFFFF8E21` and reads `0xFFFF820C`, expecting a bus error | That is rosco I/O space; behaviour is undefined or a watchdog BERR | **Remove.** Also remove `move.b #0x00,0xffff8240` (ST palette write) after `jsr _main` |
| Memory sizing | `get_mem_size` probes RAM; H&N Multiboard probe at 0x400000; TT fast RAM | 1 MB + optional expansion | Use `SDB_MEMSIZE` (`0x414`), captured by the loader before it overwrites low memory. Or set `FIXED_MEM_SIZE`. Remove Multiboard/TT code |
| **RAM budget** | `NR_BUFS 512` (512 KB cache!), `DMA_SECTORS 60`, `NR_CONS 3`, PTYs, 8 KB RS buffers with networking | **1 MB total** | Reduce `NR_BUFS` to ~40–64, disable `ENABLE_CACHE2`, `NR_CONS 1`, `NR_PTYS 0`, `ENABLE_NETWORKING 0`, 1 KB RS buffers, small `DMA_SECTORS`. Without this the system will not fit |
| Clock tick | MFP Timer C at 200 Hz, divided by `clkcnt=4` in `stmpx.s`; `HZ=60` in `minix/const.h` | DUART counter/timer | Timer mode, XCLK/16: N = 115200 / HZ. **`HZ=60` gives N=1920 (0x0780), which is exact.** Acknowledge each tick by reading the *stop counter* command register (in timer mode this clears ISR[3] without stopping the timer). Drop `clkcnt` |
| Console | VDU + IKBD keyboard (`NR_CONS 3`) | DUART channel A at 115200 baud (the firmware's console) | New serial console driver on DUART A: polled `putk` for kernel printf, interrupt-driven RX/TX for the tty |
| Serial (`/dev/tty00`) | MFP USART + PSG RTS/DTR + MFP GPIP CTS/DCD; asm fast path `rs2s.s` | DUART channel B | Rewrite the ATARI branch of `rs232.c` (section 5.4). Start with `C_RS232_INT_HANDLERS=1` (C handlers); port `rs2s.s` later |
| Interrupt controller | MFP IERA/B, IMRA/B, software EOI | One DUART interrupt with ISR/IMR | One `duartint` handler: read ISR, then dispatch counter→clock, RxRDY/TxRDY A→console, RxRDY/TxRDY B→rs232, IP change→CTS/DCD. **IMR is write-only: keep a shadow copy in RAM** |
| Disk | Floppy (DMA), ACSI, SCSI | SD card over bit-banged SPI (and optionally expansion IDE) | Phase 1: RAM/image root. Phase 2: SD driver (section 5.5) |
| Partitions | `PARTITIONING SUPRA` (Atari/ICD style) | SD cards use an MBR | Use the PC-style partition code in `drvlib.c` (`NR_PARTITIONS`, `PART_TABLE_OFF`) |
| RTC | `stdskclk.c` (ACSI clock) | None | Remove; set the time with `date` from `/etc/rc`, or later via SNTP over SLIP |
| Printer, sound, MIDI | `stprint.c`, PSG, `mdiint` | None | Remove (`ENABLE_PRINTER 0`) |
| Reboot | `kreboot`: `move.l 4,a0; jmp (a0)` | After boot, address 4 is **RAM** (it holds the kernel's `start`), not ROM | Reboot via `move.l 0xE00004,a0; jmp (a0)`, optionally preceded by `reset` and reloading SSP from `0xE00000` |
| Boot loader | TOS boot sector, BIOS `trap #13 Rwabs` | Firmware loads `ROSCODE1.BIN` to 0x40000 | New loader stub plus a variant of `build.c` (section 5.1). Note that **TRAP 13 means something different on rosco** |
| Kernel env | `rootdev=DEV_HD0+4`, `c0=ACSI`, ramimagedev on FD | — | `rootdev` = boot image or ram disk; `c0` = SD driver |
| Toolchain | ACK on Minix-ST | — | See section 6 |

---

## 5. Proposed design

### 5.1 Boot image and loader

* New tool, derived from `src/sttools/build.c`, that produces
  **`ROSCODE1.BIN`**:
  `[loader stub][kernel][mm][fs][init][optional root-fs image]`.
  It keeps the existing patches: the sizes table in the kernel and init's
  origin in fs.
* The loader stub (position-independent, runs at 0x40000) does this:
  1. `move.w #0x2700,sr`; write `0` to DUART IMR (mask the firmware tick); set
     `VBR = 0`.
  2. Read `SDB_MEMSIZE` (`0x414`) and keep it in a register, to be passed to
     the kernel at a fixed location or in its data.
  3. Copy a copy-routine past the end of the image (as `boot.s` does), then
     copy the kernel/mm/fs/init down to its link address (0, or 8 as on the
     ST). The destination is below the source, so an ascending copy is safe.
  4. Jump to `start`.
* Alternative: keep the firmware's low 8 KB intact. Leave the vectors at 0,
  but start kernel text at `0x2000` (enlarge the existing `.space` gap), and
  have the kernel install only the vectors it needs. This keeps the firmware
  EFP SD/SPI routines callable, which helps with phase 2 (section 5.5).
  **Recommended for bring-up.**
* Phase 1 root filesystem: append a Minix FS image to `ROSCODE1.BIN` and
  expose it as a memory-backed block device (an extra minor of the memory
  driver, e.g. `/dev/boot`). Set `rootdev` to it directly, so it is *not*
  copied into a separate RAM disk, which would need twice the RAM in a 1 MB
  machine.

### 5.2 Low-level kernel (`stmpx.s` → new `roscompx.s`)

* Keep: `save`, `restart`, `genint` (including the `k_reenter` logic), `sys`
  (trap #0), the synchronous trap handlers, `lock`/`unlock`/`restore`,
  `test_and_set`, the cache invalidation helpers (they are no-ops when
  `_proctyp <= 1`) and `.framesizes`.
* Remove: ST model detection, the Mega STe cache/speed pokes, the palette poke,
  `intr2`/`intr4` HBL/VBL handling, the MFP vectors `iob*`, `tim*`, `sia*`,
  `acia`, `dma`, `pia`, `scsi*`, Multiboard probing and `_atari_type`.
* Add:
  * one vector entry for the DUART (at the IVR value), which goes through
    `genint` to `_duartint`;
  * a privilege-violation emulation for `move sr,<ea>` (68010), unless the
    library is changed instead;
  * `_kreboot` to the ROM vector at `0xE00004`.
* `_unlock` may stay at `0x2200` or go to `0x2000`. The DUART is level 4, so a
  level-2 mask has no effect on it.

### 5.3 Kernel C side

* `rosmain.c` (from `stmain.c`): `initrosco()` replaces `initst()`. It
  * initialises the DUART (both channels, ACR, counter, IVR, IMR shadow);
  * removes the PSG/MFP/video setup;
  * sets the kernel env: `rootdev`, `ramsize=0`, `c0=SD`,
    `processor=M68000`, `NR_BUFS`, `NR_CONS=1`, `NR_PTYS=0`.
  Keep `main()`, `trap()`, `checksp()`, `panic()`, `getenv()` and
  `idle_task()` as they are.
* `rosduart.h` (replaces `stmfp.h`/`staddr.h`): register struct on odd bytes
  at `0xF00001`; bits for SR, ISR/IMR, ACR, CSR codes and OPR set/reset.
* `roscons.c` (replaces `stcon.c`/`stvdu.c`/`stkbd.c`): the console tty on
  DUART A, with polled `putk` and interrupt-driven input. Keep the debug dump
  keys by mapping them to a serial escape sequence (optional).
* `clock.c`: remove the `kb_timer()`/`fd_timer()` calls for this machine.
  Starting the tick moves into `initrosco()` or `clock.c`'s `init_clock()`.
* `table.c`: drop `floppy_task` and `printer_task`; map `c0` to the new SD
  task.
* `tty.c`: drop the Atari banner and `VDU_LOADFONT`.
* Configuration: introduce `MACHINE == ROSCO` (new value) with
  `CHIP = M68000` and `SHADOWING = 1`. Many `MACHINE == ATARI` tests in
  `mm/`, `fs/`, `kernel/` and `lib/` really mean "68k without an MMU, Minix-ST
  binary format". Turn those into a derived macro (e.g. `M68K_NOMMU`), or
  extend them with `|| MACHINE == ROSCO`. A quicker alternative is to keep
  `MACHINE = ATARI` and add `ATARI_TYPE = ROSCO`, then gate the hardware code
  on that. It is less clean, but it touches the fewest files.

### 5.4 DUART serial driver (XR68C681)

* Channel A = console, channel B = `/dev/tty00` (`NR_RS_LINES 1`).
* Baud rates: set ACR[7] (rate set 2) and the XR68C681 extended-rate commands
  in CRx (`0xA0` = extended TX, `0x80` = extended RX, as the firmware does),
  so that 115200 is available. Build a table from termios `Bxxx` to the CSR
  nibble (RX and TX are the same). Note that **ACR[7] is shared by both
  channels**, so the allowed speed combinations are limited.
* Line control: MR1x (parity, bits, RxRTS), MR2x (stop bits, TxCTS). Write MR1
  then MR2 after a "reset MR pointer" command.
* Modem lines: RTS A/B on OP0/OP1 through OPR set/reset. CTS on IP0/IP1, with
  input-port-change interrupts (ACR[3:0] / IPCR). Hardware flow control can be
  automatic (MR1[7] RxRTS, MR2[4] TxCTS). DTR/DCD/RI are not present, so treat
  DCD as always asserted.
* Interrupts: TxRDY is level-triggered. Enable TxRDY in the IMR only while the
  output queue is non-empty, or the CPU will spin in the interrupt. Receive
  errors (parity, framing, overrun) are in SRx bits 4–7 and must be cleared
  with a "reset error status" command. Breaks come through ISR bit 2 / 6.
* **Do not drive OP2, OP4, OP6 or OP7** (the SD SPI pins) or OP3/OP5 (the
  LEDs) from the serial code. The OPCR must keep them as plain outputs.
* Check the FIFO depth and any XR-specific registers against
  `docs/XR68C681 Information.pdf` in the rosco repo.

### 5.5 Storage (phase 2)

* Option A, quick: an `sd_task` built on `driver.c`/`drvlib.c` that calls the
  firmware's `EFP_SD_INIT`/`EFP_SD_READ`/`EFP_SD_WRITE` through the pointers at
  `0x44C`/`0x450`/`0x454`. This requires keeping the firmware's low 8 KB and
  its BSS intact (section 5.1). It is polled and blocks the whole kernel while
  transferring, which is acceptable for a first version.
* Option B, proper: a native SD/SPI driver that bit-bangs SPI through the
  DUART OPR set/reset and IP2. Port `bbspi.c`/`bbsd.c`/`dua_spi_asm.asm` from
  the firmware into ACK assembly or C. Use SDHC block addressing and MBR
  partitions.
* Option C, optional: if expansion IDE hardware is present, add an ATA PIO
  driver modelled on `at_wini.c`.
* Swap (`ENABLE_SWAP`) is useful on 1 MB. Put a swap partition on the SD card.

---

## 6. Toolchain options

The code requires an ACK-compatible compiler with **16-bit ints** and ACK m68k
assembler syntax. The x86-64 libraries currently in `src/lib` are not usable.

1. **Self-hosted ACK under emulation (recommended first).** Run Minix-ST 2.0.4
   in the Hatari emulator, build there with the original toolchain
   (`make -f makefile.st`), and copy `kernel`/`mm`/`fs`/`init` and the root FS
   image out. This changes the least and reproduces the known-good ST build
   first.
2. **A cross ACK on Linux.** Old ACK 5.x/6.0 releases had a 68000 target with
   2-byte ints (`m68k2`). The modern davidgiven/ack mainly supports 32-bit-int
   m68k targets, which do not match `_EM_WSIZE=2` binaries or the kernel's
   assembly calling conventions. Possible, but needs effort and verification.
3. **Migrate to GCC (`m68k-elf-gcc -m68000 -mshort`) with binutils.** This is
   a large job:
   * rewrite all ACK-syntax `.s` files (kernel, `lib/m68000`);
   * reproduce the executable header and GEMDOS-style relocation that
     `mm/exec.c` expects;
   * check the calling and return conventions against the assembly glue.
   It is the best long-term maintainability path, but it should not be the
   first step.

---

### 6.1 Decision (2026-10-17): GNU tools from the rosco SDK

The user chose the rosco toolchain from the docker image
`roscopeco/rosco_m68k` (<https://github.com/rosco-m68k/rosco_m68k_docker>):
`m68k-elf-gcc` 13.3.0, GNU binutils 2.40, with a 68000 multilib.

* **How to run it:** `src/rosco/xrun <command>` runs the command in the
  container. It runs as the calling user, mounts the repository at its own
  path, and adds the image's `linuxbrew` group (gid 1001), which is needed
  because the toolchain lives in the mode-750 `/home/linuxbrew`.
* **Checked:**
  * `-mcpu=68010 -mshort` gives `sizeof(int) == 2`.
  * An `int` argument is a word at `4(sp)` and a following `long` is at
    `6(sp)`. This is the same stack layout the ACK m68k 2-byte-int code (and
    the kernel's `sys:` glue) uses.
* **Consequences** (this is option 3 above):
  * Every ACK-syntax `.s` file used by the rosco build needs a GNU-syntax
    rosco copy (`.S`): `stmpx.s` → `roscompx.S`, `klib68k.s` →
    `roscklib.S`, and for user programs `src/lib/m68000/**/*.s`
    (crtso, setjmp, syscall stubs, string routines).
  * The shared C sources (kernel, mm, fs, lib) are compiled with
    `m68k-elf-gcc -mcpu=68010 -mshort -ffreestanding -fno-builtin -std=gnu89`.
    Expect warnings from the K&R style code; any real incompatibility goes
    into a rosco copy of the affected file.
  * **libgcc:** the image has no `-mshort` multilib, so 32-bit
    multiply/divide helpers (`__mulsi3`, `__divsi3`, `__udivsi3`,
    `__modsi3`, `__umodsi3`) must come from our own `roscklib.S`. Those
    helpers take `long` arguments, but linking the stock libgcc built for
    32-bit ints is not safe in general.
  * **Executable format:** MM loads programs in the Minix-ST format, with an
    a.out-like header from `outmix.h` and GEMDOS-style relocation (see
    `mm/exec.c`). We need a host tool that converts `m68k-elf` output
    (linked with `--emit-relocs` or `-q`) into that format, and a rosco
    variant of `sttools/build.c` for the boot image of kernel, mm, fs and
    init.
  * **Binary compatibility:** existing ACK-built Minix-ST binaries should
    still run, since the trap #0 ABI and the 16-bit int message layout are
    unchanged. Verify this early with a known binary from the ST
    distribution.
  * The **`move sr` problem** in `setjmp` disappears for gcc-built
    programs, because the rosco copy of setjmp uses `move ccr`. Old
    ACK-built binaries still need the `prvtrp` emulation in the kernel.

### 6.2 Emulator: r68k with a DUART model

The user pointed to r68k (<https://github.com/roscopeco/r68k>), a simplified
emulator that runs rosco binaries.

What upstream r68k does:

* Musashi core in 68010 mode; 1 MB RAM; a small ROM at 0xE00000.
* It loads the binary to 0x40000, resets, and the ROM jumps there. The ROM
  fills in the SDB (magic, memory size).
* Firmware TRAP 13/14/15 are handled by the emulator itself (the ROM executes
  an illegal instruction with magic values in d6/d7). TRAP 13 reads and
  writes the file `rosco_sd.bin` as the SD card.
* **No DUART registers.** I/O space reads 0 and writes are dropped. A thread
  raises IRQ 4 (vector 0x45) about every 10 ms, regardless of any mask.
* So code that drives the DUART directly (our loader, and later the Minix
  kernel) hangs in its first polled transmit loop. This was confirmed with
  the M1 image: no output.

What we added: `src/rosco/emu/r68k-duart.patch` (applied by
`src/rosco/emu/build-r68k.sh` to r68k at commit `bac2219`) adds a minimal
**XR68C681 model**, enabled with **`r68k -d`**:

* **Registers:** at 0xF00001 + 2n, on odd bytes only.
* **Channel A:** TX goes to stdout; RX comes from stdin; SRA RxRDY/TxRDY are
  driven; RHRA pops the input.
* **Channel B:** TX goes to stderr; there is no RX.
* **Interrupt registers:** ISR, a write-only IMR, and IVR (reset value 0x0F).
  The IRQ is **level sensitive**: it is asserted while `ISR & IMR` is non-zero,
  and the acknowledge returns IVR.
* **Counter/timer:** runs in real time from ACR/CTUR/CTLR. "Start counter"
  starts it; in timer mode "stop counter" only clears ISR[3].
* **Not modelled:** MR/CSR/CR are accepted and ignored apart from the MR
  pointer, so there are no baud rates, framing, FIFO depth or break. TX is
  always ready. There are no RTS/CTS or OP pins, so no bit-banged SD.
* Without `-d`, r68k behaves exactly as upstream.

Build and run:

```
src/rosco/emu/build-r68k.sh          # clone, patch, build into src/rosco/emu/r68k/
cd src/rosco/loader && make run      # r68k -d ROSCODE1.BIN
```

**M1 result in the emulator (2026-10-17; re-run after the keep range moved
to 0x400–0x2000):**

* The loader banner prints.
* The payload reports magic `524F5343` and memory `00100000`.
* The firmware SDB at 0x400 is intact, so the keep range works.
* The 60 Hz timer delivers one '.' per second through vector 0x45 from IVR.
* Input is echoed.
* `q` returns through the ROM reset vector, and the r68k ROM restarts the
  image.

Limits to keep in mind:

* Serial timing and overrun behaviour, hardware flow control, real bus
  errors for unmapped addresses (r68k returns 0 instead), and the bit-banged
  SD path can only be tested on the real board.
* For SD in the emulator, the firmware-EFP option (5.5 A) is the one that can
  work, because r68k handles TRAP 13. Whether calling the EFP pointers
  directly works there needs checking.
* r68k's ROM is its own small stub (`firmware/` in r68k), not the real
  firmware, so the low-RAM layout it leaves behind is not the real one. The
  real layout comes from `../rosco_m68k` (section 3).

### 6.3 Local reference checkouts

* `../r68k`: upstream r68k at `bac2219`, the commit `build-r68k.sh` pins.
  The DUART patch applies cleanly (`git apply --check`). Point the script at
  it with `R68K_URL=$(realpath ../r68k) src/rosco/emu/build-r68k.sh` (run
  from the repo top) instead of cloning from GitHub.
* `../rosco_m68k`: all branches are available. The checkout is now on
  **`release/version-2.42`** (`60ab3a8`, 2024-01-07), the latest firmware
  release. Releases 2.1, 2.2, 2.4 and 2.42 and `develop` all agree on what
  the port depends on: firmware RAM data/bss at `kram = 0x1800`, load address
  `0x40000`, `/ROSCODE1.BIN`, IVR `0x45`, SDB/EFP addresses, and DUART at
  `0xF00001` on the r2 mainboard. So it does not matter which 2.x firmware
  the board runs. Notes from `develop` (`ec229f9`):
  * unchanged: DUART at `0xF00001`, IVR `0x45`, SDB/EFP addresses,
    `KERNEL_LOAD_ADDRESS` `0x40000`, `/ROSCODE1.BIN`;
  * new: stage 2 also loads `/ROSCODE1.ELF`, but it refuses segments below
    `0x2000`, so it cannot place a kernel linked at 0. Our BIN loader stays;
  * stage 2 now loads at `0xC0000` (was `0xF0000`). It is gone once our
    loader runs, so this does not matter;
  * **firmware .data/.bss are at `0x1800–0x1FFF`** (also in v2.4). The keep
    range was corrected from `0x400–0x1000` to `0x400–0x2000` (section 9).

## 7. Milestones

| # | Milestone | Done when |
|---|---|---|
| M0 | Toolchain: rebuild the **unmodified** ST kernel/image (Hatari + ACK) | The image boots in Hatari |
| M1 | Bare-metal loader stub: `ROSCODE1.BIN` loads at 0x40000, copies a test payload to its link address, prints over DUART A with polling | The text shows on the rosco serial console |
| M2 | Kernel with the new vector table, `initrosco`, polled `putk`, DUART tick at 60 Hz, no disks (disk tasks are `fake_task`) | The "Minix 2.0.4" banner prints; the clock ticks |
| M3 | Interrupt-driven console tty on DUART A, `rs232` on DUART B | Echo, line discipline and `stty` work |
| M4 | Root on the appended memory image; MM/FS/INIT start; `/etc/rc` runs a shell | A login or shell prompt on the serial console |
| M5 | SD driver (A, then B), MBR partitions, Minix FS on SD, swap | `mount` of SD partitions works; root on SD |
| M6 | Tuning: RAM budget, expansion RAM support, `rs2s.s` fast path, `move sr` fix in libs, `src/test` suite | Tests pass; usable multi-user |

---

## 8. Risks and open questions

* **RAM.** 1 MB has to hold the kernel, MM, FS, the buffer cache, init, the
  shell and shadow copies for fork. That is tight but realistic for Minix 2
  (the ST ran in 1 MB) if the buffer cache and networking are cut. Expansion
  RAM would help a lot. *Question: does the target board have expansion RAM?*
* **68010 `move sr`** in user code. Other libraries or binaries may contain it
  too: `grep` for `move.w sr` / `move sr` across `src/lib`, `src/commands` and
  the binary distribution.
* **The firmware tick and its IMR state** must be stopped before the kernel
  takes over, or a stray level-4 interrupt will arrive through a vector the
  kernel has not yet installed.
* **The interrupt priority model** of `genint`/`unlock` assumes several MFP
  sources. With a single level-4 source, kernel critical sections use `0x2700`
  (still fine), but the clock and the serial line share one handler. Make sure
  TX/RX latency at 115200 baud is acceptable; that is about 87 µs per
  character with only a small RX FIFO. Lowering the console to 38400 or 19200
  at first is a safe fallback.
* **HZ.** The ST code sets `HZ=60` but uses MFP Timer C at 200/4 = 50 Hz
  (`clkcnt=4`). Verify how the ST port reconciles this before relying on it.
  On rosco, 60 Hz (N=1920) is exact.
* **Toolchain availability.** ACK m68k2 and Hatari images need to be located.
  *Question: is a working Minix-ST 2.0.4 installation (disk image) available?*
* **Firmware version.** This analysis used firmware 2.4 (`release/version-2.4`).
  *Question: which firmware is on the v2.1 board?* The load address 0x40000
  and the SDB layout are stable across 2.x.
* **Expansion hardware** (Xosera video, IDE, Ethernet) is out of scope, but it
  would remove some limits later.

---

## 9. Implementation layout (decided 2026-10-17)

**Decision (user): implement with new `rosco*` files on the `rosco-m68k`
branch. Existing files are not edited.**

How this works without touching shared files:

* **Config override.** `src/rosco/include/minix/config.h` is a copy of
  `include/minix/config.h` with rosco settings. The rosco makefiles use
  `-I../rosco/include -I$i`, so it wins over the original for the kernel, mm,
  fs and libraries.
  * `MACHINE` stays `ATARI`. This keeps the generic no-MMU 68k code in
    mm/fs/lib (relocation, shadowing, signal frames, old-signal
    compatibility), so existing Minix-ST binaries keep working.
  * `ATARI_TYPE = ROSCO` (6) selects the rosco hardware. Nothing shared tests
    `ATARI_TYPE` except `== TT` (FPP) and `== DETECT_TYPE` (banner,
    `atari_type`), which are both false for ROSCO.
* **Same interface, new implementation.** The shared kernel files call the
  machine layer only through the functions declared in `proto.h`
  (`CHIP == M68000` block). The rosco files implement those same names. The
  ST files are simply not listed in `makefile.rosco`.
* **Stubs for hooks the shared code still calls.** `clock.c` calls
  `kb_timer()` and `fd_timer()`; `tty.c` calls `vdu_loadfont()`; `table.c`
  lists `floppy_task`. The rosco files provide no-op versions
  (`floppy_task` already becomes `fake_task` via `NR_FD_DRIVES 0` in the
  `stmain.c` logic, which is carried over into `roscmain.c`).
* **SD driver hook-up.** `table.c`'s driver table only knows `acsi_task`
  (`NR_ACSI_DRIVES > 0`). It cannot be edited, so for phase 2 the options are:
  (a) name the rosco SD task entry point `acsi_task`, with `c0=ACSI` in the
  environment; or (b) a rosco copy of `table.c`. (b) is cleaner, because
  `table.c` is small. **Decide this at M5.**

### File plan

| File | Status | Contents |
|---|---|---|
| `src/rosco/include/minix/config.h` | **done** | Config override (48 buffers, 16 procs, 1 console + 1 RS line, no PTY/printer/ACSI/floppy/net/swap, `ATARI_TYPE ROSCO`, `ROSCO_HZ 60`, `ROSCO_KEEP_FW 1`: keep 0x400–0x2000, `OPTIMIZE_FOR_SPEED 0`, `_WORD_SIZE` also for gcc). Checked with host `cpp` |
| `src/kernel/roscaddr.h` | **done** | Address map, firmware SDB/EFP addresses, ROM reset vectors, load address |
| `src/kernel/roscduart.h` | **done** | XR68C681 registers (odd bytes at 0xF00001), SR/ISR/CR/MR/ACR bits, CSR codes, timer macro, OP/IP pins (SPI pins flagged). Host syntax check passed. Items marked "verify" need the Exar datasheet or schematic |
| `src/rosco/xrun` | **done** | Runs a command in the `roscopeco/rosco_m68k` toolchain container |
| `src/rosco/.gitignore` | **done** | Ignores the build outputs |
| `src/rosco/emu/r68k-duart.patch`, `build-r68k.sh` | **done** | XR68C681 model for the r68k emulator (`-d`); the script clones r68k at a fixed commit, patches and builds it (section 6.2) |
| `src/rosco/loader/loader.S`, `loader.ld` | **done (M1 passes in r68k -d; not yet run on hardware)** | `ROSCODE1.BIN` stub at 0x40000. Masks the DUART, reads `SDB_MEMSIZE`, moves its copier above the file, copies the payload to `load_addr` while skipping the keep range (default 0x400–0x2000), sets VBR=0, enters the payload with d0='ROSC', d1=memsize, a0/d2=extra image. Disassembly checked |
| `src/rosco/loader/hello.S`, `payload.ld` | **done (M1 passes in r68k -d; not yet run on hardware)** | Test payload linked at 0, laid out like the kernel. Code starts at 0x2000. Prints the hand-over values, checks that the SDB magic survived, runs the DUART timer at 60 Hz on vector 0x45 (one '.' per second), echoes input, reports unexpected exceptions, and `q` reboots via the ROM vector |
| `src/rosco/loader/mkrosco.c`, `Makefile` | **done** | Host tool that joins stub + payload (+ extra image) and fills the header. `make` builds `ROSCODE1.BIN` (9152 bytes); `make run` runs it in r68k with the DUART model |
| `src/kernel/roscompx.S` | **done (M2)** | GNU port of `stmpx.s`: vector table (DUART at IVR `0x45`, all autovectors → `rupt`), firmware gap 0x400–0x2000, bss clear, loader hand-over (`rosco_memsize`, `rosco_extra`), CPU type from `SDB_CPUINFO` (probe as fallback), unchanged `genint`/`save`/`restart`/`sys`, `kreboot` via `0xE00004`, `get_mem_size` from the loader. Still todo: `move sr` emulation in `prvtrp` (M4, for ACK binaries) |
| `src/kernel/roscklib.S` | **done (M2)** | `klib68k.s` in GNU syntax, plus `__mulsi3`/`__udivsi3`/`__divsi3`/`__umodsi3`/`__modsi3`, the `_send`/`_receive`/`_sendrec` trap stubs and the `sig*set` aliases |
| `src/kernel/roscmain.c` | **done (M2)** | From `stmain.c`: `main` (servers with size 0 are not started: kernel-only image), `initrosco()` (DUART A as the firmware sets it, channel B off, timer at HZ, IVR, kernel env), `duartint()`, IMR shadow, `trap`/`panic`/`wreboot`/`getenv`, fake tasks. Compile-time check of the proc offsets used by the assembler |
| `src/kernel/roscons.c` | **done (M2)** | Console on DUART A: polled output with `out_process`, interrupt-driven input buffer, `putk`, debug keys PF1–PF3 (process table with uptime, memory map, registers), stubs for `kb_timer`, `vdu_loadfont`, `vduswitch`, `func_key` |
| `src/kernel/rosco.h` | **done (M2)** | Prototypes and variables of the rosco machine layer |
| `src/kernel/roscstub.c` | **done (M2)** | `fd_timer`, a temporary `rs_init` (DUART B discards output until M3), `errno`, the dummy MFP for `clock.c` |
| `src/kernel/roscrs232.c` | todo (M3) | Replaces `rs232.c`: `rs_init` and friends for DUART B, called from `duartint` |
| `src/rosco/kernel/Makefile`, `kernel.ld` | **done (M2)** | GNU build in `src/rosco/kernel/` through `xrun`: rosco files + shared `proc system stshadow tty clock memory table dmp misc pty driver drvlib` + a small `libk.a` (kprintf, `_sigset`, u64, string functions). Links at 0, data on a click boundary, `sizes[0..1]` from the linker. `make` gives `kernel`, `kernel.bin` and `ROSCODE1.BIN` (loader + kernel); `make run` boots it in `r68k -d` |
| `src/rosco/kernel/staddr.h`, `stmfp.h` | **done (M2)** | Stand-ins found by the build-time copy of `clock.c`, so its MFP timer code writes a dummy in RAM |
| `src/rosco/include/stdarg.h` | **done (M2)** | gcc builtins; the `__GNUC__` branch of `include/stdarg.h` is for gcc 1/2 |
| `src/rosco/tools/` | todo (M4) | ELF → Minix-ST executable converter (with relocations); image builder for kernel + mm + fs + init (fills `sizes[2..]`, found behind the magic `SIZE`) |
| `src/mm/makefile.rosco`, `src/fs/makefile.rosco` | todo (M4) | Same as `makefile.st`, with the include override |

### M2 result (2026-10-17, r68k -d)

```
Minix rosco loader
rosco_m68k: 68020, 1024kB RAM, kernel 56kB
Minix 2.0.4  Copyright 2001 Prentice-Hall, Inc.

hi                                   <- typed, echoed by the tty layer
uptime 120 ticks                     <- PF1, then PF1 again 2 s later:
...                                     uptime 240, IDLE user time 240
```

* The kernel is 0x9700 bytes of text plus 0x49 clicks of data/bss (56 KB).
* All kernel tasks start and wait in `receive(ANY)`. MM, FS and INIT are
  not in the image yet, so they are not started.
* The clock runs at 60 Hz from the DUART timer (120 ticks in 2 s).
* Console input is interrupt-driven and echoed.

Findings while doing M2:

* **`tty.c` bug with `NR_PTYS 0`:** with `OPTIMIZE_FOR_SPEED`, `tty_init`
  reads `tty_tbl_ptr[NR_CONS+NR_RS_LINES]`, which is one past the end of
  that array. The ST config never hit this because it has PTYs. The rosco
  config turns `OPTIMIZE_FOR_SPEED` off. This also affects mm/fs, which is
  slower but correct.
* **Emulator CPU type:** r68k's Musashi accepts 68020 opcodes in 68010
  mode, so both its ROM and the kernel see a "68020". The real firmware
  identifies a 68010 correctly. The kernel behaves the same either way:
  format-0 frames, and the cache ops are harmless there.
* ACK predefines that the headers rely on are passed as `-D` flags:
  `_EM_WSIZE=2`, `_EM_PSIZE=4`, `_EM_LSIZE=4`, `__m68000`.
* Without MM/FS the kernel needs only a few library routines (`libk.a`).

## 10. Immediate next actions (no changes to existing files required)

1. Decide on the toolchain path (section 6). If it is option 1, set up Hatari
   with a Minix-ST 2.0.4 disk image and confirm M0.
2. Decide how the existing files may be changed: a new branch with in-place
   edits, or new `rosco*` files plus a small set of guarded edits. The
   `rosco-m68k` branch already exists.
3. Write the M1 loader stub and a polled DUART "hello" test, using the rosco
   firmware's `ROSCODE1.BIN` loading. This is fully independent of the Minix
   tree.
4. Grep the libraries and commands for privileged instructions (`move sr`,
   `stop`, `reset`, `movec`).
5. Draft `include/minix/config.h` settings for `MACHINE == ROSCO` (RAM budget,
   drivers off).
