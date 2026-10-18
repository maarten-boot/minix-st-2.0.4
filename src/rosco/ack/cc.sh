#!/bin/sh
# cc for Minix on the rosco_m68k (installed as /usr/bin/cc): the ACK passes
# (ack -mminix68k, in /usr/ack) with the Minix C library in /usr/ack/minix.
#
#	cc [-O] [-Dx] [-Ux] [-Idir] [-c] [-o out] [-lx] files
#
# .c and .s files are compiled with the Minix headers (/usr/include), then
# linked as  crtso.o emrts.o files libc.a libem.a libend.a.  -c only
# compiles.  -lx links /usr/ack/minix/libx.a.  A target script of the
# rosco build (src/rosco/ack/cc.sh); the host version is acc-minix.

ACKDIR=/usr/ack
export ACKDIR
ack=$ACKDIR/bin/ack
L=$ACKDIR/minix
P=$ACKDIR/share/ack/minix68k
defs="-D__ACK__ -D__minix=1 -D__m68000=1 -D_MINIX -D_POSIX_SOURCE -I/usr/include"

compile=
out=a.out
flags=
files=
libs=
while test $# -gt 0
do
	case "$1" in
	-c)	compile=1 ;;
	-o)	out=$2; shift ;;
	-l*)	libs="$libs $L/lib`expr "$1" : '-l\(.*\)'`.a" ;;
	-O*|-D*|-U*|-I*) flags="$flags $1" ;;
	-*)	echo "cc: unknown option $1" >&2; exit 1 ;;
	*)	files="$files $1" ;;
	esac
	shift
done

if test -n "$compile"
then
	if test "$out" = a.out
	then exec $ack -mminix68k $defs $flags -c $files
	else exec $ack -mminix68k $defs $flags -c -o $out $files
	fi
fi

objs=
for f in $files
do
	case "$f" in
	*.c|*.s)
		$ack -mminix68k $defs $flags -c $f || exit 1
		objs="$objs `basename $f | sed 's/\.[cs]$//'`.o" ;;
	*)	objs="$objs $f" ;;
	esac
done
exec $ack -mminix68k -o $out $L/crtso.o $L/emrts.o $objs $libs \
	$L/libc.a $P/libem.a $P/libend.a
