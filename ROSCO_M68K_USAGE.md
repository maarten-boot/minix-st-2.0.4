# Using Minix 2.0.4 on the rosco_m68k

A short guide to the SD card image (`src/rosco/rosco_sd.bin`): logging in,
accounts and passwords, networking over SLIP, and the network services.
How the port was built is in `ROSCO_M68K_PORT_SPEC.md` and the
`ROSCO_M68K_LOG_*.md` files.

## Logging in

* The console is DUART channel A, at 38400 baud by default (8N1, no flow
  control). tty00 (channel B) has a login too (for accounts with a
  password), unless it is used for SLIP.
* At boot `/etc/rc` asks for the date and time (`[[MMDDYY]hhmm[ss]]`, GMT).
  RETURN or 30 seconds without an answer keeps the clock, which starts from
  the last saved time (`/etc/stamp`).
* Accounts: `root` and `ast` (an ordinary user). Both have **no password**
  after installation, which is easy for a first look at Minix; without
  one they can only log in on the console. The system account `bin` is
  locked (`*`).
* The shell is ash (`/bin/sh`), with line editing (arrow keys, history).
  The old Minix shell is `/usr/bin/osh`.
* `man <command>` shows the manual page; `apropos <word>` searches them.

## Passwords: what needs one

Minix only lets an account without a password log in on the console
(login's "secure" terminals are the console's minor devices 0-7). tty00
and everything that comes in over the network need a password, or a trust
file. Set a password with
`passwd` (as root: `passwd ast`, `passwd root`).

| What | Without a password | With a password |
|---|---|---|
| Login on the console | works | asks for it |
| Login on tty00 | refused | asks for it |
| `su` (ast to root) | works while root has none | asks for root's |
| telnet to the board | `Password:` prompt, then refused | works |
| ftp to the board as that user | refused (530) | works |
| anonymous ftp (`ftp` or `anonymous`) | works, read only, `/usr/ftp/pub` | |
| rlogin to the board | only if trusted (see below), else refused | works when asked |
| rsh, rcp to the board | only if trusted (see below) | (passwords are not used) |
| From the board to other hosts (telnet, ftp, rlogin, rcp, nc) | depends on the other host | |

**Trust files** (rsh, rcp, rlogin): `/etc/hosts.equiv` lists hosts whose
users may use the same user name here without a password (not for root);
`~/.rhosts` lists `host user` pairs for one account, and for root only
`/.rhosts` counts. Host names come from `/etc/hosts`.

Leaving root without a password is fine on a board on your desk; set one
before connecting it to a network you do not control.

## Networking: SLIP over tty00

Networking is off until you enable it:

1. `cp /etc/rc.net.slip /etc/rc.net`
2. In `/etc/ttytab`, remove `getty` from the `tty00` line (SLIP needs the
   line).
3. Reboot (or run `. /etc/rc.net` as root).

The board is 10.0.0.2 (`rosco` in `/etc/hosts`), the other end of the line
10.0.0.1 (`host`). On a Linux PC with a USB serial adapter:

    slattach -p slip -s 38400 /dev/ttyUSB0 &
    ifconfig sl0 10.0.0.1 pointopoint 10.0.0.2 mtu 296 up

Use MTU 296: the board's TCP/IP (INET) sends 256-byte TCP segments and
cannot set an MTU itself; bigger packets from the other end get lost.
`/etc/rc.net` starts INET, the name daemon `nonamed` (names from
`/etc/hosts`), `slip`, and the servers below (through `tcpd`).

## Network services and tools

On the board (servers, started by `/etc/rc.net`):

* **ftp** (port 21): users with a password, and anonymous FTP.
* **telnet** (23): logins on the pseudo terminals ttyp0-3.
* **rsh/rcp** (514) and **rlogin** (513): by trust files, see above.

Clients on the board: `ftp`, `telnet`, `rsh`, `rcp`, `rlogin`, `nc`
(netcat: `nc host port`, `nc -l port`), `urlget` (HTTP and FTP downloads),
`ping`, `rdate host` (set the clock from a time server), `ifconfig`,
`pr_routes`.

Examples:

    rdate host                       set the clock from the PC (port 37)
    urlget http://host/file > file   fetch a file over HTTP
    nc -l 7777 > file                receive a file; on the PC:
                                     nc 10.0.0.2 7777 < file
    tar cf - dir | gzip | nc host 7777   send a directory

## Files over the serial console without networking

`kermit`, `rz`/`sz` (ZMODEM) work on the console line; `gzip`/`gunzip`
and `compress` save time on the line.

## Known limits

* 16-bit `int` (Minix-ST): programs and files are limited in places, e.g.
  objects of 64K at most in C programs.
* No real-time clock: set the time at boot, with `date`, or `rdate`.
* elvis: after ESC, `:` was reported to need an arrow key first with
  picocom (not reproduced in the emulator).
