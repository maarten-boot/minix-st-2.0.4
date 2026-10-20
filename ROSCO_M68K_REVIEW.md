# Code review of the tools: plan and progress

The user asked (Follow-up 49) for a review of all tools for obvious errors
and improvements, one at a time with a break point after each, and the
option to do 2 or 4 in parallel. This file is the list to work through. The
user picks the order; the suggested order is at the end.

## What a review of one tool covers

1. **The -mshort traps** (16-bit int and size_t, 32-bit pointers and long):
   * functions used without a prototype that return a pointer or a long
     (the caller keeps 16 bits);
   * pointer/int casts and conversions, and int where a pointer is passed
     (execl(…, 0), see Follow-up 48);
   * printf/scanf formats against the argument sizes (%d with a long);
   * objects or counts of 32K/64K and more (read/write counts, malloc of
     more than 64K, arrays);
   * long/off_t values kept in an int (lseek, file sizes, block numbers).
2. **The stack size** (chmem): run it with real input; the kernel prints
   "Stack low" when the stack gets close to the heap.
3. **Obvious upstream bugs**: missing printf arguments, unterminated
   loops, unchecked results that crash, buffer overruns on long input.
4. **A test in MAME**: normal use, the error paths (bad arguments, missing
   files), and how it ends (prompt back, exit status).
5. **Improvements** worth noting (not necessarily done).

Fixes go into patch or sed files applied to copies (as `ash.patch`,
`execl0.sed`, `telnetd.patch`), never into the original Minix sources.
After each tool or group: the default card is rebuilt, the change is
logged, and the user commits (the break point).

## Automatic scan (Follow-up 49)

Every source the cmds build compiles was compiled again with
`-fsyntax-only`. The first pass replaced `-w` by
`-Wimplicit-function-declaration -Wint-conversion
-Wincompatible-pointer-types -Wint-to-pointer-cast -Wpointer-to-int-cast`.
The second pass had builtins on and `-Wformat`; gcc knows that int is 16
bits under -mshort, so its format checks fit this system. Repeat it with
`src/rosco/tools/warnscan.py` (in src/rosco/cmds; `-f` for the formats,
tool names to limit it).

* 186 programs, about 200,000 lines (cmds only; INET, the kernel, MM, FS
  and the libraries are not in these numbers).
* Implicit declarations of library functions that return a pointer or a
  long:
  * `realloc` (flex), `ctime` (kermit), `mktemp` (elvis), `time` (awk);
  * `strcpy`/`strcat`/`strncpy` (elvis, kermit, awk): only a bug where
    the result is used.
* Program functions used before their declaration (372) were checked
  against their definitions: none returns a pointer or long (as far as
  the heuristic sees).
* Format mismatches (45):
  * de and readfs print long zone/block numbers with %u/%d (wrong
    numbers);
  * shutdown passes a long to %d, kermit an int to %ld;
  * halt, grep and telnetd's wtmp.c have a %s without an argument
    (reads a garbage pointer);
  * elle and awk print pointers with %o/%d (debug output).
* Pointer/int casts: awk (37), elle (29), nonamed, ash, sh (a few each).
* `diff`: returns the address of a local variable.

## The list

"Scan findings" counts the warnings above; "-" means the scan found
nothing, which does not mean the tool is fine.

### 0 Libraries and runtime (every tool depends on these)

