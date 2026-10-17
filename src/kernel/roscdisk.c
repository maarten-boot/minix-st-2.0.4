/* This file contains the disk driver of the rosco_m68k Classic v2.
 *
 * Milestone M4: one "disk", the file system image the loader brings along
 * at the end of ROSCODE1.BIN (rosco_extra, moved to the top of RAM by
 * main()).  It is the root device, read and written in memory; the
 * contents are lost on reset.  Milestone M5 adds the SD card here.
 *
 * The driver uses the generic driver code (driver.c) and is hooked into
 * the controller table under the name "ACSI" (table.c knows acsi_task for
 * MACHINE == ATARI with NR_ACSI_DRIVES > 0), so the boot environment says
 * c0=ACSI and rootdev=DEV_HD0.
 *
 * Minor devices: 0 is the whole image.  There are no partitions yet.
 *
 * The entry point into this file is:
 *   acsi_task:	main entry when the system is brought up
 */

#include "kernel.h"
#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include "driver.h"
#include "rosco.h"

PRIVATE struct device rd_dev;		/* the image: base and size */

FORWARD _PROTOTYPE( char *rd_name, (void) );
FORWARD _PROTOTYPE( int rd_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( struct device *rd_prepare, (int device) );
FORWARD _PROTOTYPE( int rd_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( void rd_geometry, (struct partition *entry) );

/* Entry points to this driver. */
PRIVATE struct driver rd_dtab = {
  rd_name,	/* current device's name */
  rd_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
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
  return("c0d0");
}


/*===========================================================================*
 *				rd_do_open				     *
 *===========================================================================*/
PRIVATE int rd_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  if (rd_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  return(OK);
}


/*===========================================================================*
 *				rd_prepare				     *
 *===========================================================================*/
PRIVATE struct device *rd_prepare(device)
int device;
{
/* Prepare for I/O on a device: only minor 0, and only with an image. */

  if (device != 0 || rosco_extralen == 0) return(NIL_DEV);
  return(&rd_dev);
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
/* Read or write the image, as memory.c does for /dev/ram. */

  phys_bytes mem_phys, user_phys;
  unsigned long count, dv_size = cv64ul(rd_dev.dv_size);
  phys_bytes user_base = proc_vir2phys(proc_addr(proc_nr), 0);

  while (nr_req > 0) {
	user_phys = user_base + iov->iov_addr;
	count = iov->iov_size;

	if (position >= dv_size) return(OK);		/* EOF */
	if (position + count > dv_size) count = dv_size - position;
	mem_phys = cv64ul(rd_dev.dv_base) + position;

	if (opcode == DEV_GATHER) {
		phys_copy(mem_phys, user_phys, (phys_bytes) count);
	} else {
		phys_copy(user_phys, mem_phys, (phys_bytes) count);
	}

	/* Book the number of bytes transferred. */
	position += count;
	iov->iov_addr += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; }
  }
  return(OK);
}


/*===========================================================================*
 *				rd_geometry				     *
 *===========================================================================*/
PRIVATE void rd_geometry(entry)
struct partition *entry;
{
/* A made up geometry: 64 heads, 32 sectors. */

  entry->cylinders = (rosco_extralen >> SECTOR_SHIFT) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
