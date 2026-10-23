# Minix 2.0.4 on the rosco_m68k: interface catalogue

Written 2026-10-23 against branch `rosco-m68k` at `b039f1f`, plus the
uncommitted change in `src/fs/open.c` (symbolic links and `O_EXCL`, see
1.7). All `file:line` references are to that tree.

Minix is message passing. The kernel, its tasks, MM, FS, INIT, INET and the
user programs talk through fixed message formats. This catalogue lists
those formats and the data that is shared outside them, so that each
component can be reviewed and tested on its own against its interface. It
is written for the review of MM and FS that comes next.

Conventions:

* "MM", "FS": the servers in `src/mm`, `src/fs`. "Kernel": `src/kernel`
  (generic files) plus the rosco machine layer `src/kernel/rosc*`.
* Field names such as `PROC_NR` are the `#define`s of
  `include/minix/com.h`; names such as `name1_length` are those of
  `src/mm/param.h` and `src/fs/param.h`. The raw field (`m1_i1`, ...) is
  always given too.
* Errors are written as their names. Inside MM, FS and the kernel they are
  negative (`_SIGN` is `-` under `_SYSTEM`, `include/errno.h:23-28`); the C
  library turns a negative `m_type` into `errno` and -1
  (`src/lib/other/syscall.c`).
* "Not verified" marks statements taken from reading only, with no test
  behind them.

Contents:

1. Overview: components, send rules, message layout, what this port changed
2. Programs to MM and FS: the system calls
3. MM, FS and INET to the kernel: SYSTASK calls
4. FS to drivers: device messages and the dmap table
5. MM and FS, kernel to servers, clock and TTY
6. Shared data that is an interface without being a message
7. Review checklists per component
8. Observations found while writing this (not fixed)
9. How the tables were made

---

## 1. Overview

### 1.1 Components and process numbers

`NR_TASKS` is 9 (`include/minix/const.h:53`: 8 + `NR_CTRLRS` 1, all other
`ENABLE_*` 0). `NR_PROCS` is 32 (`src/rosco/include/minix/config.h:43`).
The kernel process table has 41 slots, numbered -9 .. 31
(`src/kernel/proc.h:131`, `proc_addr()` at `proc.h:107`).

| Nr | Name (`p_name`) | What | Defined | Entry | Priority |
|---|---|---|---|---|---|
| -9 | TTY | terminal task: console (DUART A), tty00 (DUART B), 4 ptys | `com.h:20` | `tty_task`, `kernel/tty.c:144` | task |
| -8 | ACSI | controller 0: SD card and RAM root image (`c0=ACSI` in the kernel environment, `roscmain.c:503`) | `com.h:85` (`CTRLR(0)`) | `acsi_task`, `roscdisk.c:83`, via `mapdrivers()` `table.c:166` | task |
| -7 | SYN_AL | synchronous alarm task (sends `CLOCK_INT`) | `com.h:88` | `syn_alrm_task`, `src/rosco/kernel/clock.c:391` | task |
| -6 | IDLE | idle loop | `com.h:90` | `idle_task` (`roscmain.c`) | idle |
| -5 | FLOPPY | no floppy: a fake task that never replies (see 8.1) | `com.h:92` | `floppy_task`, `roscmain.c:636` | task |
| -4 | MEMORY | `/dev/ram`, `/dev/mem`, `/dev/kmem`, `/dev/null`, `/dev/zero` | `com.h:94` | `mem_task`, `kernel/memory.c:55` | task |
| -3 | CLOCK | clock task | `com.h:102` | `clock_task`, `src/rosco/kernel/clock.c:102` | task |
| -2 | SYS | system task (SYSTASK) | `com.h:115` | `sys_task`, `kernel/system.c:170` | task |
| -1 | HARDWARE | pseudo source of interrupt and KSIG messages; no code | `com.h:143` | - | task |
| 0 | MM | memory manager | `const.h:35` | `src/mm/main.c:31` | server |
| 1 | FS | file system | `const.h:36` | `src/fs/main.c:60` | server |
| 2 | INIT | init, pid 1, from the boot image | `const.h:37` | `src/sttools/init.c` | user |
| 3..31 | - | user processes; INET is one until it signs on (5.8) | | | user, INET becomes server |

The kernel sets the priorities at boot (`roscmain.c:108-176`): tasks
`PPRI_TASK`, slots below `LOW_USER` (2) `PPRI_SERVER`, INIT `PPRI_USER`.
`SYS_SYSCTL`/`SYSSIGNON` later makes INET `PPRI_SERVER` (`system.c:1063-1074`).
INET is not in the boot image; `/etc/rc` starts it
(`src/rosco/boot/Makefile:1-3`).

The clock task is compiled from `src/rosco/kernel/clock.c`, which is
identical to `src/kernel/clock.c` (`diff` is empty;
`src/rosco/kernel/Makefile:13-14` explains why it is copied).

### 1.2 Who may send to whom

The only kernel calls are SEND, RECEIVE and BOTH (sendrec)
(`include/minix/com.h:2-4`), entered by `trap #0` with the function in `d0.w`, the partner in `d1.w`
and the message pointer in `a0` (`src/rosco/lib/sndrec.S`,
`roscompx.S:332-349`). The result comes back in `d0`.

Rules in `sys_call()` and `mini_send()` (`src/kernel/proc.c`):

| Rule | Where | Error |
|---|---|---|
| partner must be a valid process number or `ANY` | `proc.c:144` | `E_BAD_DEST` |
| a user process may only use BOTH | `proc.c:147` | `E_NO_PERM` |
| a user process may only send to MM or FS | `proc.c:181`, `issysentn` `proc.h:102` | `E_BAD_DEST` |
| the destination slot must be in use | `proc.c:183` | `E_BAD_DEST` |
| on SEND the message must lie between the start of data and the end of stack (`ALLOW_GAP_MESSAGES` 1) | `proc.c:185-195` | `EFAULT` |
| sending to a process that is (transitively) sending to the caller fails | `proc.c:207-216` | `ELOCKED` |
| RECEIVE checks nothing ("calls from the tasks, MM, and FS are trusted") | `proc.c:255-260` | - |

Servers (MM, FS, INET after sign-on) and tasks may send, receive or
sendrec with anyone. Nothing restricts which server calls which SYSTASK
function (3.1).

A message is copied with `cp_mess()` (`proc.c:580-613`), which corrects
both addresses for shadowed processes and then overwrites `m_source` with
the real sender. A receiver can trust `m_source`; it cannot trust any other
field.

### 1.3 Communication patterns

There is no `notify()` in 2.0.4. The patterns are:

| Pattern | Used by | Notes |
|---|---|---|
| sendrec, reply by send | every system call; SYSTASK, CLOCK, drivers | the server replies with `send()`; the caller is already blocked in RECEIVE, so the send does not block |
| deferred reply | MM (`SUSPEND` from a handler, reply later through `setreply()`, `mm/main.c:52-65`); FS (`SUSPEND`, later `reply()` or `revive()`) | MM never replies to EXIT and EXEC |
| REVIVE from a driver | TTY, PTY, INET to FS | the driver first answered `TASK_REPLY` with `REP_STATUS = SUSPEND`, later sends `REVIVE` (4.2) |
| HARD_INT | kernel `interrupt()` to a task (`proc.c:52-124`) | not queued: one pending flag `p_int_blocked` |
| KSIG | kernel `inform()` to MM (`system.c:1203-1231`) | source `HARDWARE`; sent only when MM waits in `receive(ANY)` (`proc.c:302`) |
| CLOCK_INT | SYN_AL to INET (`clock.c:381-386`) | plain `send()`: SYN_AL blocks until INET receives |

Deadlocks between FS and a driver are resolved by `ELOCKED`: if a driver is
sending `REVIVE` to FS while FS sendrecs the driver, FS's send fails with
`ELOCKED`, FS receives the REVIVE and retries (`fs/device.c:274-298`).

### 1.4 Message layout under -mshort

`include/minix/type.h:38-81`. The rosco build defines `__MSHORT__`, not
`__MLONG__` (`src/rosco/boot/Makefile:31`, `src/rosco/lib/Makefile:39`), so
the `int` variant (`type.h:62-80`) is compiled; with `-mshort` an `int` is 2
bytes, so it is the same as the `short` variant. m68k-elf-gcc aligns `long`
and pointers to 2 bytes, so there is no padding. The offsets below were
checked by compiling `offsetof()` with the project's flags (section 9).

| Field | Type | Size | Offset |
|---|---|---|---|
| `m_source` | int | 2 | 0 |
| `m_type` | int | 2 | 2 |
| union `m_u` | | 22 | 4 |
| **whole message** | | **26** | |

| mess_1 (18) | mess_2 (18) | mess_3 (22) | mess_4 (20) | mess_5 (18) | mess_6 (14) |
|---|---|---|---|---|---|
| `m1_i1` int @4 | `m2_i1` int @4 | `m3_i1` int @4 | `m4_l1` long @4 | `m5_c1` char @4 | `m6_i1` int @4 |
| `m1_i2` int @6 | `m2_i2` int @6 | `m3_i2` int @6 | `m4_l2` long @8 | `m5_c2` char @5 | `m6_i2` int @6 |
| `m1_i3` int @8 | `m2_i3` int @8 | `m3_p1` ptr @8 | `m4_l3` long @12 | `m5_i1` int @6 | `m6_i3` int @8 |
| `m1_p1` ptr @10 | `m2_l1` long @10 | `m3_ca1` char[14] @12 | `m4_l4` long @16 | `m5_i2` int @8 | `m6_l1` long @10 |
| `m1_p2` ptr @14 | `m2_l2` long @14 | | `m4_l5` long @20 | `m5_l1` long @10 | `m6_f1` ptr @14 |
| `m1_p3` ptr @18 | `m2_p1` ptr @18 | | | `m5_l2` long @14 | |
| | | | | `m5_l3` long @18 | |

Every `int` field is 16 bits. Counts, ioctl codes, uids, fds, pids,
process numbers and status values are therefore limited to 16 bits. Only the
`l` fields and pointers carry 32 bits.

Places where a sender and a receiver use different message types and rely
on equal offsets:

| Sender writes | Receiver reads | Offset | Where |
|---|---|---|---|
| `open()` with `O_CREAT`: flags in `m1_i2` | FS `mode` = `m3_i2` | 6 | `lib/posix/_open.c`, `fs/open.c:66` |
| `mkdir()`: mode in `m1_i2` | FS `mode` = `m3_i2` | 6 | `lib/posix/_mkdir.c`, `fs/open.c:397` |
| `ptrace()`: pid in `m2_i1` | MM `pid` = `m1_i1` | 4 | `lib/posix/_ptrace.c`, `mm/trace.c:48` |
| `ioctl()`: fd in `TTY_LINE` = `m2_i1` | FS `ls_fd` = `m2_i1` | 4 | `lib/posix/_ioctl.c`, `fs/device.c:226` |

