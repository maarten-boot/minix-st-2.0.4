#!/usr/bin/env python3
"""Run a scripted console session in the rosco-emulator (MAME).

	mamesession.py [-m machine] mamedir sdimage logfile script

Starts mamedir/rosco with sdimage on -hard1 and the console (DUART A) on a
TCP socket, connects to it and works through the script, one command per
line ('#' starts a comment):

	expect TEXT[<tab>TIMEOUT]	wait for TEXT in the output (default 60 s)
	send TEXT			type TEXT and Return
	sleep SECONDS			just collect output

Everything the console prints goes to logfile.  The emulator runs
unthrottled and is stopped at the end of the script or at the first
expect that times out.  The kernel must be built for 115200 baud, the
speed of the emulated serial ports: make CONSBAUD=115200 sdcard.
This is a host program.
"""
import socket
import subprocess
import sys
import time
import os

args = sys.argv[1:]
machine = "rosco_m68k_010"
if args[:1] == ["-m"]:
    machine = args[1]
    args = args[2:]
if len(args) != 4:
    sys.exit("usage: mamesession.py [-m machine] mamedir sdimage logfile script")
mamedir, sd, logf, script = args
sd = os.path.abspath(sd)
port = 23000 + os.getpid() % 1000
p = subprocess.Popen(['./rosco', machine, '-hard1', sd,
                      '-terminal', 'null_modem', '-bitb', 'socket.localhost:%d' % port,
                      '-video', 'none', '-sound', 'none', '-seconds_to_run', '100000', '-nothrottle'],
                     cwd=mamedir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
s = None
for i in range(100):
    try:
        s = socket.create_connection(('localhost', port)); break
    except OSError:
        time.sleep(0.2)
if s is None:
    p.kill(); sys.exit("no connection")
s.settimeout(0.5)
buf = b''
log = open(logf, 'wb')
def pump(t):
    global buf
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(4096)
            if not d: return
            buf += d; log.write(d); log.flush()
        except socket.timeout:
            pass
def expect(text, timeout):
    global buf
    end = time.time() + timeout
    while time.time() < end:
        i = buf.find(text.encode())
        if i >= 0:
            buf = buf[i + len(text):]; return True
        pump(0.5)
    log.write(b'\n[session: timeout waiting for %r]\n' % text.encode()); return False
try:
    for line in open(script):
        line = line.rstrip('\n')
        if not line or line.startswith('#'): continue
        cmd, _, arg = line.partition(' ')
        if cmd == 'expect':
            t = 60
            if '\t' in arg: arg, t = arg.split('\t'); t = float(t)
            if not expect(arg, t): break
        elif cmd == 'send':
            s.sendall(arg.encode() + b'\r')
        elif cmd == 'sleep':
            pump(float(arg))
finally:
    pump(1)
    p.kill(); p.wait()
