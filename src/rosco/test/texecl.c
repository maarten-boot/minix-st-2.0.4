/* texecl - does execl() pass its arguments?  Directly and after fork().
 *	texecl		exec ourselves with "child", then fork and do it again
 *	texecl child	print argc and argv
 * (rosco bring-up test: the argv of execl() is on the caller's stack)
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

/* what execl() sees: its arguments as an array, from &arg1 */
void show(const char *path, const char *arg1, ...)
{
  char **v = (char **) &arg1;
  int i;

  printf("  &arg1 vector:");
  for (i = 0; i < 4 && v[i] != NULL; i++) printf(" [%s]", v[i]);
  printf(" (%d)\n", i);
}

int main(argc, argv)
int argc;
char *argv[];
{
  int i, s;

  if (argc > 1) {
	printf("  exec'd: argc=%d", argc);
	for (i = 0; i < argc; i++) printf(" [%s]", argv[i]);
	printf("\n");
	exit(strcmp(argv[1], "fork") == 0 ? 3 : 0);
  }
  printf("Test execl\n");
  if (getenv("DIRECT") != NULL) {
	char *av[3];

	show("./texecl", "texecl", "direct", (char *) 0);
	av[0] = "texecl"; av[1] = "viaexecve"; av[2] = NULL;
	if (strcmp(getenv("DIRECT"), "1") == 0) execve("./texecl", av, environ);
	execl("./texecl", "texecl", "direct", (char *) 0);
	printf("  execl failed\n");
	return 1;
  }
  if (fork() == 0) {
	execl("./texecl", "texecl", "fork", (char *) 0);
	printf("  execl after fork failed\n");
	exit(1);
  }
  wait(&s);
  printf("  child exit status %d (3 = argv ok)\n", WEXITSTATUS(s));
  execl("./texecl", "texecl", "direct", (char *) 0);
  printf("  execl failed\n");
  return 1;
}