`m3_ca1` is 14 bytes (`M3_STRING`, `type.h:36`). `_loadname()` puts a
path of up to 14 bytes including the NUL into the message, a longer one is
passed by pointer only (`lib/other/loadname.c`); FS's `fetch_name()` takes
the inline copy only for `M3` calls (`fs/utility.c:72-80`).

### 1.5 Error and return values

* A system call result travels in `m_type`, a 16-bit int. Results that do
  not fit are passed in other fields: `lseek` and `time` in `m2_l1`, `times`
  in `m4_l1..m4_l5`, `brk` in `m2_p1`.
* Kernel IPC errors are `E_BAD_DEST` (-1001) .. `E_BAD_PROC` (-1011)
  (`include/errno.h:103-111`) and `ELOCKED`. `_syscall()` stores a failed
  sendrec status in `m_type` (`lib/other/syscall.c`), so a program sees
  `errno` 1001 etc.
* `SUSPEND` is -998 (`com.h:33`). It is a value inside MM, FS and drivers,
  never a result a program sees.
* An in-range call number that a server does not handle reaches `no_sys()`,
  which returns `EINVAL` (`mm/utility.c:62-67`, `fs/utility.c:89`). A
  number >= `NCALLS` (78) gives `ENOSYS` (`mm/main.c:45-46`,
  `fs/main.c:88-89`).

### 1.6 Build facts that change interfaces

* `int`, `unsigned` and `size_t` are 16 bits, `long` and pointers 32 bits,
  big-endian (`CLAUDE.md`). `pid_t`, `uid_t`, `dev_t`, `ino_t`, `mode_t`
  and `nlink_t` are 2 bytes, `gid_t` is 1 byte, `off_t` and `time_t` 4
  bytes (compiled, section 9).
* `src/rosco/include/rosco_sizeof.h` makes every `sizeof(x)` an `unsigned
  int`, as with ACK. Comparisons of an `int` with `sizeof` are unsigned.
* `read()`/`write()` counts above 32767 fail with `EINVAL`: `nbytes` is
  the signed 16-bit `m1_i2` and FS rejects negative counts
  (`fs/read.c:72`).
* `ARG_MAX` is 4096 (`include/limits.h:65`), `PATH_MAX` 255, `OPEN_MAX` 20.
* ioctl codes are 16 bits with no size encoding (`include/minix/ioctl.h:33-40`).
  A driver learns the parameter size from the code alone.
* No MMU: `SHADOWING` is 1 (`config.h:234`), a click is 256 bytes
  (`const.h:67`), a process's virtual addresses equal its physical
  addresses (6.1).

### 1.7 What this port changed in the generic code

`git log` shows only two commits touching the generic kernel, MM and FS
sources since the import, plus one uncommitted change:

| Change | Where | Effect on an interface |
|---|---|---|
| `umap()`: the shadow offset applies only to addresses from the data base up; text below the data stays in place (commit `9e9c3dd`) | `kernel/system.c:1292-1299` | every SYSTASK call that maps a user address (`SYS_COPY`, `SYS_VCOPY`, ...) and all drivers using `numap()` now see the right bytes for text addresses of a shadowed process |
| `do_sigreturn()`: the condition codes (X N Z V C) come from the sigcontext, the supervisor byte stays (commit `f0a8cd6`) | `kernel/system.c:658-666` | `SYS_SIGRETURN` restores the interrupted code's CCR |
| `new_node()` gets a `follow` flag: `open(O_CREAT|O_EXCL)`, `mknod`, `mkdir` and `symlink` do not follow a symbolic link as the last component, they fail with `EEXIST` (uncommitted) | `fs/open.c:105-110`, `218-220`, `253-254`, `361-364`, `398`, `579` | OPEN, MKNOD, MKDIR, SYMLINK |

Neither kernel change carries the `rosco:` marker that `CLAUDE.md` asks
for (8.9).

Port-specific configuration (`src/rosco/include/minix/config.h`):
`NR_PROCS` 32, `NR_BUFS` 128, `ENABLE_SWAP` 0, `ENABLE_CACHE2` 0,
`ENABLE_PRINTER` 0, `NR_FD_DRIVES` 0, `NR_CONS` 1, `NR_RS_LINES` 1,
`NR_PTYS` 4, `ENABLE_NETWORKING` 0 (INET runs as a program),
`OPTIMIZE_FOR_SPEED` 0, `OLDSIGNAL_COMPAT` 1, `ENABLE_SYMLINKS` 1,
`ATARI_TYPE ROSCO`.

The machine layer replaces the ST files: `roscmain.c` (boot, `trap()` to
signal mapping, `move sr` emulation `prv_emulate()`, stack check, fake
tasks, kernel environment), `roscdisk.c` (controller 0), `roscons.c` and
`roscrs232.c` (DUART lines for TTY), `roscompx.S` (vectors, `trap #0`
entry, `sizes[]`), `roscklib.S`, `roscstub.c`. The C library has GNU
replacements in `src/rosco/lib` (`sndrec.S`, `crtso.S`, `sigret.S`,
`_execve.c`, ...). INET has fixed copies in `src/rosco/boot/inetfix`.

---

## 2. Programs to MM and FS

### 2.1 How a call is made

The library fills a message and calls `_syscall(MM or FS, number, &m)`
(`src/lib/other/syscall.c`), which sets `m_type` and does `_sendrec()`.
`_syscall` returns `m_type`, or -1 with `errno = -m_type` if negative.
MM and FS read their parameters through the names in `mm/param.h` and
`fs/param.h`. MM keeps one reply message per process (`mp_reply`,
`mm/mproc.h:43`); FS has one global reply message `m1` (`fs/param.h:91-99`,
`fs/main.c:148-159`). Neither clears the reply between calls: a reply field
that a call does not set holds a value of an earlier call (in FS, of any
process). Programs must only read the fields of their call.

Call numbers are in `include/minix/callnr.h`; the dispatch tables are
`src/mm/table.c:16-100` and `src/fs/table.c:18-107`. In the tables below,
"m" means the request message, "reply" the reply message.

### 2.2 Calls handled by MM

| # | Call | Handler | Request | Reply | Main errors | Library |
|---|---|---|---|---|---|---|
| 1 | EXIT | `do_mm_exit` `mm/forkexit.c:132` → `mm_exit` `:146` | `status` = `m1_i1` | none (`SUSPEND`); status kept for WAIT | - | `lib/posix/__exit.c` |
| 2 | FORK | `do_fork` `forkexit.c:34` | - | parent: child pid; child: 0 (`setreply` `:124`) | `EAGAIN` (table full; last 2 slots only for root, `:48-49`), `ENOMEM` | `_fork.c` |
| 7 | WAIT | `do_waitpid` `forkexit.c:227` | - | child pid; status in `m2_i1` (`reply_res2`) | `ECHILD`, `EINTR` | `_wait.c` |
| 11 | WAITPID | `do_waitpid` | `pid` `m1_i1`, options (`sig_nr`) `m1_i2` | child pid or 0 (`WNOHANG`); status `m2_i1` | `ECHILD`, `EINTR` | `_waitpid.c` |
| 17 | BRK | `do_brk` `mm/break.c:29` | `addr` `m1_p1` | 0; new break or -1 in `m2_p1` (`reply_ptr`) | `ENOMEM` | `lib/other/_brk.c` (`brk`, `sbrk`) |
| 20 | GETPID | `do_getset` `mm/getset.c:16` | - | pid; parent's pid in `m2_i1` | - | `_getpid.c`, `_getppid.c` |
| 23 | SETUID | `do_getset` | uid `m1_i1` | 0; also `tell_fs(SETUID)` | `EPERM` | `_setuid.c` |
| 24 | GETUID | `do_getset` | - | real uid; effective uid in `m2_i1` | - | `_getuid.c`, `_geteuid.c` |
| 26 | PTRACE | `do_trace` `mm/trace.c:36` | pid `m1_i1` (library writes `m2_i1`, same offset), `request` `m2_i2`, `taddr` `m2_l1`, `data` `m2_l2` | 0; data in `m2_l2` | `ESRCH`, `EIO` | `_ptrace.c` |
| 27 | ALARM | `do_alarm` `mm/signal.c:319` | `seconds` `m1_i1` | seconds left of the old alarm | - | `_alarm.c` |
| 29 | PAUSE | `do_pause` `signal.c:385` | - | only `EINTR` after a signal | `EINTR` | `_pause.c` |
| 37 | KILL | `do_kill` `signal.c:236` → `check_sig` `:520` | pid `m1_i1`, sig `m1_i2` | 0; no reply if the caller killed itself (`:576`) | `EINVAL`, `ESRCH`, `EPERM` | `_kill.c` |
| 46 | SETGID | `do_getset` | gid `m1_i1`, cast to the 8-bit `gid_t` (`mm/param.h:6`) | 0 | `EPERM` | `_setgid.c` |
| 47 | GETGID | `do_getset` | - | real gid; effective gid in `m2_i1` | - | `_getgid.c`, `_getegid.c` |
| 48 | SIGNAL | `do_signal` `signal.c:47` (`OLDSIGNAL_COMPAT`) | `sig` `m6_i1`, `func` `m6_f1` | 0, or 1 if the signal was ignored before | `EINVAL` | none: old binaries only; `signal()` is built on SIGACTION |
| 59 | EXEC | `do_exec` `mm/exec.c:51` | path length `m1_i1`, frame size `m1_i2`, path `m1_p1`, frame `m1_p2` (6.7) | none on success: the kernel restarts the process (`SYS_EXEC`) | `ENOMEM` (frame > `ARG_MAX`, no memory), `EINVAL` (path length), `EACCES`, `ENOENT` etc. (from `access`/`open`), `ENOEXEC` | `src/rosco/lib/_execve.c` (and `_execl.c`, `_execlp.c`, `lib/posix/_execv*.c`) |
| 62 | SETSID | `do_getset` | - | new process group; also `tell_fs(SETSID)` | `EPERM` (already leader) | `_setsid.c` |
| 63 | GETPGRP | `do_getset` | - | process group | - | `_getpgrp.c` |
| 64 | KSIG | `do_ksig` `signal.c:247` | `SIG_PROC` `m2_i1`, `SIG_MAP` `m2_l1` | none | `EPERM` if the sender is not `HARDWARE` | kernel `inform()` only (5.4) |
| 71 | SIGACTION | `do_sigaction` `signal.c:95` | sig `m1_i2`, new action `m1_p1`, old action `m1_p2`, address of `__sigreturn` `m1_p3` | 0 | `EINVAL`, copy errors | `_sigaction.c` |
| 72 | SIGSUSPEND | `do_sigsuspend` `signal.c:203` | mask `m2_l1` | `EINTR` after the handler | - | `_sigsuspend.c` |
| 73 | SIGPENDING | `do_sigpending` `signal.c:143` | - | 0; pending set in `m2_l1` | - | `_sigpending.c` |
| 74 | SIGPROCMASK | `do_sigprocmask` `signal.c:152` | how `m2_i1` (`SIG_INQUIRE` if no set), set `m2_l1` | 0; old mask in `m2_l1` | `EINVAL` | `_sigprocmask.c` |
| 75 | SIGRETURN | `do_sigreturn` `signal.c:217` | mask `m2_l1`, flags `m2_i2`, sigcontext pointer `m2_p1` | result of `SYS_SIGRETURN`; normally unseen: the process resumes with the restored registers, the reply lands in the static message of `_sigreturn.c` | `EFAULT`, `EINVAL` | `lib/posix/_sigreturn.c`, reached from `__sigreturn` (`src/rosco/lib/sigret.S`) |
| 76 | REBOOT | `do_reboot` `mm/misc.c:19` | how `m1_i1`, monitor code `m1_p1`, size `m1_i2` | none on success | `EPERM` (not root), `EINVAL`, `EFAULT` | `lib/other/_reboot.c` |
| 77 | SVRCTL | `do_svrctl` `mm/misc.c:55` | request `m2_i1`, argument pointer `m2_p1` | 0 | `EPERM`, `EINVAL`, kernel errors for `'S'` requests | `lib/other/_svrctl.c` (requests `'M'` and `'S'`) |

