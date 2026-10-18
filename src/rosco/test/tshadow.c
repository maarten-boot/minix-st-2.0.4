/* tshadow - can a shadowed process pass the kernel pointers into its text?
 *	tshadow [rounds]	default 6000
 * A child that never blocks keeps the parent's memory slot busy, so the
 * clock task now and then swaps the parent out to a shadow while it waits
 * for FS.  umap() must then map data and stack to the shadow, but leave
 * text (string constants, const data) where it is.  The parent stats a
 * string constant and writes a const buffer, reads it back and compares.
 * (rosco: before the umap() fix about 1 write in 80 put garbage in the
 * file, and test17/test18 now and then could not stat a file just made)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char pat[4096] = "a const buffer, in the text segment";
char buf[4096];

int main(argc, argv)
int argc;
char *argv[];
{
  int i, n, fd, badstat = 0, badwrite = 0;
  pid_t pid;
  struct stat st;

  n = argc > 1 ? atoi(argv[1]) : 6000;
  printf("Test shadow ");
  fflush(stdout);
  if (chdir("/tmp") != 0 ||
      (fd = open("tshadow.f", O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
	printf("cannot create /tmp/tshadow.f\n");
	return 1;
  }
  if ((pid = fork()) < 0) {
	printf("cannot fork\n");
	return 1;
  }
  if (pid == 0) for (;;) ;		/* always ready to run */
  for (i = 0; i < n; i++) {
	if (stat("tshadow.f", &st) != 0) badstat++;
	lseek(fd, 0L, SEEK_SET);
	write(fd, pat, sizeof(pat));
	lseek(fd, 0L, SEEK_SET);
	memset(buf, 0, sizeof(buf));
	read(fd, buf, sizeof(buf));
	if (memcmp(buf, pat, sizeof(pat)) != 0) badwrite++;
  }
  kill(pid, SIGKILL);
  wait((int *) 0);
  close(fd);
  unlink("tshadow.f");
  if (badstat == 0 && badwrite == 0) {
	printf("ok\n");
	return 0;
  }
  printf("failed: %d stats, %d writes of %d\n", badstat, badwrite, n);
  return 1;
}
