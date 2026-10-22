# Minix-ST 2.0.4 on the rosco_m68k: working notes

Branch `rosco-m68k`. The port of Minix-ST 2.0.4 to the rosco_m68k Classic
v2 (68010, 5MB). Design and state: `ROSCO_M68K_PORT_SPEC.md`; user guide:
`ROSCO_M68K_USAGE.md`; the code review plan and its progress table:
`ROSCO_M68K_REVIEW.md`; the work log, one file per day:
`ROSCO_M68K_LOG_<date>.md` (index `ROSCO_M68K_CONVERSATION_LOG.md`). The
end of the newest log lists what is open ("Open after ...").

## The compiler ABI
GNU m68k-elf-gcc 13.3 with `-mshort`: int and size_t are 16 bits,
pointers and long 32 bits, big-endian. The usual traps: pointer- or
long-returning functions without a prototype, `execl(..., 0)` (a 2-byte
0), `%d` for a long, counts and sizes past 32767/65535, read() -1
compared unsigned, signed char as an index. read()/write() of more than
32767 bytes fail system-wide; gid_t is a char.

## Where changes go
- **Commands** (`src/commands` stays as imported): the "Source fixes"
  mechanism in `src/rosco/cmds/Makefile`: the path in `FIX0`, a unified
  diff in `src/rosco/cmds/fixes/<path>.patch` (applied to a copy in
  `obj/fix` after `execl0.sed`), and an object rule compiling from
  `obj/fix` (multi-target rules `obj/a.o obj/b.o: obj/%.o:
  obj/fix/simple/%.c`). Some tools have their own patch file
  (`ash.patch`, `telnetd.patch`).
- **Stacks:** every command gets at least 8kw (`mkminix.py -m 8kw`);
  more with `STACK_<tool>` in the cmds Makefile, with a comment why.
- **INET** (`src/inet` stays as imported): fixed copies in
  `src/rosco/boot/inetfix/` with a header comment, and a rule in
  `src/rosco/boot/Makefile`. After changing a rule or flags delete the
  affected `boot/inetobj/*.o`. INET's asserts are compiled out (NDEBUG,
  CRAMPED): only ip_panic halts it.
- **C library:** fixed copies in `src/rosco/lib` (original in `EXCLUDE`,
  object in `COBJ`, a line in the list above `EXCLUDE`).
- **Kernel:** `src/kernel`, edited in place.
- Mark every change in the code with a comment starting `rosco:` that
  says why.

## Building (run from the host; `xrun` runs the toolchain container)
- One command: `cd src/rosco/cmds && ../xrun make bin/<tool>`.
- INET: `cd src/rosco/boot && make inet`. Kernel: `cd src/rosco/kernel &&
  make kernel.bin [CONSBAUD=115200]` (not through ../xrun).
- Images, from `src/rosco`: `make sdcard` (rosco_sd.bin) and `make`
  (ROSCODE1.BIN) at 38400. For the emulator: `make CONSBAUD=115200
  sdcard && cp boot/sdfat.img boot/sdfat115.img`, then the default build
  again. Rebuild the images only at the end of a full round of work
  unless a test needs a new kernel/INET.

## Testing in the emulator (MAME, ../rosco-emulator)
- A test card, from `src/rosco` (X = a letter):
  `cd boot && python3 ../tools/mkfs.py -b 153600 -i 16384 test_X_root.img ../rootfs/proto ../rootfs/sdproto manproto ../ack/ccproto /abs/proto && cd .. && python3 tools/mksd.py boot/test_X_sd.img 06:boot/sdfat115.img 81:boot/test_X_root.img 81:boot/sdp2.img 81:boot/sdp3.img`
  Proto lines as in `rootfs/sdproto`; a path already in the protos cannot
  be given again (put it elsewhere and copy it on the board). A fresh
  card for every run (MAME writes to it); a card must be 630194176
  bytes.
- Run: `python3 tools/mamesession.py -a '-bios v242 -ram 5m'
  ../../../rosco-emulator boot/test_X_sd.img boot/test_X_log.img script`
  (always `-ram 5m`, else "Not enough core"). With SLIP: `SLIPN=n
  tools/mameslip.sh card log script peerlog` (container slippeer<n>,
  host end 10.0.0.1, board 10.0.0.2; needs an /etc/rc.net from
  `rootfs/etc/rc.net.slip` with `speed=115200` and getty off on tty00:
  put an /etc/ttytab.S on the card and copy it in rc.net).
- Scripts: `expect TEXT<TAB>SECONDS`, `send TEXT`, `type TEXT`, `sleep
  SECONDS` (real time; the emulator runs ~55x faster). Start with
  `expect RETURN keeps it 300`, `send`, `expect login: 120`, `send root`,
  `expect #`. Markers must not match the echo: `send cmd; echo M1 | tr M
  N` / `expect N1`. At most 255 characters per line. Read the log with
  `tr -d '\r' < log`. Keep sessions short; kill leftover `./rosco`
  processes.
- Smoke test after an image rebuild: SLIP up, `cd /usr/test; sh sh1; sh
  sh2`, ping, `/dev/ttyp0` mode 600, cron and update running, no
  "panic", "sig=", "Stack low" or "Memory fault" in the log.
- Show a bug with the original binary first, then the fix.

## Logging and committing
- Each piece of work gets a "Follow-up N" in the day's log (numbers run
  on across days; a new day starts a new file and a row in the index):
  the user's request, cause with file:line, fix, test evidence, notes.
  Add a row to the progress table in `ROSCO_M68K_REVIEW.md` for review
  work, and keep USAGE/PORT_SPEC current when behaviour changes.
- One commit per tool group or bug, message `rosco_m68k: ...`, ending
  with the Co-Authored-By line. Commit as Maarten Boot
  <11140490+maarten-boot@users.noreply.github.com>.
- Parallel agents share Makefiles: commit one agent's part by staging a
  Makefile built from `git show HEAD:<file>` plus only its edits
  (`git hash-object -w` + `git update-index --cacheinfo`).
- Session scratch files are lost when the app restarts: keep nothing
  needed later only there.