FORK, EXIT and EXEC also send `tell_fs()` messages and SYSTASK calls (5.1, 3.1).

### 2.3 Calls handled by FS

| # | Call | Handler | Request | Reply | Main errors | Library |
|---|---|---|---|---|---|---|
| 3 | READ | `do_read` `fs/read.c:33` → `read_write` `:41` | `fd` `m1_i1`, `nbytes` `m1_i2`, `buffer` `m1_p1` | bytes read | `EBADF`, `EINVAL` (count < 0, i.e. > 32767), `EIO`, `EAGAIN`, `EINTR`; may suspend (pipe, tty, INET) | `_read.c` |
| 4 | WRITE | `do_write` `fs/write.c:26` → `read_write` | as READ | bytes written | as READ, `EFBIG`, `ENOSPC`, `EPIPE` (and `SIGPIPE` through `SYS_KILL`, `fs/pipe.c:119`) | `_write.c` |
| 5 | OPEN | `do_open` `fs/open.c:58` → `common_open` | without `O_CREAT`: m3 (length `m3_i1`, path `m3_p1`/`m3_ca1`, flags `m3_i2`); with `O_CREAT`: m1 (length `m1_i1`, flags `m1_i2`, mode `m1_i3`, path `m1_p1`) | fd | `ENOENT`, `EACCES`, `EEXIST`, `EISDIR`, `EMFILE`, `ENFILE`, `ENXIO`, `ELOOP`, `ENAMETOOLONG`, driver errors; may suspend (FIFO open) | `_open.c` |
| 6 | CLOSE | `do_close` `open.c:437` | `fd` `m1_i1` | 0 | `EBADF` | `_close.c` |
| 8 | CREAT | `do_creat` `open.c:44` | m3 path, mode `m3_i2` | fd | as OPEN | `_creat.c` |
| 9 | LINK | `do_link` `fs/link.c:35` | lengths `m1_i1`, `m1_i2`, names `m1_p1`, `m1_p2` | 0 | `EXDEV`, `EEXIST`, `EMLINK`, `EPERM` (directory, not root), path errors | `_link.c` |
| 10 | UNLINK | `do_unlink` `link.c:110` | m3 path | 0 | `EBUSY`, `EPERM`, path errors | `_unlink.c` |
| 12 | CHDIR | `do_chdir` `fs/stadir.c:35` | m3 path; from MM: slot `m1_i1` (5.1) | 0 | `ENOTDIR`, path errors | `_chdir.c` |
| 13 | TIME | `do_time` `fs/time.c:63` | - | 0; seconds in `m2_l1` | - | `_time.c` |
| 14 | MKNOD | `do_mknod` `open.c:349` | length `m1_i1`, mode `m1_i2`, device `m1_i3`, path `m1_p1` (`m1_p2` unused) | 0 | `EPERM` (not root and not a FIFO), `EEXIST` | `_mknod.c` |
| 15 | CHMOD | `do_chmod` `fs/protect.c:25` | m3 path, mode `m3_i2` | 0 | `EPERM`, `EROFS` | `_chmod.c` |
| 16 | CHOWN | `do_chown` `protect.c:64` | length `m1_i1`, owner `m1_i2`, group `m1_i3` (8-bit `gid_t`), path `m1_p1` | 0 | `EPERM` | `_chown.c` |
| 18 | STAT | `do_stat` `stadir.c:125` | length `m1_i1`, path `m1_p1`, buffer `m1_p2` | 0; `struct stat` (30 bytes) copied to the buffer | path errors, `EFAULT` | `_stat.c` |
| 19 | LSEEK | `do_lseek` `open.c:512` | `ls_fd` `m2_i1`, `offset` `m2_l1`, `whence` `m2_i2` | 0; new position in `m2_l1` | `EBADF`, `ESPIPE`, `EINVAL` | `_lseek.c` |
| 21 | MOUNT | `do_mount` `fs/mount.c:25` | lengths `m1_i1`, `m1_i2`, read-only `m1_i3`, special `m1_p1`, directory `m1_p2` | 0 | `EPERM`, `EBUSY`, `ENOTBLK`, `ENFILE`, `EINVAL` | `_mount.c` |
| 22 | UMOUNT | `do_umount` `mount.c:123` | m3 path | 0 | `EPERM`, `EBUSY`, `EINVAL` | `_umount.c` |
| 25 | STIME | `do_stime` `time.c:76` | `tp` `m2_l1` | 0 | `EPERM` | `_stime.c` |
| 28 | FSTAT | `do_fstat` `stadir.c:146` | `fd` `m1_i1`, buffer `m1_p1` | 0 | `EBADF` | `_fstat.c` |
| 30 | UTIME | `do_utime` `time.c:23` | length `m2_i1` (0 means NULL times, length then in `m2_i2`), atime `m2_l1`, mtime `m2_l2`, path `m2_p1` | 0 | `EPERM`, `EROFS`, `EACCES` | `_utime.c` |
| 33 | ACCESS | `do_access` `protect.c:118` | m3 path, mode `m3_i2` | 0 | `EACCES`, `EINVAL` | `_access.c` |
| 36 | SYNC | `do_sync` `fs/misc.c:134` | - | 0 | - | `_sync.c` |
| 38 | RENAME | `do_rename` `link.c:169` | as LINK | 0 | `EXDEV`, `EINVAL`, `EISDIR`, `ENOTDIR`, `EBUSY`, `EMLINK` | `_rename.c` |
| 39 | MKDIR | `do_mkdir` `open.c:373` | length `m1_i1`, mode `m1_i2` (read as `m3_i2`), path `m1_p1` | 0 | `EEXIST`, `EMLINK` | `_mkdir.c` |
| 40 | RMDIR | `do_unlink` `link.c:110` | m3 path | 0 | `ENOTEMPTY`, `EBUSY`, `EINVAL` | `_rmdir.c` |
| 41 | DUP | `do_dup` `misc.c:36` | `fd` `m1_i1` (bit 0100 set: dup2), `fd2` `m1_i2` | new fd | `EBADF`, `EMFILE` | none: `dup`/`dup2` use FCNTL (`_dup.c`, `_dup2.c`); old binaries only |
| 42 | PIPE | `do_pipe` `fs/pipe.c:30` | - | 0; fds in `m1_i1`, `m1_i2` | `EMFILE`, `ENFILE`, inode allocation errors | `_pipe.c` |
| 43 | TIMES | `do_tims` `time.c:93` | - | 0; user, system, child user, child system times and uptime in `m4_l1..m4_l5` | - | `_times.c` |
| 49 | RDLNK | `do_rdlink` `stadir.c:237` | length `m1_i1`, buffer size `m1_i2`, path `m1_p1`, buffer `m1_p2` | length of the link | `EINVAL` (not a link), path errors | `lib/other/_readlink.c` |
| 50 | LSTAT | `do_lstat` `stadir.c:208` | as STAT | 0 | as STAT | `lib/other/_lstat.c` |
| 54 | IOCTL | `do_ioctl` `fs/device.c:218` | `ls_fd`/`TTY_LINE` `m2_i1`, request `REQUEST`/`TTY_REQUEST` `m2_i3`, argument `ADDRESS` `m2_p1` | driver status; for old sgtty codes (`'t'`) also `TTY_SPEK` `m2_l1` and `TTY_FLAGS` `m2_l2` (`device.c:233-251`) | `EBADF`, `ENOTTY` (not a device), driver errors; may suspend | `_ioctl.c` |
| 55 | FCNTL | `do_fcntl` `misc.c:74` | `fd` `m1_i1`, command `m1_i2`, int argument `m1_i3`, `struct flock *` `m1_p1` | per command | `EBADF`, `EINVAL`, `EMFILE`, `ENOLCK`, `EAGAIN`; `F_SETLKW` may suspend | `_fcntl.c` |
| 57 | SYMLINK | `do_symlink` `open.c:549` | as LINK (name 1 the contents, name 2 the new link) | 0 | `EEXIST`, path errors | `lib/other/_symlink.c` |
| 60 | UMASK | `do_umask` `protect.c:104` | complement mode `m1_i1` | old mask | - | `_umask.c` |
| 61 | CHROOT | `do_chroot` `stadir.c:74` | m3 path | 0 | `EPERM` | `_chroot.c` |
| 77 | SVRCTL | `do_svrctl` `misc.c:385` | request `m2_i1`, argument `m2_p1` | 0 | `EPERM`, `ENODEV`, `EBUSY`, `EINVAL`, `EFAULT` | `lib/other/_svrctl.c` (requests `'F'` and `'I'`; FS knows only `FSSIGNON`) |

"Path errors" are those of `fetch_name()` and the path walk: `EINVAL`
(length <= 0), `ENAMETOOLONG`, `ENOENT`, `ENOTDIR`, `EACCES`, `ELOOP`.

### 2.4 Numbers that are not system calls

