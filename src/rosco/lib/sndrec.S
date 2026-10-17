/*
 * send, receive, sendrec: trap #0 to the kernel (lib/m68000/rts/sndrec.s,
 * send.s, receive.s, sendrec.s in GNU syntax).  <minix/syslib.h> maps the
 * C names send/receive/sendrec to _send/_receive/_sendrec.
 *	_send(dest, ptr), _receive(src, ptr), _sendrec(srcdest, ptr)
 * destroy d0, d1, a0.  int at 4(sp), message pointer at 6(sp).
 */

SEND	= 1
RECEIVE	= 2
BOTH	= 3

	.text
	.globl	_send, _receive, _sendrec, send, receive, sendrec
_send:	move.w	#SEND,%d0
	bra	1f
_receive:
	move.w	#RECEIVE,%d0
	bra	1f
_sendrec:
	move.w	#BOTH,%d0
1:	move.w	4(%sp),%d1		/* d1 = dest-src */
	move.l	6(%sp),%a0		/* a0 = message pointer */
	trap	#0
	rts

send:	jmp	_send
receive: jmp	_receive
sendrec: jmp	_sendrec