| Part | Where | Notes |
|---|---|---|
| libc (ansi, posix, other, stdio, ip) | src/lib, built by src/rosco/lib | execl/execv family, stdio, malloc, getpwent, termcap, regexp |
| rosco runtime | src/rosco/lib/*.S, gccmem.c, u64.c | crtso, sndrec, setjmp, sigret, m68kmul |
| libm, fpprintf, soft float | src/lib/math, fphook, libgcc | awk, cawf |
| libcurses, libedit | src/lib/curses, editline | life, gomoku; ash |
| INET | src/inet, src/rosco/boot | all network tools |

### 1 Shells, login, accounts, scheduling

Dependencies: the base of everything a user does; ash and login run every session.

| Tool | Lines | Scan findings |
|---|---|---|
| sh | 4367 | 1 int/ptr, 10 incompat. ptr |
| ash | 12224 | 2 int/ptr |
| login | 507 | - |
| getty | 197 | - |
| su | 153 | - |
| passwd | 260 | - |
| pwdauth | 357 | - |
| cron | 1361 | - |
| crontab | 1137 | - |
| at | 223 | - |
| mail | 776 | - |
| asktime | 87 | - |

### 2 Files and directories

| Tool | Lines | Scan findings |
|---|---|---|
| dd | 406 | - |
| ls | 1150 | - |
| cp | 1385 | - |
| mkdir | 265 | - |
| rmdir | 46 | - |
| cat | 138 | - |
| chmod | 256 | - |
| chown | 168 | - |
| touch | 242 | - |
| mknod | 70 | - |
| mkfifo | 196 | - |
| find | 913 | - |
| du | 218 | - |
| df | 426 | - |
| stat | 369 | - |
| file | 278 | - |
| install | 637 | - |
| basename | 76 | - |
| dirname | 36 | - |
| pwd | 49 | - |
| sync | 14 | - |
| update | 24 | - |
| mount | 204 | - |
| umount | 92 | - |
| printroot | 83 | - |

### 3 Text tools

| Tool | Lines | Scan findings |
|---|---|---|
| grep | 379 | 1 format |
| sed | 1613 | - |
| sort | 1193 | - |
| uniq | 195 | - |
| cut | 310 | - |
| tr | 169 | - |
| head | 78 | - |
| tail | 359 | - |
| wc | 151 | - |
| cmp | 129 | - |
| comm | 206 | - |
| diff | 1252 | 1 return-local-addr |
| cdiff | 365 | - |
| fgrep | 355 | - |
| paste | 456 | - |
| join | 360 | - |
| split | 117 | - |
| fold | 70 | - |
| expand | 99 | - |
| unexpand | 121 | - |
| rev | 75 | - |
| tee | 66 | - |
| tsort | 356 | - |
| od | 314 | - |
| strings | 139 | - |
| sum | 121 | - |
| cksum | 155 | - |
| crc | 117 | - |
| prep | 136 | - |
| pr | 504 | - |
| printf | 465 | - |
| echo | 51 | - |
| expr | 629 | - |
| test | 324 | - |
| env | 83 | - |
| printenv | 17 | - |
| yes | 23 | - |
| sleep | 34 | - |
| date | 443 | - |
| time | 160 | - |
| kill | 100 | - |
| id | 107 | - |
| who | 71 | - |
| whoami | 19 | - |
| tty | 30 | - |
| mesg | 48 | - |
| write | 268 | - |
| uname | 128 | - |
| which | 86 | - |
| xargs | 425 | - |
| look | 158 | - |
| cgrep | 379 | - |
| ifdef | 504 | - |

### 4 Editors and pagers

Dependencies: need termcap (/etc/termcap) and a vt100 terminal.

| Tool | Lines | Scan findings |
|---|---|---|
| elvis | 16853 | 28 implicit ptr/long |
| elle | 14609 | 29 int/ptr, 31 incompat. ptr, 35 format |
| ellec | 1405 | 2 incompat. ptr |
| mined | 3711 | - |
| yap | 4134 | 7 incompat. ptr |
| ed | 2198 | - |

### 5 awk, bc, m4, formatting

Dependencies: awk, cawf: floating point (libm, fpprintf, libgcc); man runs nroff (cawf) and more (yap).

| Tool | Lines | Scan findings |
|---|---|---|
| awk | 5461 | 2 implicit ptr/long, 37 int/ptr, 1 format |
| bc | 7268 | - |
| m4 | 2188 | - |
| cawf | 5968 | - |
| man | 695 | - |
| indent | 3642 | - |

### 6 Archives and compression

Dependencies: ftpd and sh1 use tar/compress/gzip.

| Tool | Lines | Scan findings |
|---|---|---|
| tar | 1106 | - |
| compress | 1616 | - |
| gzip | 7396 | - |
| aal | 1563 | - |
| shar | 71 | - |
| uue | 217 | - |
| uud | 562 | - |
| decomp16 | 444 | - |
| backup | 590 | - |

### 7 Development

Dependencies: make runs commands with /bin/sh; cc (ACK) is separate (src/rosco/ack).

| Tool | Lines | Scan findings |
|---|---|---|
| make | 2588 | - |
| patch | 0 | - |
| yacc | 6359 | - |
| flex | 10135 | 1 implicit ptr/long, 1 int/ptr |
| nm | 226 | - |
| size | 64 | - |
| strip | 169 | - |
| chmem | 140 | - |
| mkproto | 248 | - |
| proto | 688 | - |
| ci | 343 | - |
| co | 252 | - |
| fix | 224 | - |

### 8 System and disks

Dependencies: ps reads the kernel/MM/FS tables (NR_PROCS); de, fsck, readfs read raw devices.

| Tool | Lines | Scan findings |
|---|---|---|
| ps | 597 | - |
| fsck | 1501 | - |
| mkfs | 1367 | - |
| mkswap | 194 | - |
| de | 3757 | 3 format |
| dosread | 1198 | - |
| readall | 139 | - |
| readfs | 598 | 1 format |
| badblocks | 646 | - |
| inodes | 243 | - |
| ncheck | 178 | - |
| halt | 216 | 1 format |
| shutdown | 598 | 1 format |
| sysenv | 80 | - |
| last | 470 | - |
| leave | 210 | - |
| cleantmp | 361 | - |
| synctree | 1481 | - |
| remsync | 1551 | - |
| intr | 148 | - |
| isoread | 860 | - |

### 9 Network

Dependencies: need INET (src/rosco/boot/inet, also to review) and lib/ip; servers start through tcpd; telnetd/rlogind use the ptys and getty/login.

| Tool | Lines | Scan findings |
|---|---|---|
| slip | 323 | - |
| ifconfig | 352 | - |
| add_route | 347 | - |
| pr_routes | 307 | - |
| hostaddr | 311 | - |
| nonamed | 2165 | 2 int/ptr |
| ping | 151 | - |
| rdate | 109 | - |
| tcpd | 303 | - |
| tcpdp | 303 | - |
| ftp | 1946 | - |
| in.ftpd | 1971 | - |
| telnet | 701 | - |
| in.telnetd | 700 | 1 format |
| rsh | 568 | - |
| rcp | 848 | - |
| rlogin | 1777 | - |
| in.rshd | 445 | - |
| in.rlogind | 549 | - |
| urlget | 707 | - |
| nc | 152 | - |
| finger | 1091 | - |

### 10 Serial and terminals

Dependencies: tty00 and the console driver (roscrs232.c).

| Tool | Lines | Scan findings |
|---|---|---|
| kermit | 11402 | 5 implicit ptr/long, 1 int/ptr, 14 incompat. ptr, 1 format, 2 overflow |
| rz | 1549 | - |
| sz | 1755 | - |
| term | 539 | - |
| termcap | 163 | - |
| clr | 79 | - |
| tget | 98 | - |
| stty | 1277 | - |

### 11 Games and small programs

Dependencies: life and gomoku use libcurses.

| Tool | Lines | Scan findings |
|---|---|---|
| advent | 0 | - |
| life | 241 | 1 int/ptr |
| gomoku | 739 | - |
| ttt | 299 | - |
| fortune | 75 | - |
| banner | 153 | - |
| cal | 315 | - |
| calendar | 230 | - |
| factor | 53 | - |
| dhrystone | 585 | - |
| ic | 1048 | - |

## Suggested order

1. **The scan findings first** (concrete bugs, small fixes):
   * the %s without an argument: halt, grep, telnetd (wtmp);
   * the long formats: de, readfs, shutdown, kermit;
   * flex (realloc), awk (time, casts), diff (local address).
2. **Group 1: shells, login, accounts.** Every session uses them; ash and
   sh have a few casts.
3. **Group 0: the libraries, as far as group 1 uses them** (execl family,
   getpwent/crypt, stdio, malloc).
4. **Group 9: network**, with INET: the newest and least used code (two
   bugs found in Follow-ups 46-48).
5. **Groups 2 and 3: the everyday file and text tools.** Many small
   programs, two or four at a time.
6. **Groups 4 to 8 and 10, 11.**

**Dependencies:**
* Review a library before the tools that rely on what was found in it.
* The servers and their helpers belong together:
  * tcpd before in.ftpd, in.telnetd, in.rshd, in.rlogind;
  * getty/login before in.telnetd and in.rlogind;
  * sh, ls, tar and gzip before in.ftpd.
* man needs cawf and yap.
* cron needs sh and mail.

**In parallel (2 or 4):** each tool goes to a separate agent, which reads
the source, writes its fixes as a patch or sed file, and tests in its own
MAME session on its own card (`src/rosco/boot/*.img`). The cmds Makefile
and sdproto edits are merged one at a time.

## Progress

| Step | Tools | Result | Follow-up |
|---|---|---|---|
| 1 | halt, shutdown, grep, in.telnetd | 4 format bugs fixed, plus: shutdown 16-bit overflow at 10 hours, -x broken, pid file, buffer overruns; grep malloc one byte short; telnetd kill(-1) after a failed fork, child running on after exec failure | 50 |
| 1 | de, readfs | de: zone numbers and 'G' on V2/big endian, unsigned address test, inode overflow; readfs: V2 file systems read as V1, /bin/Mkdir, partial last block | 50 |
| 2 | flex | realloc without prototype (pointer cut to 16 bits), allocation sizes in 16-bit int, upstream -F uninitialized state 0 | 51 |
| 2 | kermit | the line speed in a 16-bit int (no speed above 32767 could be set), ctime without prototype, ttsndb, SIGINT after a shell escape; /usr/spool/locks was missing | 52 |
| 3 | diff, cdiff | line numbers past 32767 negative, -c/cdiff hunk buffers wrapping at 32K, long lines in -c, -b running past the string, isdir, warn format, -r header order | 53 |
| 3 | awk | pointer NULL tests through 16-bit int, time(0), %x/%o/%u and substr cut to 16 bits, undeclared jStrchr; lost statements in multi-statement rules, delete losing hash chains, $0 assignment freeing a static buffer, call leaks, gsub empty-match loop, regexp jumps over 255, EOF-comment hang, record/field overruns, system()/match()/split(re) | 54 |
| 1 | ash | stack block growth past 32767, expr result printed with %d (`expr 1 + 2 = 3` false), unbounded copy of edited lines, signal() result cast to int, `trap -`; stack 12kw → 32kw | 55 |
| 1 | login, getty, su; who, bin | login: shell argv not terminated, name overruns; getty: option injection (`-h`), off-by-one; su: $SHELL overflow; who stack 4kw → 8kw; `bin` account without a password in group 0 (path to root) locked; USAGE: tty00 needs a password | 56 |
| 1 | passwd, pwdauth, adduser; umask | passwd dropped /bin/sh shells, password buffers of 9 (stack overflow), unterminated salt, failed crypt stored as password, getpwuid NULL; umask 0 everywhere (now 022 in /etc/rc and /etc/profile) | 57 |
| 1 | cron, crontab, at | jobs without USER/HOME (putenv), clock set back stopped all jobs, CPU spin without spool/at/past, fd leak, crontab -c checked one file and installed another, at overwriting another user's job | 58 |
| 1 | sh (osh) | here document temp names colliding (pid*1000 in 16 bits), `set` leaking (out of string space after ~100), backquote zombies filling the process table, read() -1 as unsigned; stack 11kw → 16kw | 59 |
| 1 | mail, asktime, date | mail: temp files open to link/pre-creation attacks as root (truncate system files, mail /etc/shadow), mail lost when delivered during a session, false letter splits, NULL FILE, `*` memory fault (stack 8kw), empty -s mails not sent; asktime: -t 0 waited forever, long answers went to getty | 60 |
| 0 | libc (getpwent, getgrent, execl, execlp, execve, popen, TZ, ctype) | /etc/passwd read stopped at a last line without newline or a long line (passwd dropped the rest), execl with a bare 0, execve frames of 32K+, pclose losing other children, TZ offsets of 10 hours+, last-Sunday rules, ctype with chars 128-255 | 61 |
| 9 | slip, ifconfig, add_route, pr_routes, hostaddr, ping, rdate, nonamed | nonamed: missing return made the cache-save job loop forever at 100% CPU 5 minutes after the first cached answer, read() -1 as unsigned, stack 8kw → 16kw; ping: negative and >16K lengths, checksum length; hostaddr: uninitialised flag; open: INET psip keeps oversized packets (ping 3000 kills slip) | 62 |
| 9 | telnet, in.telnetd, rlogin, in.rlogind | telnetd: IAC NOP/SB ate all input, suboptions reached login, pty 0666 after logout (password snooping); rlogind: trust without a privileged port, user name as login option, 0377 bytes ended sessions; telnet: core dump on a split TERMTYPE, ~. left the tty raw; rlogin speed; sdproto ptys 600; open: ruserok trusts root via hosts.equiv, INET low ports for anyone | 63 |
