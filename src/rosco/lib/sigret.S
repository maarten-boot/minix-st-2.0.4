/*
 * __sigreturn: the return address of a signal handler (installed by
 * sigaction).  As lib/m68000/rts/__sigreturn.s: jump to _sigreturn()
 * (lib/posix/_sigreturn.c), which finds the sigcontext pointer where it
 * expects its argument and does not return.
 */
	.text
	.globl	__sigreturn
__sigreturn:
	jmp	_sigreturn
