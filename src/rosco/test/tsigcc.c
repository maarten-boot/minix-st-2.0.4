/* tsigcc - does a signal handler leave the condition codes alone?
 *	tsigcc [loops]	default 2000
 * A child sends SIGUSR1 to the parent as fast as it can, while the parent
 * runs a loop of compares and branches.  After the handler, sigreturn must
 * give the interrupted code back its own condition codes (X N Z V C): a
 * signal between a compare and its branch must not change the branch.
 * (rosco: before the sigreturn fix the status register was the one of the
 * handler's sigreturn call, and the branch went wrong now and then)
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile long sigs;

void catch(sig)
int sig;
{
  signal(SIGUSR1, catch);
  sigs++;
}

/* 10000 rounds of two compares, each followed by the branch that must not
 * be taken; returns the number of branches that were taken anyway.
 */
long cmploop()
{
  long n = 10000, bad = 0, a = 1, b = 2;

  __asm__ volatile (
	"1:	cmp.l	%2,%2\n"	/* Z set */
	"	bne	2f\n"
	"	cmp.l	%2,%3\n"	/* Z clear */
	"	beq	2f\n"
	"	cmp.l	%3,%2\n"	/* 1 - 2: C set */
	"	bcc	2f\n"
	"	bra	3f\n"
	"2:	addq.l	#1,%1\n"
	"3:	subq.l	#1,%0\n"
	"	bgt	1b\n"
	: "+d" (n), "+d" (bad)
	: "d" (a), "d" (b)
	: "cc");
  return bad;
}

int main(argc, argv)
int argc;
char *argv[];
{
  int i, loops;
  long bad = 0;
  pid_t pid, ppid;

  loops = argc > 1 ? atoi(argv[1]) : 2000;
  printf("Test sigcc ");
  fflush(stdout);
  signal(SIGUSR1, catch);
  ppid = getpid();
  if ((pid = fork()) < 0) {
	printf("cannot fork\n");
	return 1;
  }
  if (pid == 0) {
	signal(SIGUSR1, SIG_IGN);
	for (;;) if (kill(ppid, SIGUSR1) != 0) _exit(0);
  }
  for (i = 0; i < loops; i++) bad += cmploop();
  kill(pid, SIGKILL);
  wait((int *) 0);
  if (bad == 0 && sigs > 0) {
	printf("ok\n");
	return 0;
  }
  printf("failed: %ld wrong branches, %ld signals, %d loops\n",
	bad, sigs, loops);
  return 1;
}