| # | Name | Handled by | Sender | Notes |
|---|---|---|---|---|
| 1, 2, 23, 46, 59, 62, 76 | EXIT, FORK, SETUID, SETGID, EXEC, SETSID, REBOOT at FS | FS `do_exit`, `do_fork`, `do_set`, `do_exec`, `do_setsid`, `do_reboot` | MM only, via `tell_fs()` | others get `EGENERIC` (`ENOSYS` for SETSID) |
| 12 | CHDIR from MM | `do_chdir` `stadir.c:45-62` | MM | MM borrows a user's FS identity (5.1) |
| 64 | KSIG | MM `do_ksig` | kernel (`HARDWARE`) | 5.4 |
| 65 | UNPAUSE | FS `do_unpause` `pipe.c:270` | MM (and tasks: only `who > MM_PROC_NR` is refused) | 5.1 |
| 67 | REVIVE | FS `do_revive` `misc.c:364` | tasks, servers (`fp_pid == PID_SERVER`) | 4.2 |
| 68 | TASK_REPLY | nobody (`no_sys`) | drivers, as replies | must never reach a server main loop |
| 0, 31, 32, 34, 35, 44, 45, 51-53, 56, 58, 66, 69, 70 | unused | `no_sys` in both | | `EINVAL` |

### 2.5 Calls that behave specially on this port

* READ, WRITE: at most 32767 bytes per call (1.6). The library does not
  split larger transfers (the `__MLONG__` chunking in `_read.c` is not
  compiled).
* EXEC: frame at most `ARG_MAX` 4096 bytes; `_execve.c` returns `E2BIG`
  before calling MM (`src/rosco/lib/_execve.c:74-85`). Programs are
  relocated by MM (6.6).
* FORK: no copy is made; the parent's data and stack are copied to a
  shadow area and the child runs in place (`kernel/stshadow.c`, 3.1
  `SYS_FORK`).
* SETGID, CHOWN: group ids are 8 bits.
* GETUID etc.: a uid above 32767 comes back as a negative `m_type` and the
  library reports an error.
* IOCTL: request codes are 16 bits (1.6).
* OPEN with `O_CREAT|O_EXCL`, MKNOD, MKDIR, SYMLINK on a dangling symbolic
  link: `EEXIST` (1.7, uncommitted).
* No swapping (`ENABLE_SWAP` 0): `MMSWAPON`/`MMSWAPOFF` are `EINVAL`.
* SIGNAL (48) and DUP (41) exist only for old binaries.

---

## 3. MM, FS and INET to the kernel (SYSTASK)

### 3.1 The SYSTASK calls

`sys_task()` (`kernel/system.c:170-209`) receives from `ANY`, dispatches on
`m_type`, puts the result in `m_type` and sends the message back. Unknown
types give `E_BAD_FCN`. User processes cannot reach SYSTASK (1.2); every
server and task can call every function. The library wrappers are in
`src/lib/syslib/sys_*.c` (prototypes `include/minix/syslib.h`) and use
`_taskcall()`, which returns the result (negative on error) instead of
setting `errno`.

The comment table at `system.c:36-99` is partly stale (it lists the stack
pointer of SYS_EXEC under `PID`); the code and `src/lib/syslib` agree with
the table below.

| # | SYS_* (`com.h`) | Callers | Request | Kernel action | Reply |
|---|---|---|---|---|---|
| 1 | SYS_XIT `:116` | MM `forkexit.c:185` | `PROC1` `m1_i1` parent, `PROC2` `m1_i2` exiting slot | `do_xit` `:394`: add the child's times to the parent, `cancel_alarm`, unready, `rmshadow()`, name `<noname>`, remove it from send queues, clear pending signals, slot `PPRI_NONE` | OK; with shadowing `m1_i1` base and `m1_i2` size (clicks) of the memory MM must free |
| 2 | SYS_GETSP `:117` | MM `break.c:52,187`, `signal.c:453,671` | `PROC1` | `do_getsp` `:467` (asserts user or server: INET uses `sbrk`) | `STACK_PTR` `m1_p1` = saved sp |
| 3 | SYS_OLDSIG `:118` | MM `signal.c:486` (handler set by SIGNAL) | `PR` `m6_i1`, `SIGNUM` `m6_i2`, `FUNC` `m6_f1` | `do_oldsig` `:689`: push 8 bytes (int signal, u16 psw, long pc, `build_sig` `:885`) on the user stack, pc = handler; panics if the stack is not mapped | OK, `E_BAD_PROC` |
| 4 | SYS_FORK `:119` | MM `forkexit.c:112` | `PROC1` parent, `PROC2` child, `PID` `m1_i3`, `m1_p1` = child base in clicks (shadowing) | `do_fork` `:218`: copy the proc entry, clear `PENDING`/`SIG_PENDING`/`P_STOP`, child pid, child `d0` = 0, times 0; `mkshadow(parent, base)` copies the parent's data and stack to `base` and marks the parent shadowed; the child runs in place first (`:262-263`) | OK |
| 5 | SYS_NEWMAP `:120` | MM `break.c:119`; inside `do_fresh` | `PROC1`, `MEM_PTR` `m1_p1` = address of `struct mem_map[3]` in the caller | `do_newmap` `:273`: copy 18 bytes of map; with shadowing nothing else (`NO_MAP` is for the MMU case) | OK, `E_BAD_PROC` |
| 6 | SYS_COPY `:121` | MM `exec.c:85,92,138`, `signal.c:105,113`, `misc.c:34`; FS `utility.c:79`, `read.c:267,272`, `stadir.c:199,263`, `lock.c:36,144`, `misc.c:396` | `SRC_SPACE` `m5_c1`, `SRC_PROC_NR` `m5_i1`, `SRC_BUFFER` `m5_l1`, `DST_SPACE` `m5_c2`, `DST_PROC_NR` `m5_i2`, `DST_BUFFER` `m5_l2`, `COPY_BYTES` `m5_l3` | `do_copy` `:770`: `umap()` both sides, `phys_copy`. With shadowing `ABS` is not supported (`:811-813`) | OK, `EFAULT` |
| 7 | SYS_EXEC `:122` | MM `exec.c:191` | `PROC1`, `PROC2` traced flag, `STACK_PTR` `m1_p1` new sp, `NAME_PTR` `m1_p2` program name in MM, `IP_PTR` `m1_p3` start pc | `do_exec` `:347`: set sp, low-water mark, pc; clear `RECEIVING` (MM does not reply), ready; copy 15 chars of name; `SIGTRAP` if traced | OK |
| 8 | SYS_TIMES `:123` | FS `time.c:99` | `PROC1` | `do_times` `:484` | `USER_TIME` `m4_l1`, `SYSTEM_TIME` `m4_l2`, `CHILD_UTIME` `m4_l3`, `CHILD_STIME` `m4_l4`, `BOOT_TICKS` `m4_l5` |
| 9 | SYS_ABORT `:124` | MM `utility.c:86`, `misc.c:48`; FS `utility.c:115` | how `m1_i1` (`RBT_*`); `RBT_MONITOR` fields `m1_i2`, `m1_p1`, `m1_i3` are ignored on this machine (`:512-524`) | `do_abort` `:508` → `wreboot(how)` | does not return |
| 10 | SYS_KILL `:125` | FS `pipe.c:119` (SIGPIPE) | `PR` `m6_i1`, `SIGNUM` `m6_i2` | `do_kill` `:729` → `cause_sig()` (5.4) | OK |
| 11 | SYS_UMAP `:126` | no caller in MM, FS or INET | `SRC_SPACE`, `SRC_PROC_NR`, `SRC_BUFFER`, `COPY_BYTES` | `do_umap` `:907` | OK always; physical address or 0 in `SRC_BUFFER` |
| 12 | SYS_MEM `:127` | MM `alloc.c:269` (raw sendrec at start-up) | - | `do_mem` `:863`: hand out the next free chunk of `mem[]` and zero it in the table | `m1_i1` base, `m1_i2` size (0: no more), `m1_i3` total memory, all in clicks (16-bit fields; fine up to 8MB, see 6.10) |
| 13 | SYS_TRACE `:128` | MM `trace.c:70,104`, `signal.c:683` (core dump reads the proc entry with `T_GETUSER` = 3) | `m2_i1` proc, `m2_i2` request, `m2_l1` address, `m2_l2` data | `do_trace` `:929` | OK and data in `m2_l2`; `EIO` |
| 14 | SYS_VCOPY `:129` | INET `src/rosco/boot/inetfix/sr.c:708,757` | `m1_i1` source proc, `m1_i2` destination proc, `m1_i3` count (<= `CPVEC_NR` 16), `m1_p1` `cpvec_t[]` in the caller | `do_vcopy` `:824`: `numap()` (D space) each entry | OK, `EDOM`, `EFAULT` |
| 15 | SYS_SENDSIG `:131` | MM `signal.c:489` | `PROC1`, `SIG_CTXT_PTR` `m1_p1` = `struct sigmsg` in MM | `do_sendsig` `:562`: build `sigcontext` and `sigframe` below the user sp (6.8), set sp, pc, fp | OK, `EFAULT` |
| 16 | SYS_SIGRETURN `:132` | MM `signal.c:228` | `PROC1`, `m1_i2` flags (unused), `SIG_CTXT_PTR` `m1_p1` = user's sigcontext | `do_sigreturn` `:625`: copy the sigcontext in, require `SC_SIGCONTEXT`, restore registers; CCR from the sigcontext (`:658-666`, rosco change) | OK, `EFAULT`, `EINVAL` |
| 17 | SYS_ENDSIG `:133` | MM `signal.c:310` | `PROC1` | `do_endsig` `:747`: one KSIG signal done; ready the process when `p_pendcount` reaches 0 | OK, `E_BAD_PROC` |
| 18 | SYS_GETMAP `:134` | MM `main.c:137,149` | `PROC1`, `MEM_PTR` | `do_getmap` `:315` | OK |
| 19 | SYS_SYSCTL `:135` | MM `misc.c:65` (svrctl `'S'` for a process), FS `main.c:261` (`SYSGETENV`) | `m2_i1` proc, `m2_i2` request, `m2_i3` privileged flag, `m2_p1` argument | `do_sysctl` `:1048`: `SYSSIGNON` (needs privilege and a user process; makes it `PPRI_SERVER`, `p_pid` 0, writes `struct systaskinfo`), `SYSGETENV` (one variable or the whole environment) | OK, `EPERM`, `EFAULT`, `EINVAL`, `ESRCH`, `E2BIG` |
| 20 | SYS_PUTS `:136` | `lib/syslib/putk.c:31` (MM, INET; FS has its own `putk`, 4.6) | `m1_i1` count, `m1_p1` buffer | `do_puts` `:1117` | OK, `EFAULT`; on failure the library writes to fd 2 through FS |
| 21 | SYS_FINDPROC `:137` | INET `inet/inet.c:194` | `m3_ca1` name, `m3_i1` flags (ignored) | `do_findproc` `:1138`: search tasks and servers by `p_name` | OK and process number in `m3_i1`; `ESRCH` |
| 22 | SYS_FRESH `:140` (Atari only) | MM `exec.c:433` | `PROC1`, `m1_i2` initialised data in clicks, `m1_p1` new map | `do_fresh` `:533`: `rmshadow()`, `do_newmap()`, zero from the end of the initialised data to the end of the stack | OK; `m1_i1` base, `m1_i2` size of the old memory MM must free |

