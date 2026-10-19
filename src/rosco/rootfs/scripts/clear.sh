#!/bin/sh
#
#	clear 1.0 - clear the screen			Author: Kees J. Bot
#								30 Mar 1996
# rosco: commands/scripts/clear.sh without "export PATH=...", which the
# old Minix sh does not take.
PATH=/bin:/usr/bin
exec tget -str cl
