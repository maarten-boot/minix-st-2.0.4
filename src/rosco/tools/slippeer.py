#!/usr/bin/env python3
"""The host end of a SLIP line to the rosco_m68k, in a network namespace.

	slippeer.py [-t] [-w dir] serial local remote

Creates the TUN interface sl0 (MTU 296) with address 'local', point to
point to 'remote' (the board), and passes IP packets between it and 'serial', the
SLIP line: a Unix socket path, or host:port for TCP (e.g. MAME's tty00 port
through socat).  Every packet is logged as one line.  -t also serves the
RFC 868 time protocol (TCP port 37) on 'local', for rdate on the board;
-w serves the files in 'dir' over HTTP (port 80), for urlget.

It needs CAP_NET_ADMIN: run it in a container with its own network
namespace, so the host's networking is not touched, e.g.

	docker run --rm --cap-add NET_ADMIN --device /dev/net/tun \\
		-v DIR:DIR roscopeco/rosco_m68k python3 slippeer.py ...

The kernel of the namespace answers ping and runs the TCP/IP side; nothing
else is needed.  This is a host program.
"""

import fcntl
import os
import select
import socket
import struct
import sys
import threading
import time

END, ESC, ESC_END, ESC_ESC = 0o300, 0o333, 0o334, 0o335

TUNSETIFF = 0x400454ca
IFF_TUN, IFF_NO_PI, IFF_UP = 0x0001, 0x1000, 0x1
SIOCSIFFLAGS, SIOCSIFADDR, SIOCSIFDSTADDR = 0x8914, 0x8916, 0x8918
SIOCSIFNETMASK, SIOCGIFFLAGS, SIOCSIFMTU = 0x891c, 0x8913, 0x8922
MTU = 296       # classic SLIP; the board's INET announces TCP MSS 256


def ifreq_addr(name, addr):
    sa = struct.pack("HH4s8x", socket.AF_INET, 0, socket.inet_aton(addr))
    return struct.pack("16s16s", name.encode(), sa)


def tun_open(name, local, remote):
    fd = os.open("/dev/net/tun", os.O_RDWR)
    fcntl.ioctl(fd, TUNSETIFF, struct.pack("16sH", name.encode(),
                                          IFF_TUN | IFF_NO_PI))
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fcntl.ioctl(s, SIOCSIFADDR, ifreq_addr(name, local))
    fcntl.ioctl(s, SIOCSIFDSTADDR, ifreq_addr(name, remote))
    fcntl.ioctl(s, SIOCSIFNETMASK, ifreq_addr(name, "255.255.255.255"))
    fcntl.ioctl(s, SIOCSIFMTU, struct.pack("16si", name.encode(), MTU))
    flags = struct.unpack("16sH", fcntl.ioctl(
        s, SIOCGIFFLAGS, struct.pack("16sH", name.encode(), 0)))[1]
    fcntl.ioctl(s, SIOCSIFFLAGS, struct.pack("16sH", name.encode(),
                                            flags | IFF_UP))
    s.close()
    return fd


def slip_encode(pkt):
    out = bytearray([END])
    for b in pkt:
        if b == END:
            out += bytes([ESC, ESC_END])
        elif b == ESC:
            out += bytes([ESC, ESC_ESC])
        else:
            out.append(b)
    out.append(END)
    return bytes(out)


def describe(pkt):
    if len(pkt) < 20 or pkt[0] >> 4 != 4:
        return "%d bytes, not IPv4" % len(pkt)
    proto = {1: "icmp", 6: "tcp", 17: "udp"}.get(pkt[9], str(pkt[9]))
    return "%s %s -> %s, %d bytes" % (proto, socket.inet_ntoa(pkt[12:16]),
                                      socket.inet_ntoa(pkt[16:20]), len(pkt))


def time_server(local):
    """RFC 868: on connect, send the seconds since 1900 (4 bytes)."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((local, 37))
    srv.listen(4)
    while True:
        c, a = srv.accept()
        c.sendall(struct.pack(">I", int(time.time()) + 2208988800))
        c.close()
        log("time served to %s" % a[0])


def http_server(local, root):
    import functools
    import http.server
    handler = functools.partial(http.server.SimpleHTTPRequestHandler,
                                directory=root)
    http.server.HTTPServer((local, 80), handler).serve_forever()


def log(msg):
    print("slippeer: %s" % msg, flush=True)


def main():
    args = sys.argv[1:]
    timesrv, www = False, None
    while args[:1] in (["-t"], ["-w"]):
        if args[0] == "-t":
            timesrv, args = True, args[1:]
        else:
            www, args = args[1], args[2:]
    if len(args) != 3:
        sys.exit("usage: slippeer.py [-t] [-w dir] serial local remote")
    serial, local, remote = args
    tun = tun_open("sl0", local, remote)
    if ":" in serial:
        host, port = serial.rsplit(":", 1)
        line = socket.create_connection((host, int(port)))
    else:
        line = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        line.connect(serial)
    log("sl0 %s, point to point %s, line %s" % (local, remote, serial))
    if timesrv:
        threading.Thread(target=time_server, args=(local,),
                         daemon=True).start()
    if www:
        threading.Thread(target=http_server, args=(local, www),
                         daemon=True).start()
    frame, esc = bytearray(), False
    while True:
        r, _, _ = select.select([line, tun], [], [])
        if tun in r:
            pkt = os.read(tun, 2048)
            if pkt[:1] and pkt[0] >> 4 != 4:
                continue        # IPv6 (router solicitations): not for INET
            log("to board:   " + describe(pkt))
            line.sendall(slip_encode(pkt))
        if line in r:
            data = line.recv(4096)
            if not data:
                log("line closed")
                return
            for b in data:
                if esc:
                    frame.append(END if b == ESC_END else
                                 ESC if b == ESC_ESC else b)
                    esc = False
                elif b == ESC:
                    esc = True
                elif b == END:
                    if frame and frame[0] >> 4 != 4:
                        # not IPv4 (e.g. a getty's output on the line
                        # before SLIP starts): the TUN would refuse it
                        log("from board: " + describe(bytes(frame)) +
                            ", dropped")
                    elif frame:
                        log("from board: " + describe(bytes(frame)))
                        os.write(tun, bytes(frame))
                    frame = bytearray()
                else:
                    frame.append(b)


if __name__ == "__main__":
    main()
