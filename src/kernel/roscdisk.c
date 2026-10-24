/* This file contains the disk driver of the rosco_m68k Classic v2.
 *
 * Two drives on controller 0:
 *
 *   c0d0  the SD card, through the firmware's SD routines (bit banged SPI
 *	   on the DUART output port; called through the extension function
 *	   table, see sd_efp_* in roscklib.S).  Minor 0 is the whole card,
 *	   minors 1-4 the primary partitions of its MBR (c0d0p0..p3).
 *   c0d1  the file system image the loader brings along at the end of
 *	   ROSCODE1.BIN (rosco_extra, moved to the top of RAM by main()),
 *	   minor 5.  It is the root device until the root is on the SD card;
 *	   it lives in RAM, so changes are lost on reset.
 *
 * The SD routines poll; the whole kernel waits while a block moves.  The
 * firmware keeps its SD state in its RAM at 0x1800-0x2000, which is why
 * the loader and the kernel leave that area alone (ROSCO_KEEP_FW).
 *
 * drvlib.c's partition() cannot be used: it copies the little endian MBR
 * entries as they are.  The primary partition table is read here; Minix
 * subpartitions and extended partitions are not supported yet.
 *
 * The driver uses the generic driver code (driver.c) and is hooked into
 * the controller table under the name "ACSI" (table.c knows acsi_task for
 * MACHINE == ATARI with NR_ACSI_DRIVES > 0): c0=ACSI in the boot
 * environment.
 *
 * The entry points into this file are:
 *   acsi_task:	main entry when the system is brought up
 *   sd_root_minor: find a Minix partition on the SD card for the root
 */

#include "kernel.h"
#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include "driver.h"
#include "drvlib.h"
#include "rosco.h"

#define	SD_MINORS	(1 + NR_PARTITIONS)	/* c0d0, c0d0p0..p3 */
#define	RD_MINOR	ROSCO_RD_MINOR		/* c0d1: the root image */

#if (RD_MINOR != SD_MINORS)
#error "ROSCO_RD_MINOR must follow the SD card's minors"
#endif

PRIVATE struct device sd_dev[SD_MINORS];	/* SD card: whole, partitions */
PRIVATE struct device rd_dev;		/* the image: base and size */
PRIVATE int rd_device;			/* current minor device */
PRIVATE int sd_state;			/* 0: untried, 1: ready, -1: failed */
PRIVATE int sd_opens;			/* open count, to rescan when 0 */
PRIVATE long sd_card[8];		/* firmware SDCard structure */
PRIVATE char sd_buf[SECTOR_SIZE];	/* bounce buffer */
PRIVATE unsigned char sd_type[NR_PARTITIONS];	/* partition types */