### 3.2 Address mapping inside SYSTASK

* `umap(rp, seg, vir, bytes)` (`system.c:1243-1302`) returns a physical
  address. For D and S it picks the segment by the last byte. It checks only
  that the start lies below the end of that segment, not that it lies above
  the start (`:1276-1277`). On a 68010 without MMU this is no protection
  issue: a process can touch all memory anyway.
* With shadowing a process's virtual address is its physical address
  (6.1). For a shadowed process `umap()` adds `p_shadow - D.mem_phys`, but
  only for addresses from the data base up (`:1292-1299`, the fix of
  `9e9c3dd`).
* `numap(proc, vir, bytes)` is `umap(.., D, ..)` (`:1309`), `vir_copy()`
  (`:1327`) copies D to D.
* `proc_vir2phys()` (`proc.h:123-127`) is a second mapping used by drivers.
  It applies the shadow offset to every address, text included (8.3).

---

## 4. FS to drivers

### 4.1 Request messages

FS builds every device request in `dev_io()` (`fs/device.c:66-110`),
`gen_opcl()` (`:116-138`), `clone_opcl()` (`:368-419`) or `do_unpause()`
(`fs/pipe.c:299-310`). All use message format m2 (`com.h:152-163`).

| `m_type` (`com.h`) | `DEVICE` `m2_i1` | `PROC_NR` `m2_i2` | `COUNT` `m2_i3` | `POSITION` `m2_l1` | `TTY_FLAGS` `m2_l2` | `ADDRESS` `m2_p1` |
|---|---|---|---|---|---|---|
| `DEV_OPEN` 6 | minor | process | open flags and mode bits (`R_BIT`, `W_BIT`, `O_NOCTTY` ...) | - | - | - |
| `DEV_CLOSE` 7 | minor | process (0 from `dev_close()`, `device.c:59`) | 0 | - | - | - |
| `DEV_READ` 3 | minor | process whose buffer it is | bytes | offset | file flags (`O_NONBLOCK`) | buffer (virtual, in `PROC_NR`) |
| `DEV_WRITE` 4 | minor | as READ | bytes | offset | file flags | buffer |
| `DEV_IOCTL` 5 | minor | caller | request code (`REQUEST`, `TTY_REQUEST`) | 0 | file flags | argument pointer |
| `DEV_GATHER` 9 | minor | `FS_PROC_NR` | number of `iovec_t` entries | offset | 0 | `iovec_t[]` in FS (`fs/cache.c:367-369`) |
| `DEV_SCATTER` 8 | minor | `FS_PROC_NR` | entries | offset | 0 | `iovec_t[]` |
| `CANCEL` 0 | minor (`TTY_LINE`) | process to cancel | `R_BIT` or `W_BIT` (the call it hung in) | - | - | - |

There is no `SELECT`. `HAVE_SCATTERED_IO` is 1 (`const.h:46`); FS uses
GATHER/SCATTER only for block devices with its own buffers (`cache.c:324`,
`read.c:510`). `TTY_SETPGRP` (10) and `TTY_EXIT` (11) are defined
(`com.h:31-32`) but neither sent nor handled. For old sgtty ioctls FS
forwards the whole user message (`device.c:233-251`).

`iovec_t` is two `vir_bytes` (address, size), 8 bytes (`type.h:129-132`).
A driver updates `iov_size` to what was not transferred; FS looks at it
per buffer (`cache.c:371-380`).

### 4.2 Replies, suspension and revival

| Reply `m_type` | `REP_PROC_NR` `m2_i1` | `REP_STATUS` `m2_i2` | When |
|---|---|---|---|
| `TASK_REPLY` 68 | the `PROC_NR` of the request | bytes transferred, `OK`, an error, `SUSPEND`, or for TTY open 1 (became controlling tty) | answer to every request |
| `REVIVE` 67 | process that was suspended | bytes or error | later, by `send()` to FS |

Rules FS relies on:

* `gen_io()` (`device.c:261-321`) waits until a message arrives whose
  `REP_PROC_NR` equals the request's `PROC_NR`. It does not check the
  type: INET answers every request with `REVIVE` (`inetfix/sr.c:512`) and
  that is accepted as the reply. Other messages that arrive meanwhile must
  be `REVIVE`; they revive their process.
* `REP_STATUS == SUSPEND`: FS calls `suspend(task)` (`pipe.c:160-181`),
  stores fd and call in `fp_fd`, the task number as `-task` in `fp_task`
  (a `char`), and does not reply. With `O_NONBLOCK` FS sends `CANCEL`
  instead and turns `EINTR` into `EAGAIN` (`device.c:96-102`).
* A `REVIVE` that arrives in FS's main loop goes to `do_revive()`
  (`misc.c:364-380`): `revive()` (`pipe.c:227-264`) replies the bytes or
  error to the suspended process, FS sends nothing to the driver.
* On a signal MM sends `UNPAUSE`; FS sends `CANCEL` to the driver and
  replies `EINTR` to the process (`pipe.c:299-314`). The driver must
  answer the `CANCEL` (TTY: `tty.c:627-657`, INET: `inetfix/sr.c:183-189`).
  If the driver's completion crosses the `CANCEL`, FS drops the
  completion (`device.c:285-286`).
* An open must not suspend: `dev_open()` panics on `SUSPEND`
  (`device.c:48`).
* A driver must answer every request exactly once. A request without an
  answer blocks FS, and with it every process (fixed in INET, see the
  header of `inetfix/sr.c`; still true for FLOPPY, 8.1).

### 4.3 The dmap table on this port

`src/fs/table.c:125-151` (the `MACHINE == ATARI` variant), `max_major` 8.
Device nodes are from `src/rosco/rootfs/proto` and `sdproto`.

| Major | Nodes (minor) | Open/close | I/O | Task | Driver |
|---|---|---|---|---|---|
| 0 | - | `no_dev` | - | 0 | `ENODEV` |
| 1 | `/dev/ram` (b 0), `mem` (1), `kmem` (2), `null` (3), `zero` (4) | `gen_opcl` | `gen_io` | MEM -4 | `kernel/memory.c` on `driver.c` |
| 2 | none | `gen_opcl` | `gen_io` | FLOPPY -5 | fake task, never replies (8.1) |
| 3 | `/dev/c0d0` (b 0), `c0d0p0..p3` (1-4), `c0d1` (5) | `gen_opcl` | `gen_io` | CTRLR(0) -8 | `kernel/roscdisk.c` on `driver.c`; minor 5 is the RAM root image (`ROSCO_RD_MINOR`, `rosco.h:18`) |
| 4 | `/dev/console` (0), `log` (15), `tty00` (16), `ttyp0..3` (128-131), `ptyp0..3` (192-195) | `tty_opcl` | `gen_io` | TTY -9 | `kernel/tty.c` (minors `tty.c:72-76`), `roscons.c`, `roscrs232.c`, `pty.c` |
| 5 | `/dev/tty` (0) | `ctty_opcl` | `ctty_io` | TTY | the process's controlling tty |
| 6 | none | `no_dev` (`ENABLE_PRINTER` 0) | - | 0 | - |
| 7 | `/dev/ip` (0, `sdproto:308`), `/usr/ftp/dev/tcp` (2, `sdproto:366`); INET makes its other nodes itself (`inet/inet_config.c:59-75`) | `no_dev` until `FSSIGNON`, then `clone_opcl` | `gen_io` | `ANY` until sign-on, then INET's slot | INET, minors `if2minor(ifno, dev)` (`inet/inet_config.h:56-63`) |

### 4.4 Drivers on the generic driver code

`driver_task()` (`kernel/driver.c:64-123`) serves MEM and the disk:

* It accepts requests only from FS or from tasks (`m_source < 0`); others
  are printed and dropped without a reply (`:90-93`).
* `DEV_READ`/`DEV_WRITE` check the buffer with `numap()` and become a
  one-entry GATHER/SCATTER (`do_rdwt` `:156-185`); the reply is the byte
  count. GATHER/SCATTER from FS copy the vector in and back (`:191-235`).
* `HARD_INT` is ignored; other types give `EINVAL`. `CANCEL` is not
  handled (block drivers never suspend).
* The reply is always `TASK_REPLY` with `REP_PROC_NR` = request
  `PROC_NR` (`:117-121`).

MEM (`memory.c`): `m_transfer()` uses `proc_vir2phys()` (`:94`). Ioctls:
`MIOCRAMSIZE` (only `PROC_NR == FS`, size in blocks in `POSITION`,
`:225-246`), `MIOCSPSINFO` (MM or FS register their table address,
`:248-260`), `MIOCGPSINFO` (copies `struct psinfo` to the caller,
`:262-268`), else `do_diocntl` (partition get/set).

Disk (`roscdisk.c`): minors 0-4 the SD card through the firmware's polled
SPI routines, minor 5 the RAM image. SD transfers must be whole 512-byte
sectors (`:195`). Positions past the end give a short transfer (EOF), not
an error (`:183-184`). The first open initialises the card and reads the
MBR; the last close forgets it (`:104-136`).

### 4.5 TTY and PTY

`tty_task()` (`tty.c:144-245`):

* Handles `DEV_READ`, `DEV_WRITE`, `DEV_IOCTL`, `DEV_OPEN`, `DEV_CLOSE`,
  `CANCEL`; anything else gets `EINVAL`, an unknown minor `ENXIO`. PTY
  master minors (192+) go to `do_pty()` (`pty.c:76`) except ioctls.
* One reader and one writer per line: a second read or write while one is
  suspended gets `EIO` (`tty.c:261`, `:334`), it is not queued.
* A read or write that cannot finish replies `SUSPEND`, or `EAGAIN` /
  the partial count with `O_NONBLOCK`, and later `REVIVE`
  (`tty.c:309-317`, `:357-365`).
* `DEV_OPEN` returns 1 if the line became the caller's controlling tty (no
  `O_NOCTTY`, not the log device); FS's `tty_opcl()` then sets `fp_tty`
  (`device.c:170-173`). `tty_pgrp` is set to the opener's process number,
  and signals from the keyboard go to it through `cause_sig()`
  (`tty.c:1276-1287`).
