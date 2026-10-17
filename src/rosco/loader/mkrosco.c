/* mkrosco - build ROSCODE1.BIN from the loader stub and a payload.
 *
 *	mkrosco [-l loadaddr] [-k lo:hi] [-e entry] [-x extra] -o out stub payload
 *
 * The payload is padded to a multiple of 4 and placed after the stub, the
 * optional extra image (e.g. a root file system) after the payload.  The
 * header in the stub (see loader.S) is filled in.  Without -e the entry
 * point is the payload's reset PC, the long at offset 4 of its vector table.
 * -k 0:0 disables the keep range (default 0x400:0x2000, the firmware's
 * System Data Area).
 *
 * This is a host program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	HDR_MAGIC	4
#define	HDR_PAYLOAD_OFF	8
#define	HDR_PAYLOAD_LEN	12
#define	HDR_LOAD_ADDR	16
#define	HDR_ENTRY	20
#define	HDR_KEEP_LO	24
#define	HDR_KEEP_HI	28
#define	HDR_EXTRA_OFF	32
#define	HDR_EXTRA_LEN	36
#define	HDR_FILE_LEN	40
#define	HDR_END		44

#define	LOAD_BASE	0x40000UL	/* where the firmware loads the file */
#define	RAM_SIZE	0x100000UL	/* on-board RAM */

static unsigned char *readfile(const char *name, unsigned long *lenp)
{
	FILE *f;
	unsigned char *buf;
	long len;

	if ((f = fopen(name, "rb")) == NULL) {
		perror(name);
		exit(1);
	}
	fseek(f, 0L, SEEK_END);
	len = ftell(f);
	rewind(f);
	if ((buf = malloc(len + 4)) == NULL) {
		fprintf(stderr, "mkrosco: out of memory\n");
		exit(1);
	}
	memset(buf, 0, len + 4);
	if (len > 0 && fread(buf, 1, len, f) != (size_t) len) {
		perror(name);
		exit(1);
	}
	fclose(f);
	*lenp = len;
	return buf;
}

static void put32(unsigned char *p, unsigned long v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}

static unsigned long get32(const unsigned char *p)
{
	return ((unsigned long) p[0] << 24) | ((unsigned long) p[1] << 16)
		| ((unsigned long) p[2] << 8) | p[3];
}

#define	ALIGN4(n)	(((n) + 3) & ~3UL)

static void usage(void)
{
	fprintf(stderr,
"usage: mkrosco [-l loadaddr] [-k lo:hi] [-e entry] [-x extra] -o out stub payload\n");
	exit(2);
}

int main(int argc, char **argv)
{
	unsigned char *stub, *payload, *extra = NULL, *out;
	unsigned long stub_len, payload_len, extra_len = 0;
	unsigned long payload_off, extra_off = 0, file_len;
	unsigned long load_addr = 0, keep_lo = 0x400, keep_hi = 0x2000, entry;
	int have_entry = 0, i;
	char *outname = NULL, *extraname = NULL;
	FILE *f;

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (i + 1 >= argc) usage();
		switch (argv[i][1]) {
		case 'l': load_addr = strtoul(argv[++i], NULL, 0); break;
		case 'e': entry = strtoul(argv[++i], NULL, 0); have_entry = 1;
			  break;
		case 'x': extraname = argv[++i]; break;
		case 'o': outname = argv[++i]; break;
		case 'k':
			if (sscanf(argv[++i], "%li:%li",
				   (long *) &keep_lo, (long *) &keep_hi) != 2)
				usage();
			break;
		default: usage();
		}
	}
	if (outname == NULL || argc - i != 2) usage();

	stub = readfile(argv[i], &stub_len);
	payload = readfile(argv[i + 1], &payload_len);
	if (extraname != NULL) extra = readfile(extraname, &extra_len);

	if (stub_len < HDR_END || memcmp(stub + HDR_MAGIC, "MXRO", 4) != 0) {
		fprintf(stderr, "mkrosco: %s is not the loader stub\n", argv[i]);
		exit(1);
	}
	if (!have_entry) {
		if (payload_len < 8) {
			fprintf(stderr, "mkrosco: payload too short\n");
			exit(1);
		}
		entry = get32(payload + 4);
	}
	if (keep_hi < keep_lo || (keep_lo & 3) || (keep_hi & 3)) {
		fprintf(stderr, "mkrosco: bad keep range\n");
		exit(1);
	}

	payload_off = ALIGN4(stub_len);
	payload_len = ALIGN4(payload_len);
	file_len = payload_off + payload_len;
	if (extra != NULL) {
		extra_off = file_len;
		file_len += ALIGN4(extra_len);
	}
	if (LOAD_BASE + file_len >= RAM_SIZE)
		fprintf(stderr, "mkrosco: warning: %lu bytes will not fit "
			"in on-board RAM at 0x%lx\n", file_len, LOAD_BASE);

	if ((out = calloc(1, file_len)) == NULL) {
		fprintf(stderr, "mkrosco: out of memory\n");
		exit(1);
	}
	memcpy(out, stub, stub_len);
	memcpy(out + payload_off, payload, payload_len);
	if (extra != NULL) memcpy(out + extra_off, extra, extra_len);

	put32(out + HDR_PAYLOAD_OFF, payload_off);
	put32(out + HDR_PAYLOAD_LEN, payload_len);
	put32(out + HDR_LOAD_ADDR, load_addr);
	put32(out + HDR_ENTRY, entry);
	put32(out + HDR_KEEP_LO, keep_lo);
	put32(out + HDR_KEEP_HI, keep_hi);
	put32(out + HDR_EXTRA_OFF, extra_off);
	put32(out + HDR_EXTRA_LEN, extra_len);
	put32(out + HDR_FILE_LEN, file_len);

	if ((f = fopen(outname, "wb")) == NULL
	    || fwrite(out, 1, file_len, f) != file_len || fclose(f) != 0) {
		perror(outname);
		exit(1);
	}
	printf("%s: %lu bytes, payload %lu at 0x%lx -> 0x%lx, entry 0x%lx",
		outname, file_len, payload_len, LOAD_BASE + payload_off,
		load_addr, entry);
	if (keep_hi > keep_lo)
		printf(", keep 0x%lx-0x%lx", keep_lo, keep_hi);
	if (extra != NULL)
		printf(", extra %lu at 0x%lx", extra_len, LOAD_BASE + extra_off);
	printf("\n");
	return 0;
}
