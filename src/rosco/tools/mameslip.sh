#!/bin/sh
# Run a mamesession.py script with tty00 (DUART B) connected over SLIP to a
# Linux network namespace (tools/slippeer.py in a container), for testing
# networking in the rosco-emulator (MAME):
#
#	tools/mameslip.sh sdimage logfile script [peerlog]
#
# SLIPN=n (0-9, default 0) selects one of several sessions that can run
# at the same time: port 24555+n, /tmp/rslip<n>, container slippeer<n>
# (n 0 is /tmp/rslip and slippeer, as before).
#
# MAME gets tty00 on TCP port 24555 (null_modem, 115200 baud: set tty00 to
# 115200 on the board, e.g. speed=115200 in /etc/rc.net), socat relays it to
# the Unix socket /tmp/rslip/s, and the container runs slippeer.py with the
# time server, and an HTTP server for /tmp/rslip/www when that exists: the
# host end is 10.0.0.1, the board 10.0.0.2.  Needs docker
# (the roscopeco/rosco_m68k image) and socat.  Run from src/rosco.  This
# is a host program.

[ $# -ge 3 ] || { echo "usage: $0 sdimage logfile script [peerlog]" >&2; exit 1; }
img=$1 log=$2 script=$3 peerlog=${4:-/dev/null}
n=${SLIPN:-0}
case $n in [0-9]) ;; *) echo "$0: SLIPN must be 0-9" >&2; exit 1; esac
port=$((24555 + n))
if [ $n = 0 ]; then dir=/tmp/rslip name=slippeer; else dir=/tmp/rslip$n name=slippeer$n; fi
tools=$(cd "$(dirname "$0")" && pwd)
mkdir -p $dir
rm -f $dir/s
python3 "$tools/mamesession.py" \
	-a "-bios v242 -ram 5m -host null_modem -bitb2 socket.localhost:$port" \
	../../../rosco-emulator "$img" "$log" "$script" &
mpid=$!
sleep 8
socat UNIX-LISTEN:$dir/s TCP:localhost:$port &
spid=$!
sleep 1
docker run --rm --name $name -u 0 --cap-add NET_ADMIN \
	--device /dev/net/tun -v $dir:$dir -v "$tools:/tools" \
	--entrypoint /home/linuxbrew/.linuxbrew/bin/python3 \
	roscopeco/rosco_m68k /tools/slippeer.py -t \
	$( [ -d $dir/www ] && echo -w $dir/www ) \
	$dir/s 10.0.0.1 10.0.0.2 \
	> "$peerlog" 2>&1 &
wait $mpid
docker stop -t 1 $name >/dev/null 2>&1
kill $spid 2>/dev/null
