# execl0.sed - terminate execl() argument lists with a pointer.
#
# execl() uses its arguments as the argv array (lib/posix/_execl.c).  With
# gcc -mshort a bare 0 is a 2-byte int, not a 4-byte null pointer, so the
# array was not terminated and execve() read garbage (in.rshd died with
# SIGSEGV).  Applied to copies of the files below (cmds/Makefile, obj/fix).
/execl[ep]*(/s/, *0)/, (char *) 0)/
# zmodem/rz.c had no terminator at all
s/execl("\/bin\/sh", "sh", "-c", s);/execl("\/bin\/sh", "sh", "-c", s, (char *) 0);/