FORWARD _PROTOTYPE( char *rd_name, (void) );
FORWARD _PROTOTYPE( int rd_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int rd_do_close, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( struct device *rd_prepare, (int device) );
FORWARD _PROTOTYPE( int rd_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( void rd_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( int sd_open, (void) );
FORWARD _PROTOTYPE( void sd_partition, (void) );
FORWARD _PROTOTYPE( int sd_rw, (int opcode, unsigned long block,
					phys_bytes buf) );
FORWARD _PROTOTYPE( unsigned long le32, (unsigned char *p) );

/* Entry points to this driver. */
PRIVATE struct driver rd_dtab = {
  rd_name,	/* current device's name */
  rd_do_open,	/* open or mount */
  rd_do_close,	/* release */
  do_diocntl,	/* get or set a partition's geometry */
  rd_prepare,	/* prepare for I/O on a given minor device */
  rd_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  rd_geometry,	/* tell the geometry of the disk */
};


/*===========================================================================*
 *				acsi_task				     *
 *===========================================================================*/
PUBLIC void acsi_task()
{
  rd_dev.dv_base = cvul64((unsigned long) rosco_extra);
  rd_dev.dv_size = cvul64(rosco_extralen);
  driver_task(&rd_dtab);
}


/*===========================================================================*
 *				rd_name					     *
 *===========================================================================*/
PRIVATE char *rd_name()
{
  static char name[] = "c0d0";

  name[3] = rd_device == RD_MINOR ? '1' : '0';
  return(name);
}


/*===========================================================================*
 *				rd_do_open				     *
 *===========================================================================*/
PRIVATE int rd_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  if (rd_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  if (rd_device == RD_MINOR) return(OK);

  /* The SD card: initialize it on the first open. */
  if (sd_opens == 0 && sd_open() != OK) return(EIO);
  if (rd_device != 0 && cmp64u(sd_dev[rd_device].dv_size, 0) == 0)
	return(ENXIO);			/* no such partition */
  sd_opens++;
  return(OK);
}


/*===========================================================================*
 *				rd_do_close				     *
 *===========================================================================*/
PRIVATE int rd_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* After the last close of the card it is initialized again on the next
 * open, so a changed card is noticed.
 */
  if (rd_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  if (rd_device != RD_MINOR && sd_opens > 0 && --sd_opens == 0)
	sd_state = 0;
  return(OK);
}


/*===========================================================================*
 *				rd_prepare				     *
 *===========================================================================*/
PRIVATE struct device *rd_prepare(device)
int device;
{
/* Prepare for I/O on a device. */

  if (device == RD_MINOR) {
	if (rosco_extralen == 0) return(NIL_DEV);
	rd_device = device;
	return(&rd_dev);
  }
  if (device < 0 || device >= SD_MINORS) return(NIL_DEV);
  rd_device = device;
  return(&sd_dev[device]);
}


/*===========================================================================*
 *				rd_transfer				     *
 *===========================================================================*/
PRIVATE int rd_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write the image (as memory.c does for /dev/ram) or the SD card,
 * a 512 byte block at a time.
 */
  struct device *dv;
  phys_bytes mem_phys, user_phys;
  unsigned long count, dv_size;

  dv = rd_device == RD_MINOR ? &rd_dev : &sd_dev[rd_device];
  dv_size = cv64ul(dv->dv_size);

  while (nr_req > 0) {
	/* rosco: map each address (proc_vir2phys, shadowing) */
	user_phys = proc_vir2phys(proc_addr(proc_nr), iov->iov_addr);
	count = iov->iov_size;

	if (position >= dv_size) return(OK);		/* EOF */
	if (position + count > dv_size) count = dv_size - position;

	if (rd_device == RD_MINOR) {
		mem_phys = cv64ul(dv->dv_base) + position;
		if (opcode == DEV_GATHER) {
			phys_copy(mem_phys, user_phys, (phys_bytes) count);
		} else {
			phys_copy(user_phys, mem_phys, (phys_bytes) count);
		}
	} else {
		/* Whole blocks only, as FS and the partition code ask. */
		if ((position | count) & SECTOR_MASK) return(EINVAL);
		count = SECTOR_SIZE;
		if (sd_rw(opcode, (cv64ul(dv->dv_base) + position)
					>> SECTOR_SHIFT, user_phys) != OK)
			return(EIO);
	}

	/* Book the number of bytes transferred. */
	position += count;
	iov->iov_addr += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; }
  }
  return(OK);
}


/*===========================================================================*
 *				sd_open					     *
 *===========================================================================*/
PRIVATE int sd_open()
{
/* Initialize the card and read its partition table. */
  long r;
  int i;

  if (sd_state == 1) return(OK);
  for (i = 0; i < sizeof(sd_card) / sizeof(sd_card[0]); i++) sd_card[i] = 0;
  r = sd_efp_init((char *) sd_card);
  if (r != 0) {
	printf("c0d0: no SD card (init status %ld)\n", r);
	sd_state = -1;
	return(EIO);
  }
  sd_state = 1;

  /* The card size is not asked yet: the whole card is "as large as can be
   * addressed", the partitions limit what is used.
   */
  sd_dev[0].dv_base = cvul64(0);
  sd_dev[0].dv_size = cvul64(0xFFFFFE00L);
  sd_partition();
  return(OK);
}


/*===========================================================================*
 *				sd_partition				     *
 *===========================================================================*/
PRIVATE void sd_partition()
{
/* Read the MBR and set up the four primary partitions (c0d0p0..p3).  The
 * entries are little endian; empty ones get size 0.
 */
  unsigned char *pe;
  unsigned long low, size;
  int par;

  for (par = 1; par < SD_MINORS; par++) {
	sd_dev[par].dv_base = cvul64(0);
	sd_dev[par].dv_size = cvul64(0);
	sd_type[par - 1] = NO_PART;
  }
  if (sd_rw(DEV_GATHER, 0L, (phys_bytes) sd_buf) != OK) {
	printf("c0d0: can't read partition table\n");
	return;
  }
  if ((sd_buf[510] & 0xFF) != 0x55 || (sd_buf[511] & 0xFF) != 0xAA) return;

  for (par = 0; par < NR_PARTITIONS; par++) {
	pe = (unsigned char *) sd_buf + PART_TABLE_OFF + par * 16;
	if (pe[4] == NO_PART) continue;		/* sysind */
	low = le32(pe + 8);
	size = le32(pe + 12);
	if (low == 0 || size == 0 || low + size < low) continue;
	if (low + size > 0x7FFFFFL) size = 0x7FFFFFL - low;	/* 4GB */
	sd_dev[1 + par].dv_base = mul64u(low, SECTOR_SIZE);
	sd_dev[1 + par].dv_size = mul64u(size, SECTOR_SIZE);
	sd_type[par] = pe[4];
	printf("c0d0p%d: type %02x, %lu sectors at %lu\n",
		par, pe[4], size, low);
  }
}


/*===========================================================================*
 *				sd_root_minor				     *
 *===========================================================================*/
PUBLIC int sd_root_minor()
{
/* Called by initrosco() when no root image came with ROSCODE1.BIN: return
 * the minor device of the first Minix partition on the SD card (the card
 * may start with the FAT partition the firmware boots from), or -1.  The
 * card stays initialized for FS's open of the root device.
 */
  int par;

  if (sd_open() != OK) return(-1);
  for (par = 0; par < NR_PARTITIONS; par++) {
	if (sd_type[par] == MINIX_PART) return(1 + par);
  }
  printf("c0d0: no Minix partition on the SD card\n");
  return(-1);
}


/*===========================================================================*
 *				sd_rw					     *
 *===========================================================================*/
PRIVATE int sd_rw(opcode, block, buf)
int opcode;			/* DEV_GATHER or DEV_SCATTER */
unsigned long block;		/* 512 byte block on the card */
phys_bytes buf;			/* where the data goes or comes from */
{
/* Move one block.  The firmware wants an even buffer; odd user buffers go
 * through sd_buf.
 */
  char *p = (buf & 1) ? sd_buf : (char *) buf;
  long ok;

  if (sd_state != 1) return(EIO);
  if (opcode == DEV_GATHER) {
	ok = sd_efp_read((char *) sd_card, block, p);
	if (ok && p != (char *) buf)
		phys_copy((phys_bytes) p, buf, (phys_bytes) SECTOR_SIZE);
  } else {
	if (p != (char *) buf)
		phys_copy(buf, (phys_bytes) p, (phys_bytes) SECTOR_SIZE);
	ok = sd_efp_write((char *) sd_card, block, p);
  }
  if (!ok) {
	printf("c0d0: %s error, block %lu\n",
		opcode == DEV_GATHER ? "read" : "write", block);
	return(EIO);
  }
  return(OK);
}


/*===========================================================================*
 *				le32					     *
 *===========================================================================*/
PRIVATE unsigned long le32(p)
unsigned char *p;
{
  return ((unsigned long) p[3] << 24) | ((unsigned long) p[2] << 16)
	| ((unsigned long) p[1] << 8) | p[0];
}


/*===========================================================================*
 *				rd_geometry				     *
 *===========================================================================*/
PRIVATE void rd_geometry(entry)
struct partition *entry;
{
/* A made up geometry: 64 heads, 32 sectors. */
  struct device *dv;

  dv = rd_device == RD_MINOR ? &rd_dev : &sd_dev[rd_device];
  entry->cylinders = div64u(dv->dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
