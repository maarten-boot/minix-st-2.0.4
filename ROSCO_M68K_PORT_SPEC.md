# SPEC: Porting Minix-ST 2.0.4 to the rosco_m68k Classic v2.1

Status: started 2026-10-17 as an analysis; the port is implemented on the
`rosco-m68k` branch (sections 9 and 11). Sections 2-8 and 10 are the
original analysis and are kept as written; where they differ from the code,
section 9 and the conversation log are current. Updated 2026-10-18.

Existing files: the port is in new `rosco*` files. Since 2026-10-18 an
existing file may be changed on `rosco-m68k` after the user approves the
specific file; so far `src/kernel/system.c` (`umap()`, section 9 M6). The
user's commit `8a7b10f` holds the host-build Makefile edits and `bin/aal`
(section 2.2); they are not part of the port.

---

## 1. Goal and constraints

* **Goal:** run this Minix 2.0.4 tree (the Atari ST port) on the
  rosco_m68k **Classic v2.1** mainboard
  (<https://github.com/rosco-m68k/rosco_m68k>).
* **Approach requested:** remove every Atari ST device the rosco board does not
  have, and move serial I/O (and therefore the console) to the on-board
  **XR68C681P DUART**.
* **Constraint for this phase:** do not change existing files. This document
  and `ROSCO_M68K_CONVERSATION_LOG.md` are the only new files. (Later
  relaxed; see the status note at the top.)

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
* Update 2026-10-18: the user committed the Makefile edits and `bin/aal` in
  `8a7b10f` ("makefile updates and aal (outdated)"): `bin/aal`,
  `src/Makefile`, `src/lib/Makefile`, `src/lib/makeconfig` and the
  Makefiles of `src/lib/{ansi,curses,editline,end,float,ip,libm2,libp}`.
  The rosco build does not use them.

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

(The analysis as of 2026-10-17. Current values: console 38400 by default
(`ROSCO_CONS_BAUD`, 115200 for MAME), `NR_BUFS` 128; see section 9.)

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

### 5.2 Low-level kernel (`stmpx.s` → new `roscompx.S`)

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

* `roscmain.c` (from `stmain.c`): `initrosco()` replaces `initst()`. It
  * initialises the DUART (both channels, ACR, counter, IVR, IMR shadow);
  * removes the PSG/MFP/video setup;
  * sets the kernel env: `rootdev`, `ramsize=0`, `c0=SD`,
    `processor=M68000`, `NR_BUFS`, `NR_CONS=1`, `NR_PTYS=0`.
  Keep `main()`, `trap()`, `checksp()`, `panic()`, `getenv()` and
  `idle_task()` as they are.
* `roscduart.h` (replaces `stmfp.h`/`staddr.h`): register struct on odd bytes
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
* Baud rates: rate set 1 (ACR[7] = 0) plus the XR68C681 extended-rate
  commands in CRx (`0xA0` = extended TX, `0x80` = extended RX) where a
  speed needs them. 38400 is the plain code C, and 115200 is code 8 with
  the extended bit. The firmware uses rate set 2; see the 2026-10-18 note
  in section 9. Build a table from termios `Bxxx` to the CSR
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

(As of 2026-10-17. The questions are answered: the board has the 4MB
expansion (5MB), runs firmware 2.42, the port uses new `rosco*` files, and
the toolchain is gcc (section 6.1) with ACK on the target (11.2); M0 with
Hatari was skipped. Open items are in section 12.)

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
branch. Existing files are not edited.** (Relaxed 2026-10-18: an existing
file may be changed after the user approves it; see the status note.)

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
| `src/rosco/include/minix/config.h` | **done** | Config override (128 buffers (48 until M6), 16 procs, 1 console + 1 RS line, no PTY/printer/ACSI/floppy/net/swap, `ATARI_TYPE ROSCO`, `ROSCO_HZ 60`, `ROSCO_KEEP_FW 1`: keep 0x400–0x2000, `OPTIMIZE_FOR_SPEED 0`, `_WORD_SIZE` also for gcc). Checked with host `cpp` |
| `src/kernel/roscaddr.h` | **done** | Address map, firmware SDB/EFP addresses, ROM reset vectors, load address |
| `src/kernel/roscduart.h` | **done** | XR68C681 registers (odd bytes at 0xF00001), SR/ISR/CR/MR/ACR bits, CSR codes, timer macro, OP/IP pins (SPI pins flagged). Host syntax check passed; the pins were checked against the r2 schematic and the baud rates on the board (rate set 1) |
| `src/rosco/xrun` | **done** | Runs a command in the `roscopeco/rosco_m68k` toolchain container |
| `src/rosco/.gitignore` | **done** | Ignores the build outputs |
| `src/rosco/emu/r68k-duart.patch`, `build-r68k.sh` | **done (M5 update)** | XR68C681 model for the r68k emulator (`-d`); the script clones r68k at a fixed commit, patches and builds it (section 6.2) |
| `src/rosco/loader/loader.S`, `loader.ld` | **done (runs on the board)** | `ROSCODE1.BIN` stub at 0x40000. Masks the DUART, reads `SDB_MEMSIZE` (prints `RAM xxxxxxxx`; firmware 2.42 overwrites the SDB magic, so a plausible size, a multiple of 512K from 1 to 14MB, is accepted without it), moves its copier above the file, copies the payload to `load_addr` while skipping the keep range (default 0x400–0x2000), sets VBR=0, enters the payload with d0='ROSC', d1=memsize, a0/d2=extra image. Disassembly checked |
| `src/rosco/loader/hello.S`, `payload.ld` | **done (M1 passes in r68k -d; not yet run on hardware)** | Test payload linked at 0, laid out like the kernel. Code starts at 0x2000. Prints the hand-over values, checks that the SDB magic survived, runs the DUART timer at 60 Hz on vector 0x45 (one '.' per second), echoes input, reports unexpected exceptions, and `q` reboots via the ROM vector |
| `src/rosco/loader/mkrosco.c`, `Makefile` | **done** | Host tool that joins stub + payload (+ extra image) and fills the header. `make` builds `ROSCODE1.BIN` (9152 bytes); `make run` runs it in r68k with the DUART model |
| `src/kernel/roscompx.S` | **done (M2)** | GNU port of `stmpx.s`: vector table (DUART at IVR `0x45`, all autovectors → `rupt`), firmware gap 0x400–0x2000, bss clear, loader hand-over (`rosco_memsize`, `rosco_extra`), CPU type by probing (firmware 2.42 overwrites the SDB, so `SDB_CPUINFO` is not usable), unchanged `genint`/`save`/`restart`/`sys`, `kreboot` via `0xE00004` after resetting the DUART (IVR 0x0F, MR pointers, ACR) so the firmware finds it again, `get_mem_size` from the loader, `move sr` emulation in the privilege-violation handler (M6) |
| `src/kernel/roscklib.S` | **done (M2)** | `klib68k.s` in GNU syntax, plus `__mulsi3`/`__udivsi3`/`__divsi3`/`__umodsi3`/`__modsi3`, the `_send`/`_receive`/`_sendrec` trap stubs and the `sig*set` aliases |
| `src/kernel/roscmain.c` | **done (M2)** | From `stmain.c`: `main` (servers with size 0 are not started: kernel-only image), `initrosco()` (DUART A as the firmware sets it, channel B off, timer at HZ, IVR, kernel env), `duartint()`, IMR shadow, `trap`/`panic`/`wreboot`/`getenv`, fake tasks. Compile-time check of the proc offsets used by the assembler |
| `src/kernel/roscons.c` | **done (M3)** | Console glue: `scr_init` hands channel A to `roscrs232.c`; polled `putk` (TxRDY test and write under `lock()`); debug keys PF1–PF3 (process table with uptime, memory map, registers) and PF4 (test line on `/dev/tty00`); stubs for `kb_timer`, `vdu_loadfont`, `vduswitch`, `func_key` |
| `src/kernel/rosco.h` | **done (M2)** | Prototypes and variables of the rosco machine layer |
| `src/kernel/roscdisk.c` | **done (M5)** | Disk driver on `driver.c`, registered as `acsi_task` (`NR_ACSI_DRIVES 1`, `c0=ACSI`). `c0d0` (minors 0–4): the SD card through the firmware's EFP SD routines (`sd_efp_*` in `roscklib.S`), whole card plus the four MBR primary partitions, parsed here because `drvlib`'s `partition()` copies little-endian entries raw. `c0d1` (minor 5): the root image appended to `ROSCODE1.BIN`, in RAM. `sd_root_minor()` finds the first Minix partition for the root when there is no image |
| `src/kernel/roscstub.c` | **done** | `fd_timer`, `errno`, the dummy MFP for `clock.c` |
| `src/kernel/roscrs232.c` | **done (M3)** | `rs232.c` logic on the DUART for both lines: line 0 is the console (channel A), line 1 is `/dev/tty00` (channel B). Interrupt-driven buffered input and output; the TxRDY interrupt is only enabled while output may go out. termios speeds 50–115200 (CSR code + XR extended bit, rate set 1: ACR[7]=0), parity, data and stop bits; `rs_config()` writes only what changed, waits for TXEMT, then resets and re-enables the channel; B0 drops RTS, any other setting raises it again (after HUPCL at close). RTS input flow control on OP0/OP1. CTS is gated in hardware (MR2) unless CLOCAL; the console never waits for CTS and keeps `ROSCO_CONS_BAUD`. Errors are counted and the character dropped; break, HUPCL (drops RTS) |
| `src/rosco/kernel/Makefile`, `kernel.ld` | **done (M2)** | GNU build in `src/rosco/kernel/` through `xrun`: rosco files + shared `proc system stshadow tty clock memory table dmp misc pty driver drvlib` + a small `libk.a` (kprintf, `_sigset`, u64, string functions). Links at 0, data on a click boundary, `sizes[0..1]` from the linker. `make` gives `kernel`, `kernel.bin` and `ROSCODE1.BIN` (loader + kernel); `make run` boots it in `r68k -d` |
| `src/rosco/kernel/staddr.h`, `stmfp.h` | **done (M2)** | Stand-ins found by the build-time copy of `clock.c`, so its MFP timer code writes a dummy in RAM |
| `src/rosco/include/stdarg.h` | **done (M2)** | gcc builtins; the `__GNUC__` branch of `include/stdarg.h` is for gcc 1/2 |
| `src/rosco/Makefile` | **done (M5)** | Top level: builds loader, kernel, lib, boot, cmds, the root fs and `ROSCODE1.BIN`; `make run` boots it in `r68k -d`. `make sdcard` builds `rosco_sd.bin` to `dd` to a card: four 150MB partitions, p0 FAT16 with `ROSCODE1.BIN` (no root image), p1 the Minix root (153600 blocks, 16384 i-nodes), p2/p3 empty Minix file systems (`SDEXTRA`); written sparse (601MB). `make runsd` boots it in r68k, `make CONSBAUD=115200 runmame` in MAME; `make cc` adds the C compiler |
| `src/rosco/lib/u64.c` | **done (M5)** | Replaces `lib/m68000/misc/u64.c`, whose `cmp64u()` always returned 0 for `u == 0` (operator precedence), and whose `mul64u()` lost the high word |
| `src/rosco/lib/` | **done (M4)** | C library build: `libc.a` (lib/ansi, posix, other, stdio; generated syscall stubs; fphook no-float printf/strtod; strcasecmp from lib/ip), `libsys.a` (lib/syslib), `crtso.o`; for floating point `libm.a` (lib/math, `fpmisc.c` frexp/modf) and `fpprintf.o` (fphook.c, ecvt.c with long double as double), with the soft float of the m68000 libgcc. GNU replacements: `crtso.S` (with the `0xDADA` data block), `sndrec.S`, `setjmp.S` (register-saving jmp_buf, `move ccr`), `sigret.S`, `brksz.S`, `m68kmul.S` (mul/div helpers, shared with the kernel), `gccmem.c`, `regexp.c` wrapper |
| `src/rosco/include/string.h` | **done (M4)** | Renames the Minix `memcpy`/`memmove`/`memset`/`memcmp` to `_memcpy16` etc. (16 bit `size_t`); the plain names are the 32 bit versions gcc calls itself (`lib/gccmem.c`) |
| `src/rosco/boot/` | **done (M4)** | Builds MM (6k stack), FS (4k), INIT (`sttools/init.c`, 2k) |
| `src/rosco/cmds/` | **done** | Builds the old Minix shell (`commands/sh`) and 127 commands (`commands/simple` and others: ps, aal, make, patch, mined, elvis, halt/shutdown, dosread/dosdir/doswrite, awk, bc, yap, m4, cron/crontab, kermit, rz/sz, cawf, ...), with the ACK build's stack sizes (`tools/stacks.py`). Fixes as files here: `dosread.patch` (big-endian FAT access), `printfint.h`, `elvisproto.h`; curses programs link `libcurses.a` from `src/rosco/lib` |
| `src/rosco/tools/` | **done (M4)** | `prog.ld` (program link script), `mkminix.py` (ELF → Minix-ST a.out with GEMDOS relocations), `mkimage.py` (boot image: relocates MM/FS/INIT, fills `sizes[]` behind the magic `rosSIZES`, patches INIT's origin into FS), `mkfs.py` (big endian Minix V2 file system from a proto file), `stacks.py`; M5: `mkfat.py` (FAT16 with ROSCODE1.BIN for the firmware), `mksd.py` (SD card image with an MBR, typed partitions) |
| `src/rosco/test/` | **done (M6)** | Builds the Minix test suite (`src/test`, at `-O0`: the tests busy-wait on non-volatile flags set by signal handlers), plus `troscorun` (gcc runtime helpers, struct copies, varargs, `move sr` emulation), `texecl` (execl directly and after fork) and `tshadow` (kernel copies from a shadowed process, see M6) |
| `src/rosco/include/rosco_sizeof.h` | **done (M6)** | `sizeof(...)` as a 16-bit unsigned int, as with ACK, for K&R code that calls functions without prototypes; `-include`d by every rosco Makefile (kernel, lib, boot, cmds, test), with a compile-time check that the object is smaller than 64K |
| `src/rosco/lib/_uname.c` | **done (M6)** | `uname`: machine `rosco_m68k` (arch stays `m68000`); node name from `/etc/hostname.file` (`rosco`) |
| `src/rosco/lib/system.c` | **done (M6)** | `system()` waits for its own child with `waitpid()`; the original reaped any child with `wait()` (test28) |
| `src/rosco/tools/mamesession.py` | **done (M6)** | Scripted console sessions in the rosco-emulator (MAME): console on a TCP socket, `expect`/`send` script, log |
| `src/rosco/rootfs/` | **done** | Root file system: `proto` (devices, /etc, /bin, /tmp, /usr; RAM root), `sdproto` (the rest of the SD root: commands, tests), `emptyproto` (p2/p3), `etc/rc`, `etc/ttytab` (getty on the console, vt100, and on tty00), `etc/profile` (TZ, `stty intr ^C erase ^?`), `/etc/termcap`, `passwd`, `group`, `utmp` |
| MM and FS build | **done** | In `src/rosco/boot/Makefile` (no `makefile.rosco` in `src/mm`, `src/fs`) |
| `src/rosco/ack/` | **done** | The native C compiler (section 11.2) |
| `src/rosco/cc/` | kept | The c68/Sozobon chain, host tools only; fallback |
| `src/kernel/system.c` (existing file) | **changed 2026-10-18** | `umap()`: the shadow offset only for addresses in data and stack, not for text below the data (M6, create/stat anomaly) |

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

### M3 result (2026-10-17, r68k -d)

* The console runs on the same interrupt-driven line code as `/dev/tty00`.
  Echo and line editing behave as the Minix defaults dictate: DEL is
  INTR (`^?`, input flushed), `^U` kills the line with `\b \b`, and CR
  becomes NL.
* PF4 queues a test line on `/dev/tty00`. It came out on channel B (r68k
  stderr) with LF→CR LF processing.
* `stty` and opening `/dev/tty00` from a process need MM/FS (M4). r68k
  does not model channel B receive.

Hardware facts checked against the r2 schematic
(`design/r2/kicad/DUART.kicad_sch`):

* OP0 = RTSA, OP1 = RTSB, IP0 = CTSA, IP1 = CTSB;
* SPI on IP2 / OP2 / OP4 / OP6 / OP7;
* CTS runs straight to the 6-pin UART headers J1/J2 with no pull resistor.

Console speed (2026-10-17): the target board's firmware runs the console
(serial 1 = channel A) at **38400**. The stock firmware 2.x uses 115200.
`ROSCO_CONS_BAUD` in the rosco `config.h` (default 38400) sets channel A
in `rs_cons_setup()`, which is called from `initrosco()`. The speed comes
from the same table as `tty00`. The loader does not touch the speed, so it
always prints at the firmware's rate.

Board test (2026-10-18): with 38400 as CSR code C plus the extended bit
under rate set 2, the console switched to **19200** once Minix started.
That is the set 2 value without the extended bit, although MAME's
XR68C681 table gives 38400 for it. The kernel now uses rate set 1, where
38400 is the standard MC68681 code C and needs no XR extension. Confirmed
on the board: the console stays at 38400. 115200
(code 8, extended) is unchanged and was retested in MAME. 19200 is now
code C with the extended bit, and is the entry to check on the board.

Still "verify" on the board:

* the extended baud rate table: 38400 under set 1 (plain code C) works on
  the board; the set 2 extended code C gave 19200 on the board;
  115200 (code 8, extended) is confirmed by the firmware in set 2, and in
  set 1 by MAME only;
* the internal pull-ups on the IP pins.

Risk: interrupt-driven output at 115200 baud means one interrupt per
character (about 11,500/s) on a 10 MHz 68010. If that costs too much CPU,
fall back to 38400 on the console, or send bursts from the TTY task.

### M4 result (2026-10-17, r68k -d): shell prompt

```
Minix rosco loader
rosco_m68k: 68020, 1024kB RAM, kernel 62kB
root image: 400kB at 9c000
Minix 2.0.4  Copyright 2001 Prentice-Hall, Inc.

Memory size = 1024K   MINIX = 207K   RAM disk = 0K   Available = 418K

rosco_m68k: running /etc/rc
# cat /tmp/f | wc
      2      4     18
# stty
speed 38400 baud; 24 rows 80 columns
# df
Filesystem    1k-Blocks     free     used    %  FUsed%  Mounted on
/dev/c0d0           400      118      282  71%    56%   /
```

* Boot path: the loader copies kernel, MM, FS and INIT (`boot/image.bin`,
  206 KB) to 0. The kernel moves the root image (400 KB) to the top of
  RAM, and FS mounts it from `roscdisk.c`.
* INIT runs `/etc/rc`, then the ttytab entry: `sh` on the console.
* Tested and working: fork/exec, pipes, redirection, files and
  directories, `stty`, `sleep` (alarm), `df`/`mtab`.
* The date is 1970, because there is no RTC.
* `make` in `src/rosco` builds it all; `make run` boots it.

Findings while doing M4:

* **gcc's own `memcpy` calls pass a 32-bit length.** Struct assignment
  (`*rpc = *rpp` in the kernel's `do_fork`) compiles to a `memcpy` call
  with gcc's `size_t`, which is 32 bits even with `-mshort`. The Minix
  `memcpy` took a 16-bit length and copied nothing, so fork failed ("MM
  can't reply to 3"). Fixed with the rosco `<string.h>` renaming plus
  `gccmem.c`. A scan of all built sources found no implicit mem*
  declarations that would bypass the renaming.
* **`sizeof` is 32 bits with gcc** (ACK: 16 bits). Code that passes
  `sizeof` to an unprototyped function or a `%d` varargs argument breaks.
  Fixed globally by `include/rosco_sizeof.h` (see section 9).
* **ACK predefines needed by the headers and sources:** `__minix`,
  `__m68000`, `_EM_WSIZE/PSIZE/LSIZE`. Plus `_MINIX`, `_POSIX_SOURCE` and
  `__MSHORT__` from the old gnu-16 library config.
* **`<stdarg.h>`:** `va_list` stays `char *` (Minix prototypes), and
  `va_start` uses `__builtin_va_start`. With `__builtin_next_arg`, gcc
  cloned a static variadic function into a fixed-argument copy.
* **The root image size is a RAM trade-off:** 400 blocks leave about
  418 KB for processes.

### M5 result (2026-10-17, r68k -d): SD card storage, root on SD

```
Minix rosco loader
c0d0p0: type 06, 16384 sectors at 2048
c0d0p1: type 81, 16384 sectors at 18432
root on the SD card: c0d0p1
...
Memory size = 1024K   MINIX = 209K   RAM disk = 0K   Available = 816K
# df
/dev/c0d0p1        8192     7853      339   5%     5%   /
```

* **SD access:** through the firmware's SD routines (EFP table), with the
  same driver on the board and in r68k (`rosco_sd.bin`). The firmware's
  SD timeouts are loop counts, and its SPI code only touches the DUART
  input port and the OPR set/reset commands, so it coexists with the
  kernel. Its state lives in firmware RAM 0x1800–0x1FFF, which is kept.
* **Working:** `mount`/`umount` of `c0d0pN`, reading and writing, and
  persistence across reboots.
* **Root on SD:** used when no root image is appended. Programs then get
  816 KB, against 416 KB with the RAM root.
* **Card layout** (`make sdcard`): p0 FAT16 with `ROSCODE1.BIN` (the
  firmware tries the partitions in order and boots the first FAT with
  `ROSCODE1.BIN`), p1 the Minix root. `fsck.fat` accepts the FAT
  partition.

Findings while doing M5:

* **The r68k ROM corrupted the loaded program.** r68k loads `ROSCODE1.BIN`
  to 0x40000 before its ROM runs, and the ROM's RAM probe writes
  0x12345678 at 0x80000, which is file offset 0x40000. This time it hit
  the `lea` in `/bin/sh`'s `getc` inside the root image: `getc` recursed
  forever, the shell's stack ran down through all of memory (no MMU) and
  overwrote the kernel. The probe now restores the word it tests (emulator
  patch only; the real firmware sizes RAM before loading). Found with the
  new emulator aids: write watchpoints, PC trace, RAM snapshots.
* **`cmp64u()` bug in the Minix-ST library** (see `src/rosco/lib/u64.c`);
  it made the SD partition open fail with ENXIO.
* **`drvlib`'s `partition()` is not big-endian safe;** `roscdisk.c` parses
  the MBR itself.

Still open for storage:

* the card size (CSD register), so the whole-card device has a real size;
* Minix subpartitions and extended partitions;
* swap on SD (`ENABLE_SWAP`);
* a native SPI driver that doesn't depend on the firmware (option B);
* the SD routines block the kernel while a block moves.

### M6 status (2026-10-17): test suite 39/40 in the full-hardware emulator

The user built `../rosco-emulator` (MAME 0.289 cut down to the rosco
machines: full DUART, bit-banged SPI SD card, bus errors, the real
firmware). `make CONSBAUD=115200 runmame` (or `make CONSBAUD=115200
sdcard` and `tools/mamesession.py`) boots `rosco_sd.bin` there. A plain
`make runmame` rebuilds the card with a 38400 console.

The whole boot path works in MAME:

* the firmware boots `ROSCODE1.BIN` from the card's FAT partition;
* the kernel finds the root on the Minix partition and uses the firmware's
  real SPI SD routines;
* the CPU is reported correctly as a 68010 (r68k said 68020);
* getty and login on the console work.

Test suite, run as `ast` after login:

* final full run: **39 passed, 1 failed** (test14, a test artifact, see
  below; it passed in the run before);
* `troscorun` and `texecl` pass;
* test17 once printed "stat: cannot stat f7" and "creat: incorrect mode"
  (a just-created file not found by name) in the full run. It did not
  come back in 12 more runs (test17 alone ×5; tests 14–17 ×3 on the card
  state the suite left behind). Ruled out: the firmware's block cache
  (`BBSD_read_block` always reads the card), the SPI pins (atomic OPR
  set/reset), task preemption, the driver's sector loop. If it comes back,
  log directory block reads and writes in `roscdisk.c`.
  It came back on 2026-10-18 with the test suite built with ACK (MAME,
  firmware 2.42, 5MB): test18's identical create loop, "cannot stat f4".
  So it does not depend on the compiler or the library build.
  **Solved 2026-10-18: a kernel bug in shadowing.** A forked child shares
  its parent's memory slot; the clock task (`unshadow()`, every few
  quanta) swaps one of them into the slot and copies the other out to a
  shadow area, also while that one waits for FS. `umap()` then maps the
  waiting process's addresses to its shadow, but it did so for every
  address below the end of the data, text included, and only data and
  stack are shadowed. String constants and const data are in text (ACK
  and gcc), so FS copied the name "f4" (a constant in test18's text, with
  a child left from an earlier subtest) from unrelated memory, got "",
  and stat failed with ENOENT. Found with MAME debugger breakpoints at
  FS's `do_stat()` and the kernel's `phys_copy()`, and the proc table at
  the failure. Fixed in `umap()` (`src/kernel/system.c`): the shadow
  offset only for addresses from the data base up. `tshadow` (write a
  const buffer while a child keeps the slot busy, read back and compare):
  76 of 6000 writes corrupted before, 0 after.

Fixes found by the tests:

* **libc `__divsi3`/`__modsi3`:** they tested the dividend's sign with
  stale flags. The time was off by 2^32/60 s after setting the date.
* **libc build:** it built `posix/_exec.c`/`_execn.c`, old files not in
  the ACK `makefile.all`; their `execl` lost argv[0]. This broke login
  and test7.
* **`system()` reaped the caller's other children** (rosco `system.c`).
* **`cp` (also rm/mv/ln):** a 31744-byte stack buffer; the stack is now
  24kw instead of 16kw.
* **`/dev/ram` node:** must be a block device.
* **Buffer cache 48 → 128 blocks:** with 48, every exec re-read the
  program over the bit-banged SPI, and test29/test33 hit their alarms.
  733 KB is left for programs with the SD root.
* **Tests built with `-O0`.**
* **Clock:** set from `/etc/stamp` at boot (no RTC). getty/login with
  `/usr/lib/pwdauth`, user `ast`.
* **Kernel `move sr` emulation** for 68000 user code.

Known and not fixed:

* **test14:** it builds a file name from the pid, which contains `/`
  when pid & 31 == 14, so it fails at random.
* **Shell tests:** `sh1` passes when run alone. In the full `run`
  sequence (after tests 1–40) its four `compress` runs can fail with
  "program too big" (exec ENOMEM): compress needs 470K contiguous (450K
  stack/heap for 16-bit codes) out of 733K free, and memory is then
  fragmented. Seen before and after the global `sizeof` change, so not a
  regression. With the 4MB expansion (5MB) it passes in the full run
  too. `sh2` passes since 2026-10-18 with the native C compiler (below).
* **The 32-bit `sizeof` hazard:**
  * gcc's `sizeof` is 32 bits even with `-mshort` (ACK: 16 bits). In a call
    without a prototype (K&R `char *malloc();`) a `sizeof` argument is
    pushed as 4 bytes where 2 are read. `aal` failed with "Out of core"
    because of this.
  * Fixed: `include/rosco_sizeof.h` (`#define sizeof(x)` as
    `(unsigned) sizeof(x)` plus a negative-array check that fails the
    build for objects of 64K or more) restores the ACK type. It is
    `-include`d by all rosco Makefiles: kernel, lib, boot, cmds, test.
  * Full MAME suite with it: 39/40 (only test14), troscorun and texecl
    pass, as before.
* **`rs2s.s` fast path:** not needed; the C interrupt handlers are in use.
* **Expansion RAM:** contiguous expansion is already used via the
  firmware's RAM size (up to 14 MB); untested.

## 10. Immediate next actions (no changes to existing files required)

(Historical, 2026-10-17: all done or decided; see sections 6.1 and 9.)

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

## 11. Ideas for later

### 11.1 Minix in the unused part of the 1MB flash ROM (noted 2026-10-18)

The ROM space is 1MB at 0xE00000-0xEFFFFF (two SST39SF040, even and odd
bytes: the firmware build makes `rosco_m68k_even.rom` and
`rosco_m68k_odd.rom`). Firmware 2.42 uses only 43792 bytes of it (2.50.DEV
892928), so with 2.42 about 980kB are free. Possible uses, cheapest first:

* **Boot image in ROM.** The boot image (kernel, MM, FS, INIT, about 292kB)
  at a fixed ROM offset, e.g. 0xE80000. A small ROSCODE1.BIN (just the
  loader) or a firmware hook copies it to RAM: no 14 s SD load at 21kB/s,
  and the SD card is only the root file system. The loader already copies
  a payload; it would take the source address from the ROM instead of
  from the file.
* **Read-only ROM disk.** A Minix file system image in ROM as another
  minor of the rosco disk driver (like c0d1, the RAM root, but with
  `dv_base` in ROM and writes refused): a fast /usr with the commands
  that rarely change, mounted read-only, using no RAM and no SD time.
* **Execute in place:** kernel text run from ROM. Not worth it: the
  kernel is small, and Minix-ST shadowing and the image layout assume
  everything in RAM.

To check first: how the board's ROMs are (re)programmed (external
programmer, or an in-system flash tool in the firmware: not in 2.42 as far
as seen); that the firmware leaves the rest of the ROM alone; and the
even/odd split for the image tools. MAME can try it first: its ROM region
is the whole 1MB, so a ROM file with the image appended can be loaded with
`-bios` once the driver gets an entry for it.

### 11.2 C compiler on the target (done 2026-10-18)

The Amsterdam Compiler Kit for minix68k (m68k2, 16-bit int), the original
Minix-ST compiler, in `src/rosco/ack/`:

* `build-ack.sh` + `ack-minix68k.patch`: ACK at a pinned commit on the
  host (fixes: ncg `con_mult()` for 4-byte constants on m68k2, the cv
  header on 64-bit hosts, `maketempfile.c` for Minix); `acc` is the cross
  compiler.
* `lib/`: the Minix C library (src/lib) compiled with ACK, plus `emrts.s`
  (EM symbols ACK's libem.a expects); `acc-minix` compiles and links
  Minix programs on the host. The ACK-built test suite passes 40/40.
* `mkpass.py`: the ACK passes rebuilt with m68k-elf-gcc -mshort as Minix
  programs from the host build's compile commands, with `passinc/` and
  `passlib/` (stdint.h, strdup, the 64-bit gcc routines).
* On the SD root: `/usr/bin/cc` (`cc.sh`), `/usr/ack`, `/usr/include`
  (`mkccproto.py`), added by `make sdcard` after `make cc`.
* sh2 passes (make, cc, the program, strings).

The c68/Sozobon chain in `src/rosco/cc/` (host tools only) is kept as a
fallback.

## 12. Open items (2026-10-18)

| Item | Status |
|---|---|
| SLIP over tty00 (also SNTP for the clock, section 4 RTC) | Open |
| Brandy BASIC | Open; assessed only |
| Second SD card on CS1 (OP7; schematic SPICS2, header J7) | Open; `OP_SPI_CS1` is left alone by the kernel |
| Boot image or read-only ROM disk in the flash ROM (11.1) | Open |
| 115200 input-overrun test on the board | Open; `overrun_errors` is counted but not reported |
| Yellow firmware banner | Open; needs a firmware rebuild |
| Swap on SD (`ENABLE_SWAP` 0), card size (CSD), subpartitions, native SPI driver | Open |
| ttytab in the repo: tty00 without a "stty 38400" field (9600) | The user's board setting works; repo default unchanged |
| create/stat anomaly (test17/test18) | Solved: `umap()` fix (M6) |
| Manual pages for man/whatis | Open; none in the tree (`/usr/man` is empty) |
| Clock without RTC: ask the date at boot or login (cron, at) | Done: `asktime -t 30` in `/etc/rc` (Follow-up 35); the clock still starts from `/etc/stamp` |
| zip/unzip-style compression for serial transfers | Open (user, Follow-up 34); `compress` and `tar` are on the image |
