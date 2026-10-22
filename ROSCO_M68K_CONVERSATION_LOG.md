# Conversation log: Minix-ST → rosco_m68k

The log of the port, one file per working day. The Follow-up numbers run on
across the files. The design and the current state are in
`ROSCO_M68K_PORT_SPEC.md`.

| Day | File | Follow-ups |
|---|---|---|
| 2026-10-17 | `ROSCO_M68K_LOG_2026-10-17.md` | the feasibility study, 1-17: spec, M1 loader to M6 test suite in the emulators |
| 2026-10-18 | `ROSCO_M68K_LOG_2026-10-18.md` | 18-33: the real board, firmware 2.42 and 5MB, ACK and cc on Minix, editors, four partitions, FAT access, tty00, the create/stat anomaly |
| 2026-10-19 | `ROSCO_M68K_LOG_2026-10-19.md` | 34-: awk, bc, yap, man, cron/at, kermit, zmodem, m4; floating point (libm, libgcc soft float); asktime; gzip; manual pages; ash (/bin/sh); clock saved in /etc/stamp; more commands (advent, elle, de, yacc, flex, ...); the cc relocation bug; finger, isoread, ACK anm/asize; SLIP over tty00 |
| 2026-10-20 | `ROSCO_M68K_LOG_2026-10-20.md` | 45-65: ash prompt after errors, PATH exported (man), elvis ESC (not reproduced); ftp, ftpd, tcpd, NR_PROCS 32; telnet, telnetd, anonymous FTP; rsh, rcp, rlogin, nc, the execl(…, 0) bug, user guide; code review plan, rounds 1-2 (halt, shutdown, grep, telnetd, de, readfs, flex, kermit), groups 1 and 9 and their libc parts |
| 2026-10-21 | `ROSCO_M68K_LOG_2026-10-21.md` | 66-74: the open bugs after group 9 (lib/ip ruserok and gethostent, INET psip and TCP, the kernel's user bus error), the fragment panic, the INET review (TCP, IP, UDP/ICMP, framework) |
| 2026-10-22 | `ROSCO_M68K_LOG_2026-10-22.md` | 75-: groups 2 and 3 (files and directories, text tools), a minimum stack of 8kw for every command, CLAUDE.md, docs and images brought up to date |