* Ioctl parameter sizes come from the code (`tty.c:392-445`); `TCSETSW`,
  `TCSETSF`, `TCDRAIN` may suspend until output has drained.
* `tty_reply()` panics if its `send()` fails (`tty.c:1255-1270`).

### 4.6 FS talking to tasks directly

* At start-up FS sends `DEV_IOCTL` `MIOCSPSINFO` (with `PROC_NR` FS and
  `fproc` as address) and `MIOCRAMSIZE` straight to MEM
  (`fs/main.c:210-215`, `331-337`).
* FS prints through its own `putk()` (`fs/putk.c`), a `DEV_WRITE` to major
  4 minor 0 (the console), not through `SYS_PUTS`.
* FS asks CLOCK for the time (`GET_TIME`, `fs/utility.c:37`) and sets it
  (`SET_TIME`, `fs/time.c:83`).

---

## 5. MM and FS, the kernel to the servers, clock and TTY

### 5.1 MM to FS: tell_fs()

`tell_fs(what, p1, p2, p3)` (`mm/utility.c:93-114`) puts `p1..p3` in
`m1_i1..m1_i3` and does `_taskcall(FS, what)`. The result is ignored.

| what | `m1_i1` | `m1_i2` | `m1_i3` | MM call sites | FS handler | FS effect |
|---|---|---|---|---|---|---|
| CHDIR 12 | slot | FALSE (unused) | 0 | `exec.c:101`, `signal.c:510` (core dump) | `do_chdir` `stadir.c:45-62` | MM's own fproc slot takes the user's root and working directory, real ids = the user's effective ids, umask |
| EXEC 59 | slot | | | `exec.c:186` | `do_exec` `misc.c:240` | close the `FD_CLOEXEC` descriptors |
| EXIT 1 | slot | | | `forkexit.c:172`; `misc.c:79` (MMSIGNON) | `do_exit` `misc.c:270` | cancel a hanging call, close all fds, drop directories, revoke the tty of a session leader, `fp_pid = PID_FREE` |
| FORK 2 | parent | child | child pid | `forkexit.c:116` | `do_fork` `misc.c:198` | copy fproc, count up the filps, not a session leader, `dup_inode` of the directories |
| SETUID 23 | slot | real uid | effective uid | `getset.c:48`, `exec.c:162`; `utility.c:41,46` (MM's own slot: root while opening an executable, then back) | `do_set` `misc.c:338` | |
| SETGID 46 | slot | real gid | effective gid | `getset.c:57`, `exec.c:166` | `do_set` | |
| SETSID 62 | slot | | | `getset.c:64` | `do_setsid` `device.c:198` | session leader without a tty (no return value, 8.5) |
| SYNC 36 | | | | `utility.c:85` (MM panic) | `do_sync` `misc.c:134` | |
| UNPAUSE 65 | slot | 0 | | `signal.c:636` | `do_unpause` `pipe.c:270` | if the process hangs in FS: `CANCEL` to the driver or release the pipe/lock wait, reply `EINTR` to the process |
| REBOOT 76 | | | | `misc.c:46` | `do_reboot` `misc.c:161` | exit all processes, unmount everything |

MM also uses FS like a program, on behalf of the user whose identity it
borrowed with CHDIR: `access`, `open`, `fstat`, `read`, `lseek`, `close`
for exec (`utility.c:21-56`, `exec.c:200-311`, `exec.c:662-719`), `open`,
`write` for core dumps (`signal.c:643-698`), and `open`/`ioctl`/`close` of
`/dev/ram` for `MIOCSPSINFO` (`mm/main.c:180-183`).

`rw_seg()` (`exec.c:587-629`) reads a segment straight into the user
process: it passes `fd | seg << 5 | proc << 7` as fd and the user's
virtual address as buffer. FS honours the encoding only for MM and only
when bits above the low byte are set (`fs/read.c:54-62`). This works for
process numbers >= 2 (for slots 0 and 1 the encoded value has no high
bits).

Start-up handshake: FS sends MM the RAM disk size in clicks in `m1_i1`
(`fs/main.c:339-342`); MM waits for it with `receive(FS)`, reads its
memory with `SYS_MEM`, prints the sizes and sends the message back
(`mm/main.c:161-177`).

### 5.2 FS to MM

Apart from the start-up handshake FS never sends to MM.

### 5.3 Kernel to tasks: HARD_INT

`interrupt(task)` (`proc.c:52-124`) gives a task waiting in `receive(ANY
or HARDWARE)` a message with `m_source = HARDWARE`, `m_type = HARD_INT`
and nothing else. If the task is busy the interrupt is remembered as one
flag (`p_int_blocked`) and delivered at its next receive (`proc.c:285-290`).
Interrupts during kernel work are held and replayed (`unhold()`).

### 5.4 Signals from the kernel: cause_sig, KSIG, SYS_ENDSIG

Sources of kernel signals, all through `cause_sig(proc, sig)`
(`system.c:1162-1196`):

| Source | Signals | Where |
|---|---|---|
| CPU exceptions of a user process | `SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGABRT`, `SIGTRAP`, `SIGEMT`, `SIGFPE` | `roscmain.c:239-272` (`vecsig[]`) |
| stack check at restart | `SIGSTKFLT` (16) | `checksp()` `roscmain.c:335-363` |
| TTY | `SIGINT`, `SIGQUIT`, `SIGKILL`, `SIGHUP` to `tty_pgrp` | `tty.c:1276-1287` |
| CLOCK alarm | `SIGALRM` | `clock.c:362-371` |
| FS | `SIGPIPE` via `SYS_KILL` | `fs/pipe.c:119` |
| traced exec | `SIGTRAP` | `system.c:361` |

`cause_sig()` sets the bit in `p_pending`, counts `p_pendcount`, makes the
process unrunnable (`SIG_PENDING`) and calls `inform()` if MM is waiting
in `receive(ANY)`. `inform()` (`system.c:1203-1231`) sends MM, from
`HARDWARE`:

| Field | Value |
|---|---|
| `m_type` | `KSIG` 64 |
| `SIG_PROC` `m2_i1` | slot of the process |
| `SIG_MAP` `m2_l1` | its pending set (`sigset_t`, 4 bytes) |

MM's `do_ksig()` (`signal.c:247-313`) accepts it only from `HARDWARE`,
signals each bit with `check_sig()` (`SIGINT`/`SIGQUIT` to the process
group, `SIGKILL` to all), and calls `SYS_ENDSIG` once per signal. The
process runs again when the count reaches 0 (`system.c:747-765`).
`SIGSTKFLT` is special: `inform()` already counts it as done
(`system.c:1225-1232`), MM grows the stack (`stack_fault()`,
`break.c:175-197`) and does not send `SYS_ENDSIG` for it
(`signal.c:286-290`). Contract: one `SYS_ENDSIG` per signal bit, except
`SIGSTKFLT`.

Catching a signal: MM computes the new stack pointer (`sigcontext` 78 +
`sigframe` 22 bytes below sp, `signal.c:453-468`), checks it with
`adjust()`, and sends `SYS_SENDSIG` with a `struct sigmsg` (18 bytes:
`sm_signo` int @0, `sm_mask` long @2, `sm_sighandler` @6, `sm_sigreturn`
@10, `sm_stkptr` @14; `type.h:145-155`). Old-style handlers (SIGNAL) use
`SYS_OLDSIG`. Then MM unpauses the process (MM call or `tell_fs(UNPAUSE)`).

### 5.5 The CLOCK task

Request format m6 (`clock.c:1-45`, dispatch `:117-136`). The reply is the
request message with `m_type = OK`.

| `m_type` | Sent by | Request | Reply |
|---|---|---|---|
| `SET_ALARM` 1 | MM `signal.c:369-374` | `CLOCK_PROC_NR` `m6_i1`, `DELTA_TICKS` `m6_l1` (0 cancels) | `SECONDS_LEFT` `m6_l1` of the old alarm; at expiry `cause_sig(proc, SIGALRM)` |
| `GET_TIME` 3 | FS `utility.c:37` | - | `NEW_TIME` `m6_l1`, seconds since 1970 |
| `SET_TIME` 4 | FS `time.c:83` | `NEW_TIME` | OK |
| `GET_UPTIME` 5 | INET `inet/clock.c:41` | - | `NEW_TIME` = ticks since boot |
| `SET_SYNC_AL` 6 | INET `inet/clock.c:155-160` | `CLOCK_PROC_NR`, `DELTA_TICKS` | `SECONDS_LEFT`; at expiry SYN_AL sends `CLOCK_INT` (= `HARD_INT` 2, `com.h:110`) to the process |
| `HARD_INT` | `HARDWARE` | - | no reply |
| other | | | panic (`clock.c:131`) |

Both alarm kinds use the one timer `tmr_alarm[proc_nr]` (`clock.c:79`,
`:341`); a process cannot have both. `CLOCK_PROC_NR` is not range-checked.
`SYS_XIT` cancels the alarm (`system.c:415`). `HZ` is 60 (`const.h:17`).

### 5.6 INET signing on

`inet/inet.c:150-200`:

1. `svrctl(MMSIGNON)` to MM (`mm/misc.c:69-101`): MM tells FS `EXIT` for
   INET's slot (all its files are closed), releases a waiting parent,
   gives INET's children to INIT, and sets pid, process group and parent
   to 0.
2. `svrctl(SYSSIGNON)` to MM, which passes it to SYSTASK as `SYS_SYSCTL`
   (`mm/misc.c:64-66`): INET becomes `PPRI_SERVER` and learns its process
   number.
3. `svrctl(FSSIGNON)` with `{ dev = /dev/ip's device, style =
   STYLE_CLONE }` to FS (`fs/misc.c:388-415`): root only; the major must
   still be `ANY`; FS sets `clone_opcl`, `gen_io` and INET's slot, and
   `fp_pid = PID_SERVER`.
4. `SYS_FINDPROC "SYN_AL"` to find the synchronous alarm task.

After this INET receives FS's device messages (4.1), answers with
`REVIVE` (4.2), copies user data with `SYS_VCOPY`, and keeps time with
`GET_UPTIME`/`SET_SYNC_AL`. The `struct fssignon` (4 bytes) and `struct
systaskinfo` are in `include/sys/svrctl.h`.

---

## 6. Shared data that is an interface without being a message

Each item says what is shared, who depends on it, and why a reviewer of
one component must know it.

### 6.1 Addresses: virtual equals physical

MM gives every segment `mem_vir == mem_phys` (`exec.c:401-403`); the
kernel does the same for MM, FS and INIT (`roscmain.c:152-154`). A pointer
in a process is a physical address. Consequences:

* Programs are relocated at exec (6.6) and cannot move afterwards; fork
  copies the parent away instead of the child (shadowing).
* Any process can read or write any memory: there is no protection to
  review, only correctness.
* `umap()` accepts addresses below a process's data (3.2). Address
  validation in drivers (`numap()`) only bounds the top.
* Clicks are 16 bits (`phys_clicks`, `vir_clicks`, `type.h:14-16`): at 256
  bytes a click, physical addresses up to 16MB can be named. Messages carry
  clicks in signed `int` fields (`SYS_MEM`, `SYS_XIT`, `SYS_FRESH`); values
  above 32767 clicks (8MB) round-trip only by bit pattern.

Why it matters: MM's segment arithmetic, FS's buffer addresses and every
driver copy rest on it.

### 6.2 Shadowing

`kernel/stshadow.c` (`mkshadow`, `rmshadow`, `unshadow`), called from
`SYS_FORK`, `SYS_XIT`, `SYS_FRESH` and the clock (`clock.c:182`). A
shadowed process has `p_shadow != 0`: its data and stack are at
`p_shadow`, while its map still names the place where the running image
lives. The clock swaps images (`unshadow`) so that the process to run is
the real one. Text is never shadowed.

Code that must apply the shadow offset when it touches another process's
memory:

| Code | Handles text correctly? |
|---|---|
| `umap()` / `numap()` / `vir_copy()` (`system.c:1243-1340`) | yes, since `9e9c3dd` |
| `cp_mess()` (`proc.c:580-613`) | not needed: messages must be in data or stack (`proc.c:185-195`) |
| `proc_vir2phys()` (`proc.h:123-127`), used by `roscrs232.c:224` (console and tty00 output), `pty.c:224,296,350`, `tty.c:727,1186`, `memory.c:94`, `roscdisk.c:174` | no: the offset is added to text addresses too (8.3) |
| `ps` (`src/rosco/cmds/fixes/sttools/ps.c.patch`, `ps_shoff`) | reads a shadowed process's arguments from its own copy |
| MM `relocate()` (`exec.c:662-719`) | writes the new image directly; runs after `SYS_FRESH` removed the shadow |

`p_physio` (`proc.h:31`) blocks flipping while a driver does slow I/O
into a process; no rosco driver sets it (only `stscsi.c`).

Why it matters: MM's fork, exec and exit paths and every driver that copies
to or from a user buffer after the caller was suspended.

### 6.3 Process tables read by ps through /dev/kmem and /dev/mem

`ps` (`src/sttools/ps.c` with the rosco patch) gets `struct psinfo`
(`type.h:160-163`: `nr_tasks`, `nr_procs`, addresses of `proc`, `mproc`,
`fproc`) with `MIOCGPSINFO`, then reads the three tables. It is compiled
against `kernel/proc.h`, `mm/mproc.h`, `fs/fproc.h` and the same
`config.h`, so the struct layouts are part of the interface:

| Table | Size per entry | Entries | Notable offsets |
|---|---|---|---|
| `struct proc` (`proc.h:11-70`) | 184 | `NR_TASKS + NR_PROCS` = 41 | `p_reg` 0 (72 bytes, d0..a6, sp, pc, psw), `p_trap` 76, `p_shadow` 78, `p_nr` 90, `p_flags` 98, `p_map` 100, `p_pid` 118, `p_priority` 120, `p_name` 168 |
| `struct mproc` (`mproc.h:8-47`) | 286 | 32 | `mp_seg` 0, `mp_pid` 20, `mp_parent` 26, `mp_sigact` 62, `mp_flags` 250, `mp_procargs` 252, `mp_reply` 260 |
| `struct fproc` (`fproc.h:7-27`) | 118 | 32 | `fp_filp` 10, `fp_tty` 96, `fp_fd` 98, `fp_suspended` 108, `fp_task` 110, `fp_pid` 112 |

`roscompx.S` hard-codes `struct proc` offsets; `roscmain.c:44-50`
asserts them at compile time (`fp` 56, `sp` 60, `pc` 64, `psw` 68,
`p_trap` 76). MM's core dump writes the `struct proc` entry (read word by
word with `T_GETUSER`) after the memory map (`signal.c:674-690`), so the
core file format depends on it too. `NR_PROCS` must be the same in kernel,
MM, FS and ps; all take it from `src/rosco/include/minix/config.h`.

