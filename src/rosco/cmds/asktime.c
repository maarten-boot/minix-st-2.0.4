/* asktime - ask for the date and time at boot, with a timeout.
 *
 *	asktime [-t seconds]
 *
 * The rosco_m68k has no real-time clock: /etc/rc starts the clock at the
 * time the image was made, then runs asktime on the console.  It shows the
 * current time and reads [[MMDDYY]hhmm[ss]] (GMT, as date(1) takes it).
 * An answer is set with /bin/date; RETURN, or no answer within the timeout
 * (default 30 seconds), keeps the clock as it is.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static void onalarm(sig)
int sig;
{
/* Just interrupt the read. */
}

static void tell(s)
char *s;
{
  write(1, s, strlen(s));
}

int main(argc, argv)
int argc;
char **argv;
{
  struct sigaction sa;
  char buf[40];
  char *p;
  int n, secs = 30;
  char c;
  long l;
  time_t now;

  if (argc == 3 && strcmp(argv[1], "-t") == 0) {
	/* alarm(0) would wait forever, and an int is 16 bits: take 1 to
	 * 32767 seconds only.
	 */
	l = strtol(argv[2], &p, 10);
	if (*p != 0 || p == argv[2] || l < 1 || l > 32767) argc = 0;
	secs = l;
  }
  if (argc != 1 && argc != 3) {
	tell("Usage: asktime [-t seconds]\n");
	return 1;
  }

  time(&now);
  tell("\nThe clock is at ");
  tell(ctime(&now));
  tell("Enter the date (GMT) as [[MMDDYY]hhmm[ss]], RETURN keeps it ");
  sprintf(buf, "(%d s): ", secs);
  tell(buf);

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = onalarm;
  sigaction(SIGALRM, &sa, (struct sigaction *) NULL);
  alarm(secs);
  n = read(0, buf, sizeof(buf) - 1);
  alarm(0);

  if (n < 0) {
	/* Timeout: drop a half typed line, so getty does not get it. */
	tcflush(0, TCIFLUSH);
	tell("\nno answer, clock kept\n");
	return 0;
  }
  if (n > 0 && buf[n-1] != '\n') {
	/* More than fits in buf: read the rest of the line, so getty does
	 * not get it (but keep what is typed after it).
	 */
	while (read(0, &c, 1) == 1 && c != '\n') {}
  }
  while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
	n--;
  buf[n] = 0;
  if (n == 0) return 0;

  /* date wants pairs of digits: hhmm, hhmmss, MMDDYYhhmm or MMDDYYhhmmss */
  for (p = buf; *p != 0; p++) {
	if (*p < '0' || *p > '9') break;
  }
  if (*p != 0 || (n != 4 && n != 6 && n != 10 && n != 12)) {
	tell("not [[MMDDYY]hhmm[ss]], clock kept\n");
	return 0;
  }
  execl("/bin/date", "date", buf, (char *) NULL);
  tell("can't execute /bin/date\n");
  return 1;
}
