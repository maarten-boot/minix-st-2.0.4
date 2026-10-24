/* tshadow2 - can a shadowed process write its text to a terminal?
 *	tshadow2 [rounds]	default 200; as root (the ptys are mode 600)
 * The parent writes a const buffer (in the text segment) to the pty slave
 * /dev/ttyp3; a forked child reads it from the master /dev/ptyp3 and
 * compares.  The pty takes 128 bytes at a time, so the parent waits in FS
 * while the TTY task copies the rest, and the child, in the same memory
 * slot, makes the clock task swap the parent out to a shadow meanwhile.
 * proc_vir2phys() must then map data and stack to the shadow, but leave
 * text where it is.
 * (rosco: before the proc_vir2phys() fix the terminal got bytes from
 * where the text would be in the shadow, as with umap() in tshadow)
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define CHUNK	1024

const char pat[CHUNK] = "A const buffer in the text segment, written to a pty";
char buf[CHUNK];

int main(argc, argv)
int argc;
char *argv[];
{
  int i, n, m, got, mfd, sfd, bad, status;
  long total;
  pid_t pid;
  struct termios tc;

  n = argc > 1 ? atoi(argv[1]) : 200;
  printf("Test shadow2 ");
  fflush(stdout);
  /* The slave without output processing: the bytes arrive as written. */
  if ((mfd = open("/dev/ptyp3", O_RDWR)) < 0 ||
      (sfd = open("/dev/ttyp3", O_RDWR)) < 0) {
	printf("cannot open /dev/ptyp3 and /dev/ttyp3 (root?)\n");
	return 1;
  }
  tcgetattr(sfd, &tc);
  tc.c_oflag &= ~OPOST;
  tc.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
  tcsetattr(sfd, TCSANOW, &tc);

  if ((pid = fork()) < 0) {
	printf("cannot fork\n");
	return 1;
  }
  if (pid == 0) {
	/* Reader: compare every chunk with pat, exit with the bad count. */
	close(sfd);
	bad = 0;
	for (i = 0; i < n; i++) {
		for (got = 0; got < CHUNK; got += m) {
			m = read(mfd, buf + got, CHUNK - got);
			if (m <= 0) _exit(255);
		}
		if (memcmp(buf, pat, CHUNK) != 0) bad++;
	}
	_exit(bad > 254 ? 254 : bad);
  }

  /* Writer. */
  total = 0;
  for (i = 0; i < n; i++) {
	if (write(sfd, pat, CHUNK) != CHUNK) break;
	total += CHUNK;
  }
  if (waitpid(pid, &status, 0) != pid) status = 255 << 8;
  close(sfd);
  close(mfd);
  bad = WIFEXITED(status) ? WEXITSTATUS(status) : 255;
  if (bad == 0 && total == (long) n * CHUNK) {
	printf("ok\n");
	return 0;
  }
  printf("failed: %d of %d chunks wrong, %ld bytes written\n", bad, n, total);
  return 1;
}
