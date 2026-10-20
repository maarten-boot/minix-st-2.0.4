/* nc - a small netcat for Minix TCP.
 *
 *	nc [-v] host port	connect to port on host
 *	nc [-v] -l port		wait for one connection on port
 *
 * Copies standard input to the connection and the connection to standard
 * output.  At the end of standard input the sending side is shut down, so
 * the other end sees end of file; nc exits when the other end closes.  The
 * port is a number or a name from /etc/services.  -v reports the
 * connection on standard error.
 *
 * Minix has no select(), so a child process copies standard input to the
 * connection, while the parent copies the connection to standard output
 * (as slip does).  Written for the rosco_m68k port.
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

static char *prog;
static int verbose;

static void fatal(label)
char *label;
{
  fprintf(stderr, "%s: %s: %s\n", prog, label, strerror(errno));
  exit(1);
}

static void usage()
{
  fprintf(stderr, "Usage: %s [-v] host port\n       %s [-v] -l port\n",
	prog, prog);
  exit(1);
}

static tcpport_t getport(name)
char *name;
{
/* A port number or a service name, in network byte order. */
  struct servent *sp;
  unsigned long p;
  char *end;

  if ((sp = getservbyname(name, "tcp")) != NULL) return sp->s_port;
  p = strtoul(name, &end, 10);
  if (*end != 0 || p == 0 || p > 0xFFFF) {
	fprintf(stderr, "%s: %s: unknown port\n", prog, name);
	exit(1);
  }
  return htons((tcpport_t) p);
}

static int copy(from, to)
int from, to;
{
/* Copy until end of file or an error; return 0 at EOF, -1 on an error. */
  static char buf[1024];
  int n, w, off;

  for (;;) {
	if ((n = read(from, buf, sizeof(buf))) <= 0) return n;
	for (off = 0; off < n; off += w) {
		if ((w = write(to, buf + off, n - off)) <= 0) return -1;
	}
  }
}

int main(argc, argv)
int argc;
char **argv;
{
  char *tcp_device, *host = NULL, *port_s;
  int i, fd, listen = 0;
  pid_t child;
  struct hostent *hp;
  ipaddr_t addr;
  nwio_tcpconf_t conf;
  nwio_tcpcl_t cl;

  prog = argv[0];
  for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != 0; i++) {
	if (strcmp(argv[i], "-l") == 0) listen = 1;
	else if (strcmp(argv[i], "-v") == 0) verbose = 1;
	else usage();
  }
  if (listen) {
	if (argc - i != 1) usage();
  } else {
	if (argc - i != 2) usage();
	host = argv[i++];
  }
  port_s = argv[i];

  if ((tcp_device = getenv("TCP_DEVICE")) == NULL) tcp_device = TCP_DEVICE;
  if ((fd = open(tcp_device, O_RDWR)) < 0) fatal(tcp_device);

  if (listen) {
	conf.nwtc_flags = NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
	conf.nwtc_locport = getport(port_s);
	if (ioctl(fd, NWIOSTCPCONF, &conf) < 0) fatal("NWIOSTCPCONF");
	cl.nwtcl_flags = 0;
	if (ioctl(fd, NWIOTCPLISTEN, &cl) < 0) fatal("NWIOTCPLISTEN");
  } else {
	if ((hp = gethostbyname(host)) != NULL) {
		memcpy(&addr, hp->h_addr, sizeof(addr));
	} else if ((addr = inet_addr(host)) == (ipaddr_t) -1) {
		fprintf(stderr, "%s: %s: unknown host\n", prog, host);
		exit(1);
	}
	conf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	conf.nwtc_remaddr = addr;
	conf.nwtc_remport = getport(port_s);
	if (ioctl(fd, NWIOSTCPCONF, &conf) < 0) fatal("NWIOSTCPCONF");
	cl.nwtcl_flags = 0;
	if (ioctl(fd, NWIOTCPCONN, &cl) < 0) fatal(host);
  }
  if (verbose && ioctl(fd, NWIOGTCPCONF, &conf) == 0) {
	fprintf(stderr, "%s: connected to %s port %u\n", prog,
		inet_ntoa(conf.nwtc_remaddr), ntohs(conf.nwtc_remport));
  }

  switch (child = fork()) {
  case -1:
	fatal("fork");
  case 0:
	/* Standard input to the connection, then tell the other end. */
	copy(0, fd);
	(void) ioctl(fd, NWIOTCPSHUTDOWN, NULL);
	_exit(0);
  }
  /* The connection to standard output, until the other end closes. */
  copy(fd, 1);
  kill(child, SIGTERM);
  (void) waitpid(child, NULL, 0);
  return 0;
}