Why it matters: any change to these structs or to `NR_PROCS` needs ps,
`roscompx.S` and the core format checked; MM and FS must register their
tables (`MIOCSPSINFO`) for ps to work.

### 6.4 Headers shared between components

| Item | Header | Shared by |
|---|---|---|
| `message` | `include/minix/type.h` | everyone (1.4) |
| `struct mem_map` (6 bytes: vir, phys, len clicks) | `type.h:123-127` | MM, kernel (`SYS_NEWMAP`, `SYS_GETMAP`, `SYS_FRESH`), ps, core files |
| `struct sigmsg` | `type.h:145-155` | MM, kernel |
| `struct sigcontext` (78), `struct sigframe` (22), `struct sigregs` (72) | `include/sys/sigcontext.h` | kernel, C library (`_sigreturn.c`, `setjmp`) |
| `iovec_t`, `cpvec_t` | `type.h:129-138` | FS, drivers, INET, kernel |
| `struct stat` (30 bytes) | `include/sys/stat.h` | FS, programs |
| ioctl codes, `struct termios`, `struct winsize` | `include/sys/ioc_*.h`, `termios.h` | programs, FS (sgtty check), TTY, INET |
| svrctl codes and structs | `include/sys/svrctl.h` | programs, MM, FS, kernel, INET |
| task numbers, field names | `include/minix/com.h` | kernel, MM, FS, INET |
| process numbers, `NR_TASKS`, clicks | `include/minix/const.h` | everyone |
| `NR_PROCS` and features | `src/rosco/include/minix/config.h` (found before `include/` by every rosco makefile) | everyone |
| errno values | `include/errno.h` | everyone; sign depends on `_SYSTEM` |

`include/minix/fslib.h` only declares file-system helper functions
(`conv2`, `conv4`, ...); it defines no messages.

### 6.5 The kernel environment

`roscmain.c:501-511` sets `rootdev`, `ramimagedev`, `c0=ACSI`, `ramsize`,
`processor`, `NR_BUFS`, `DMA_SECTORS`, `ENABLE_SYMLINKS`, `ENABLE_SWAP`,
`NR_CONS`, `NR_PTYS` (decimal strings). FS reads `rootdev`, `ramimagedev`
and `ramsize` with `SYSGETENV` (`fs/main.c:250-263`, `288-290`); the kernel
uses `c0` in `mapdrivers()`. Why it matters: the root device choice
(RAM image `c0d1`, else the first Minix SD partition) is made here, not
in FS.

### 6.6 The executable format

Made by `src/rosco/tools/mkminix.py` from an ELF linked with
`src/rosco/tools/prog.ld`; read by MM (`exec.c:200-311`, `662-719`) and by
`mkimage.py` for the boot image.

* 32-byte header (`include/a.out.h`, big-endian layout): `a_cpu` 0x0B,
  `a_flags` 0x10 (`A_EXEC`, not `A_SEP`), magic 03 01, `a_hdrlen` 0x20,
  then `a_text`, `a_data`, `a_bss`, `a_entry` (0), `a_total` (text + data
  + bss + stack), `a_syms` (0) as longs (`mkminix.py:179`).
* Text (with rodata, `prog.ld`), data, then GEMDOS relocations: a long
  offset of the first long to relocate (0: none), then bytes (2..254 even:
  distance to the next long; 1: add 254; 0: end). MM adds the text base to
  each long (`exec.c:691-718`).
* MM treats the file as common I&D: whole text clicks become the text
  segment, the partial last text click is counted as data
  (`exec.c:282-297`). Text is not shared between processes (that needs
  `SEPARATE`, `exec.c:648`).
* MM accepts `a_cpu` `A_M68K` and also `A_I8086` (`exec.c:263`), flags
  only within `A_NSYM|A_EXEC|A_SEP`.
* Stack size is `a_total` minus text, data and bss; `chmem` and
  `mkminix.py -S/-m` set it (every command at least 8kw, `CLAUDE.md`).

Why it matters: MM's exec is the only loader; it trusts the header sizes
and the relocation offsets (8.4).

### 6.7 The exec frame and the C start-up

Built by `_execve()` (`src/rosco/lib/_execve.c:45-117`), copied by MM
(`exec.c:89-95`), relocated by `patch_ptr()` (`exec.c:450-477`), read by
`crtso.S`:

```
offset 0   long argc            (long because MM and crtso expect it)
       4   argv[0..argc-1]      pointers, relative to the frame start
           NULL
           envp[...]            relative pointers
           NULL
           strings, padded to a multiple of 4
```

MM places the frame at the top of the new stack and adds that address to
every non-NULL pointer until it has seen two NULLs. `#!` scripts are
handled by MM inserting the interpreter and its argument
(`exec.c:483-581`). `crtso.S` takes `argc == 0` to mean a server started by
the kernel (MM, FS, INIT, whose stack holds one 0 long, `roscmain.c:150`)
and then calls `main()` without arguments. `mp_procargs` keeps the frame
address for ps.

### 6.8 The signal frame on the user stack

Written by the kernel (`do_sendsig`, `system.c:562-618`), consumed by the
C library:

```
sp ->  sigframe (22 bytes)
         sf_retadr   (4)  address of __sigreturn (from SIGACTION m1_p3)
         sf_signo    (2)  int
         sf_code     (2)  0
         sf_scp      (4)  -> sigcontext
         sf_scpcopy  (4)  -> sigcontext (SIGFRAME_COMPAT_16 1)
         sf_fp       (2)  int: low 16 bits of a6
         sf_retadr2  (4)  interrupted pc
       sigcontext (78 bytes)
         sc_flags (2), sc_mask (4), sc_regs (72: d0-d7, a0-a7, pc, psw, pad)
```

The handler returns to `__sigreturn` (`sigret.S`), which jumps to
`_sigreturn()`; that finds `sf_scp` where a C function expects its first
argument, because `sf_signo` and `sf_code` together take the place of the
return address. This works only because both are 2-byte ints. Old-style
handlers get the 8-byte frame of `build_sig()` instead.

### 6.9 The boot image

Built by `src/rosco/tools/mkimage.py`; read by the kernel's `main()`
(`roscmain.c:80-180`):

* The kernel (`kernel.bin`, linked at 0, vectors first) padded to
  `sizes[0] + sizes[1]` clicks. `sizes[]` (u16, clicks) sits behind the
  magic `rosSIZES` (`roscompx.S:612-621`).
