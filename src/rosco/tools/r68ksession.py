#!/usr/bin/env python3
"""Run a scripted console session in r68k (the quick emulator).

	r68ksession.py r68k program logfile script

Starts `r68k -d program` with 5MB RAM (R68K_RAM) in the current directory,
where r68k takes rosco_sd.bin as the SD card, and works through the script
on its console (stdin/stdout), with the commands of mamesession.py:

	expect TEXT[<tab>TIMEOUT]	wait for TEXT in the output (default 60 s)
	send TEXT			type TEXT and Return
	type TEXT			type TEXT only; Python escapes (\\x1b, \\r)
	sleep SECONDS			just collect output

Everything the console prints goes to logfile.  For the port: in a
directory with a copy of rosco_sd.bin,

	r68ksession.py .../r68k/r68k .../src/rosco/boot/sd/ROSCODE1.BIN log script

r68k raises an address error on an odd word/long access, as a 68010.
This is a host program.
"""
import os
import select
import subprocess
import sys
import time

if len(sys.argv) != 5:
    sys.exit("usage: r68ksession.py r68k program logfile script")
r68k, prog, logf, script = sys.argv[1:5]
env = dict(os.environ, R68K_RAM='5M')
p = subprocess.Popen([r68k, '-d', prog], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
log = open(logf, 'wb')
buf = b''


def pump(t):
    global buf
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            d = os.read(p.stdout.fileno(), 4096)
            if not d:
                return
            buf += d
            log.write(d)
            log.flush()


def expect(text, t):
    global buf
    end = time.time() + t
    while time.time() < end:
        i = buf.find(text.encode())
        if i >= 0:
            buf = buf[i + len(text):]
            return True
        pump(0.3)
        if p.poll() is not None:
            break
    log.write(b'\n[session: timeout waiting for %r]\n' % text.encode())
    return False


try:
    for line in open(script):
        line = line.rstrip('\n')
        if not line or line.startswith('#'):
            continue
        cmd, _, arg = line.partition(' ')
        if cmd == 'expect':
            t = 60
            if '\t' in arg:
                arg, t = arg.split('\t')
                t = float(t)
            if not expect(arg, t):
                break
        elif cmd == 'send':
            p.stdin.write(arg.encode() + b'\r')
            p.stdin.flush()
        elif cmd == 'type':
            p.stdin.write(arg.encode().decode('unicode_escape').encode('latin-1'))
            p.stdin.flush()
        elif cmd == 'sleep':
            pump(float(arg))
finally:
    pump(1)
    p.kill()
    p.wait()
