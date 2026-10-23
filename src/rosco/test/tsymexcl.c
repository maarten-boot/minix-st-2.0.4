/* tsymexcl - creating a file over a symbolic link, as POSIX says
 *	tsymexcl
 * open() with O_CREAT|O_EXCL must fail with EEXIST when the last component
 * is a symbolic link, also a dangling one; mkdir() and mkfifo() must not
 * follow a symbolic link either.  open() with O_CREAT alone does follow it
 * and creates the target.  Runs in /tmp.
 * (rosco: before the new_node() fix FS followed the link in all these
 * cases and created its target, so an O_EXCL temporary file could be
 * redirected to any file by a planted link)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int errs;

void e(n, what)
int n;
char *what;
{
  printf("\n  %d: %s", n, what);
  errs++;
}

/* Is there a file (not a link) at this path? */
int there(path)
char *path;
{
  struct stat st;

  return lstat(path, &st) == 0;
}

void clean()
{
  unlink("tsx_l1"); unlink("tsx_t1");
  unlink("tsx_l2"); unlink("tsx_t2");
  unlink("tsx_l3"); unlink("tsx_t3");
  unlink("tsx_l4"); rmdir("tsx_t4");
  unlink("tsx_l5"); unlink("tsx_t5");
  unlink("tsx_l6");
  unlink("tsx_n7");
}

int main()
{
  int fd;

  printf("Test symexcl ");
  fflush(stdout);
  if (chdir("/tmp") != 0) {
	printf("cannot cd to /tmp\n");
	return 1;
  }
  clean();

  /* 1: O_CREAT|O_EXCL on a dangling link: EEXIST, no target made. */
  symlink("tsx_t1", "tsx_l1");
  fd = open("tsx_l1", O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd >= 0 || errno != EEXIST) e(1, "O_EXCL on a dangling link did not fail with EEXIST");
  if (fd >= 0) close(fd);
  if (there("tsx_t1")) e(1, "O_EXCL created the target of a dangling link");

  /* 2: O_CREAT|O_EXCL on a link to an existing file: EEXIST. */
  close(creat("tsx_t2", 0644));
  symlink("tsx_t2", "tsx_l2");
  fd = open("tsx_l2", O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd >= 0 || errno != EEXIST) e(2, "O_EXCL on a link to a file did not fail with EEXIST");
  if (fd >= 0) close(fd);

  /* 3: O_CREAT alone follows a dangling link and creates the target. */
  symlink("tsx_t3", "tsx_l3");
  fd = open("tsx_l3", O_WRONLY | O_CREAT, 0644);
  if (fd < 0) e(3, "O_CREAT through a dangling link failed");
  else close(fd);
  if (!there("tsx_t3")) e(3, "O_CREAT did not create the target of the link");

  /* 4: mkdir() on a dangling link: EEXIST, no directory made. */
  symlink("tsx_t4", "tsx_l4");
  if (mkdir("tsx_l4", 0755) == 0 || errno != EEXIST) e(4, "mkdir on a dangling link did not fail with EEXIST");
  if (there("tsx_t4")) e(4, "mkdir created the target of a dangling link");

  /* 5: mkfifo() on a dangling link: EEXIST, no fifo made. */
  symlink("tsx_t5", "tsx_l5");
  if (mkfifo("tsx_l5", 0644) == 0 || errno != EEXIST) e(5, "mkfifo on a dangling link did not fail with EEXIST");
  if (there("tsx_t5")) e(5, "mkfifo created the target of a dangling link");

  /* 6: symlink() over an existing link: EEXIST. */
  symlink("tsx_t6", "tsx_l6");
  if (symlink("tsx_x6", "tsx_l6") == 0 || errno != EEXIST) e(6, "symlink over a link did not fail with EEXIST");

  /* 7: plain O_CREAT|O_EXCL: a new name works, an existing one fails. */
  fd = open("tsx_n7", O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0) e(7, "O_EXCL on a new name failed");
  else close(fd);
  fd = open("tsx_n7", O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd >= 0 || errno != EEXIST) e(7, "O_EXCL on an existing file did not fail with EEXIST");
  if (fd >= 0) close(fd);

  clean();
  if (errs == 0) {
	printf("ok\n");
	return 0;
  }
  printf("\nfailed: %d errors\n", errs);
  return 1;
}