* MM, FS, INIT follow, each relocated to its load address and padded to a
  click; `sizes[2..7]` get `(0, clicks)` pairs: everything is data
  (`mkimage.py:88-105`). The kernel derives the maps from `sizes[2t+2]`,
  `sizes[2t+3]` for t = 0..2 (`roscmain.c:136-142`).
* MM's and FS's data must start with `0xDADA` (`crtso.S` `.data.crt0`);
  mkimage writes INIT's origin and size behind FS's magic
  (`mkimage.py:114-115`). No code in `src/fs` reads it (8.14).
* The loader puts the root file system image after the boot image; the
  kernel moves it to the top of RAM (`place_image()`, `roscmain.c:186-223`)
  and serves it as `c0d1`. The firmware RAM 0x400-0x2000 is kept
  (`ROSCO_KEEP_FW`, `config.h:138`) for the SD routines.

Why it matters: MM's start-up (`sys_getmap` of tasks, MM, FS, INIT,
`mm/main.c:137-153`) and the memory left for user processes depend on
these sizes.

### 6.10 Limits that are spread over several components

| Limit | Value | Depends on it |
|---|---|---|
| `NR_PROCS` | 32 | kernel tables, MM, FS, clock `tmr_alarm[]`, ps, MM's `rw_seg` fd encoding (process number << 7 in a 16-bit int) |
| `fp_task` | `char` | FS stores `-task` (tasks -9..-1, `XPIPE` -10 .. `XPOPEN` -12, INET's slot) |
| message ints | 16 bits | counts, pids (`pid_t` 2 bytes, MM wraps at 30000, `forkexit.c:101`), uids |
| clicks | 16 bits | 16MB address space; 8MB in signed message fields |
| `ARG_MAX` | 4096 | library, MM buffer `aligned_buf` (`exec.c:62`), ps |
| `CPVEC_NR` | 16 | INET, `SYS_VCOPY` |
| `M3_STRING` | 14 | library `_loadname`, FS `fetch_name`, `SYS_FINDPROC` names |

---

## 7. Review checklists

Short pointers; the sections above are the reference.

**FS**
* Every handler validates its fields before use: fds (`get_filp`), lengths
  (`fetch_name`), counts (16-bit, negative), user pointers (copy errors).
* Exactly one reply per request: handlers return `SUSPEND` only after
  `suspend()` (or for REVIVE/EXIT-from-MM); `revive()`, `release()`,
  `do_unpause()` reply once and clear the suspended state. Check every path
  through `read_write()`, `pipe_check()`, `lock_op()`, `common_open()`.
* Messages from tasks (negative `who`) only reach `do_revive`; any other
  type from a task still gets a reply sent to the task (`fs/main.c:94`).
* Reply fields: set every field the call defines; remember `m1` is shared
  and not cleared.
* MM-only calls check `who == MM_PROC_NR`; the `rw_seg` fd encoding is
  only honoured for MM.
* Device layer: `gen_io()` matching by `REP_PROC_NR`, `ELOCKED` loop,
  `CANCEL` crossing a completion, `O_NONBLOCK`; dmap entries that point to
  dead or fake tasks (8.1, 8.2).
* The uncommitted `new_node(follow)` change: every caller passes the right
  flag.

**MM**
* Fork: slot and memory checks before any side effect; `SYS_FORK` with the
  child base; `tell_fs(FORK)`; reply to both.
* Exec: everything that can fail must fail before `new_mem()` (the point
  of no return); after it, errors are ignored today (8.4). Frame size,
  relocation bounds, `SYS_FRESH` base/size freed exactly once.
* Exit and wait: `SYS_XIT` returns the memory to free (shadow or real);
  zombie handling; children to INIT; SIGHUP to the session.
* Signals: `sig_proc` stack computation matches the kernel's frame (100
  bytes); one `SYS_ENDSIG` per KSIG bit except `SIGSTKFLT`; `unpause()`
  goes to FS only when the process is not hanging in MM.
* `setreply()` only for user slots; `who < 0` handling (8.6).
* brk/adjust: `sys_newmap` after every change of the map.

**Kernel (SYSTASK, proc.c)**
* SYSTASK functions trust the process numbers they are given (most do not
  call `isokprocn`); review the callers instead.
* Every user-memory access goes through `umap()`; `proc_vir2phys()` users
  (8.3).
* `cause_sig()`/`inform()`/`SYS_ENDSIG` counting.

**Drivers (TTY, MEM, disk, INET)**
* Exactly one `TASK_REPLY` per request, `REP_PROC_NR` = request
  `PROC_NR`; `REVIVE` later for `SUSPEND`; `CANCEL` always answered.
* User buffers checked with `numap()` at request time and mapped again at
  copy time (the process may have been flipped meanwhile).
* Minor and request range checks; ioctl sizes from the code.

**C library**
* Field placement matches `param.h` (2.2, 2.3); 16-bit truncation of
  counts and ids; `_execve` frame format; `__sigreturn` frame assumptions.

---

## 8. Observations found while writing this (not fixed)

From reading only, unless said otherwise. None of these was tested.

1. **FLOPPY never replies.** dmap major 2 is enabled with
   `gen_opcl`/`gen_io` and task FLOPPY (`fs/table.c:130`), but
   `floppy_task` is `fake_task()`, which receives and prints and never
   sends (`roscmain.c:610-619`, `636-639`). Opening any device node with
   major 2 (only root can make one) would leave FS waiting forever in
   `gen_io()`, and with it the whole system. Making the entry
   `DT(0, ...)` or answering in `fake_task` would avoid it.
2. **INET's dmap entry outlives INET.** `FSSIGNON` writes `dmap[7]`
   (`fs/misc.c:403-414`); nothing resets it when INET exits. Afterwards a
   request to major 7 makes `sendrec()` fail with `E_BAD_DEST` and FS
   panics (`device.c:304`); if the slot was reused, FS would sendrec a user
   process. A restarted INET gets `EBUSY` from `FSSIGNON`.
3. **`proc_vir2phys()` has the bug that `umap()` had.** It adds the shadow
   offset to every address (`proc.h:123-127`), text included. It is used
   where a driver copies at a later time than the request:
   console/tty00 output (`roscrs232.c:224`), pty (`pty.c:224`, `350`),
   `memory.c:94`, `roscdisk.c:174`. A `write()` of a string constant in
   text by a process that becomes shadowed while its output waits (a shell
   parent after fork) would send wrong bytes. Read buffers are always in
   data or stack, so reads are not affected.
4. **MM exec ignores errors after the point of no return.**
   `rw_seg()` failures and `relocate()` errors are dropped
   (`exec.c:146-152`); the process starts with a partial image.
   `relocate()` does not check that relocation offsets stay inside the new
   image (`exec.c:696-716`): a corrupt executable can patch any longword in
   RAM. `if (n < sizeof(long))` (`exec.c:694`) compares unsigned (16-bit
   `sizeof`, `rosco_sizeof.h`), so `read()` returning -1 is not caught
   there.
5. **FS `do_setsid()` has no return statement** (`fs/device.c:198-212`).
   FS replies to MM with whatever is in `d0`; `tell_fs()` ignores it, so
   there is no visible effect.
6. **Messages from tasks.** MM's `setreply(who, result)` with a negative
   `who` would write outside `mproc[]` (`mm/main.c:52`, `98`). Only KSIG
   from `HARDWARE` reaches MM today and it returns `SUSPEND`. FS computes
   `fp = &fproc[who]` for tasks too (`fs/main.c:83-85`) and reads
   `fp_effuid` from outside the table; harmless read.
7. **MM `sig_proc()` panic message** passes its arguments in the wrong
   order for the format (`signal.c:420-421`).
8. **MM `do_trace()` returns `-errno`** when `sys_trace()` fails
   (`trace.c:70-71`), but `sys_trace()` returns the error through
   `_taskcall()` and does not set `errno`.
9. **Two kernel changes lack the `rosco:` marker** that `CLAUDE.md`
   requires: `umap()` (`system.c:1292-1299`) and `do_sigreturn()`
   (`system.c:658-666`).
10. **`config.h` comments.** `NR_PROCS 32` says "rosco: 16 was too few",
    but `include/minix/config.h` already has 32. The header says the
    settings are sized for 1MB RAM; the target has 5MB.
11. **Stale comment table in `system.c:36-99`**: SYS_EXEC's stack pointer
    is listed under `PID` and SYS_TIMES's buffer under `PID`; the code uses
    `m1_p1` and returns the times in m4 fields.
12. **`fetch_name()` does not add a NUL** (`fs/utility.c:72-80`): it copies
    `len` bytes as given. Whether the path walk stays within `len` was not
    checked.
13. **TTY allows one reader and one writer per line**; a second one gets
    `EIO` instead of waiting (`tty.c:261`, `334`). Original Minix
    behaviour, but a surprise for programs that share the console.
14. **INIT's origin in FS's 0xDADA block is written but not read**
    (`mkimage.py:114-115`; no reader in `src/fs`).
15. **`SYS_COPY` with `ABS` is not supported under shadowing**
    (`system.c:811-813`): `proc_addr(ABS)` would index outside `proc[]`.
    Only the `SHADOWING == 0` code in MM uses `ABS`, so it is a restriction,
    not a live bug.
16. **`sigframe.sf_fp` is a 2-byte int** (`include/sys/sigcontext.h`,
    non-`__MLONG__` branch) and receives the 32-bit `a6`
    (`system.c:603`). Registers are restored from the sigcontext, so only
    frame-pointer walks through a handler frame see a truncated value.
17. **`mm_exit()` takes the session check from the caller**
    (`forkexit.c:166`: `mp->mp_procgrp`, not the exiting process's). For
    kernel signals `do_ksig()` sets `mp->mp_procgrp` first
    (`signal.c:268-269`); for `kill()` by another process the SIGHUP to the
    session may be missed. Unchanged original code; worth a test.

---

## 9. How the tables were made

* The call table (section 2) and the SYSTASK table (3.1) started from a
  skeleton printed by a throwaway script that joins `callnr.h` with the two
  `call_vec[]` tables and `com.h` with the `sys_task()` switch, plus a
  second script that lists, per `do_*` handler, the `param.h` names it uses
  and the error codes it returns. The scripts misread the `#if` branches of
  `table.c` (SIGNAL, RDLNK, LSTAT, SYMLINK); those rows were corrected by
  hand. Everything else (fields, replies, callers, notes) was filled in and
  checked by hand against the source.
* The message offsets (1.4), struct sizes and offsets (6.3, 6.4, 6.8) and
  type sizes (1.6) were obtained by compiling small `offsetof()`/`sizeof()`
  probes with `m68k-elf-gcc -mcpu=68010 -mshort` and the project's defines
  through `src/rosco/xrun` (`-S`, values read from the assembler output).
  No emulator was run.
* The scripts and probes are not in the repository.
