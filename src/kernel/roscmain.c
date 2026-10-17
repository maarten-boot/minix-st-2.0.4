/* This file contains the main program of MINIX for the rosco_m68k Classic
 * v2.  The routine main() initializes the system and starts the ball
 * rolling by setting up the proc table, interrupt vectors, and scheduling
 * each task to run to initialize itself.
 *
 * It is stmain.c with the Atari ST hardware replaced by the XR68C681 DUART:
 * initrosco() instead of initst(), duartint() as the only interrupt
 * handler, the memory size from the loader instead of probing.
 *
 * The entries into this file are:
 *   main:		MINIX main program
 *   none:		called for an interrupt to an unused vector
 *   rupt:		called for an unexpected interrupt (async)
 *   trap:		called for an unexpected trap (synchronous)
 *   checksp:		check the stack of the process being restarted
 *   panic:		abort MINIX due to a fatal error
 *   wreboot:		halt or reboot
 *   getenv:		kernel version of getenv
 *   idle_task:		the idle task
 *   duartint:		DUART interrupt handler
 *   duart_imr_set/clr:	change the DUART interrupt mask
 */

#include "kernel.h"
#if (MACHINE == ATARI && ATARI_TYPE == ROSCO)
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/boot.h>
#include "proc.h"
#include "roscaddr.h"
#include "roscduart.h"
#include "rosco.h"

FORWARD _PROTOTYPE( void initrosco, (void)				);
FORWARD _PROTOTYPE( void fake_task, (char *s)				);
FORWARD _PROTOTYPE( void k_putenv, (_CONST char *name, _CONST char *val));
FORWARD _PROTOTYPE( char *ul2a, (u32_t n, unsigned base)		);

/* The proc table offsets used by roscompx.S (sava6 .. savtt).  A wrong
 * value gives the compile error "array size is negative".
 */
#define	K_OFF(f)	__builtin_offsetof(struct proc, f)
extern int dummy_proc_check[
	K_OFF(p_reg.fp) == 56 && K_OFF(p_reg.sp) == 60 &&
	K_OFF(p_reg.pc) == 64 && K_OFF(p_reg.psw) == 68 &&
	K_OFF(p_trap) + 1 == 77 ? 1 : -1];

#if (HZ != ROSCO_HZ)
#error "ROSCO_HZ in <minix/config.h> must be equal to HZ"
#endif

/* Environment strings. */
#define	ENV_SIZE	192
PRIVATE char k_environ[ENV_SIZE];
PRIVATE char *ep = k_environ;

PRIVATE unsigned char duart_imr;	/* shadow of the write-only IMR */

#define	ALIGNMENT	   4

/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int t;
  register vir_clicks size;
  register phys_clicks base;
  reg_t ktsb;
  struct tasktab *ttp;

  mem[1].size = 0;	/* initialize */
  mem[0].base = 0;
#if FIXED_MEM_SIZE
  mem[0].size = (vir_clicks)((FIXED_MEM_SIZE*1024*1024) >> CLICK_SHIFT);
#else
  mem[0].size = (vir_clicks)(get_mem_size((char *)0) >> CLICK_SHIFT);
#endif /* FIXED_MEM_SIZE */
  tot_mem_size = mem[0].size + mem[1].size;	/* total RAM */

  initrosco();

  /* Clear the process table.
   * Set up mappings for proc_addr() and proc_number() macros.
   */
  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++t) {
        rp->p_priority = PPRI_NONE;
        rp->p_nr = t;           /* proc number from ptr */
        (pproc_addr + NR_TASKS)[t] = rp;        /* proc ptr from number */
  }

  mapdrivers();

  size = sizes[0] + sizes[1];	/* kernel text + data size */
  base = size;			/* end of kernel */

  ktsb = ((reg_t) t_stack + (ALIGNMENT - 1)) & ~((reg_t) ALIGNMENT - 1);

  for (t = -NR_TASKS; t <= LOW_USER; t++) {	/* for all drivers */
	rp = proc_addr(t);			/* t's process slot */
	ttp = &tasktab[t + NR_TASKS];		/* t's task attributes */
	strcpy(rp->p_name, ttp->name);
	if (t < 0) {				/* kernel task */
		if (ttp->stksize > 0) {
			rp->p_stguard = (reg_t *) ktsb;
			*rp->p_stguard = STACK_GUARD;
		}
		ktsb += ttp->stksize;
		rp->p_reg.sp = ktsb;
		rp->p_splow = rp->p_reg.sp;
		rp->p_reg.pc = (reg_t) ttp->initial_pc;
		rp->p_priority = PPRI_TASK;
		if (!isidlehardware(t)) {
			lock_ready(rp);	/* IDLE, HARDWARE neveready */
			rp->p_reg.psw = 0x2200;	/* S-BIT, SPL2 */
		} else {
			rp->p_reg.psw = 0x0200;	/* SPL2 */
		}
		rp->p_map[T].mem_len  = sizes[0];
		rp->p_map[D].mem_len  = sizes[1];
		rp->p_map[D].mem_phys = sizes[0];
		rp->p_map[S].mem_phys = size;
		rp->p_map[D].mem_vir  = rp->p_map[D].mem_phys;
		rp->p_map[S].mem_vir  = rp->p_map[S].mem_phys;
	} else {				/* MM, FS, INET, INIT */
		rp->p_reg.psw = (reg_t)0x0200;	/* no S-BIT, SPL2 */
		rp->p_reg.pc = (reg_t) ((long)base << CLICK_SHIFT);
		size = sizes[2*t + 2];
		rp->p_map[T].mem_len  = size;
		rp->p_map[T].mem_phys = base;
		base += size;
		size = sizes[2*t + 3];
		rp->p_map[D].mem_len  = size;
		rp->p_map[D].mem_phys = base;
		base += size;
		rp->p_map[S].mem_len  = 0;
		rp->p_map[S].mem_phys = base;
		/* setup stack */
		rp->p_reg.sp = (reg_t) ((long) base << CLICK_SHIFT) - 4;
		rp->p_splow = rp->p_reg.sp;
		* (reg_t *) rp->p_splow = 0; 	/* argc = 0, user task */
		rp->p_priority = t < LOW_USER ? PPRI_SERVER : PPRI_USER;
		rp->p_map[T].mem_vir = rp->p_map[T].mem_phys;
		rp->p_map[D].mem_vir = rp->p_map[D].mem_phys;
		rp->p_map[S].mem_vir = rp->p_map[S].mem_phys;
		/* A kernel-only image (milestone M2) has no MM, FS or INIT:
		 * do not run what is not there.
		 */
		if (rp->p_map[T].mem_len + rp->p_map[D].mem_len != 0)
			lock_ready(rp);
	}
	rp->p_flags = 0;
  }

  rp = proc_addr(HARDWARE);
  rp->p_map[D].mem_len  = ~0;	/* maximum size */
  rp->p_map[D].mem_phys = 0;
  rp->p_map[D].mem_vir  = 0;

  mem[0].size -= base;			/* used by system */
  mem[0].base += base;			/* memory no longer free */

  proc[NR_TASKS+INIT_PROC_NR].p_pid = 1;/* INIT of course has pid 1 */
  bill_ptr = proc_addr(HARDWARE);	/* it has to point somewhere */
  proc_addr(IDLE)->p_priority = PPRI_IDLE;
  lock_pick_proc();
}


/*===========================================================================*
 *                              none, rupt, trap                             *
 *===========================================================================*/
PUBLIC void none()
{
  panic("Nonexisting interrupt. Vector =", proc_ptr->p_trap);
}

PUBLIC void rupt()
{
  panic("Unexpected interrupt.  Vector =", proc_ptr->p_trap);
}

PUBLIC void trap()
{
  register int t;
  register struct proc *rp;
  static char vecsig[] = {
	0, 0, SIGSEGV, SIGBUS, SIGILL, SIGILL, SIGILL, SIGABRT,
	SIGILL, SIGTRAP, SIGEMT, SIGFPE, SIGSTKFLT
  };

  rp = proc_ptr;
  t = rp->p_trap;
  if (rp->p_reg.psw & 0x2000)
  {
	printf("\nkernel trap: pc=%lx sr=%x\n",
		(long) rp->p_reg.pc, rp->p_reg.psw);
	p_dmp();
  	panic("trap via vector", t);
  }
  if (t >= 0 && t < sizeof(vecsig)/sizeof(vecsig[0]) && vecsig[t]) {
	t = vecsig[t];
  } else {
	printf("\nUnexpected trap.  Vector = %d\n", t);
	printf("This may be due to accidentally including\n");
	printf("a non-MINIX library routine that is trying to make a system call.\n");
	t = SIGILL;
  }

  if (t != SIGSTKFLT && t != SIGTRAP) {	/* DEBUG */
	printf("sig=%d to pid=%d at pc=%lx\n",
		t, rp->p_pid, rp->p_reg.pc);
  }

  cause_sig(proc_number(rp), t);
}

/*===========================================================================*
 *                              checksp		                             *
 *===========================================================================*/
PUBLIC void checksp()
{
  register struct proc *rp;
  register phys_bytes ad;

  rp = proc_ptr;
  /* if a user process is in supervisor mode don't check stack */
  if ((rp->p_nr >= 0) && (rp->p_reg.psw & 0x2000)) return;
  if ((rp->p_reg.sp < rp->p_splow) || (rp->p_splow == 0))
	rp->p_splow = rp->p_reg.sp;
  if (rp->p_map[S].mem_len == 0)
	return;
  ad = (phys_bytes)rp->p_map[S].mem_vir << CLICK_SHIFT;
  if ((phys_bytes)rp->p_reg.sp > ad)
	return;
  /*
   * Stack violation.
   */
  ad = (phys_bytes)rp->p_map[D].mem_vir;
  ad += (phys_bytes)rp->p_map[D].mem_len;
  ad <<= CLICK_SHIFT;
  if ((phys_bytes)rp->p_reg.sp < ad + CLICK_SIZE)
	printf("Stack low (pid=%d,pc=%lx,sp=%lx,end=%lx)\n",
		rp->p_pid, (long)rp->p_reg.pc,
		(long)rp->p_reg.sp, (long)ad);

  rp->p_trap = 12;	/* fake trap causing SIGSTKFLT */
  trap();
}

/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(s,n)
_CONST char *s;
int n;
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck.
 */

  if (*s != 0) {
	printf("\nKernel panic: %s",s);
	if (n != NO_NUM) printf(" %d", n);
	printf("\n");
  }
#ifdef REBOOT_AFTER_PANIC
  wreboot(RBT_REBOOT);
#else
  printf("\nPush RESET button\n");
  lock();
  for (;;)
	;
#endif /* REBOOT_AFTER_PANIC */
}

/*==========================================================================*
 *				wreboot					    *
 *==========================================================================*/
PUBLIC void wreboot(how)
int how;		/* 0 = halt, 1 = reboot, 2 = panic!, ... */
{
  long l;

  switch (how) {
    case RBT_HALT:
  	printf("\nSystem halted\n");
	lock();
  	for (;;);
	break;
	/* notreached */
    case RBT_REBOOT:
    	printf("\nRebooting ...\n");
	for (l=0; l<100000; l++) ;	/* let the last characters go out */
    	kreboot();
	break;
    	/* notreached */
    case RBT_PANIC:
    	panic("", how);
	break;
    	/* notreached */
    default:
    	panic("", how);
    	/* notreached */
  }
  /* notreached */
}

/*==========================================================================*
 *				getenv					    *
 *==========================================================================*/
PUBLIC char *getenv(name)
_CONST char *name;
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */

  register _CONST char *namep;
  register char *envp;

  for (envp = k_environ; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NIL_PTR);
}


/*===========================================================================*
 *                                 initrosco                                 *
 *===========================================================================*/
PRIVATE void initrosco()
{
/* Set up the DUART: console on channel A as the firmware has it (8N1,
 * 115200 baud, RTS asserted; roscrs232.c takes it over), channel B off,
 * the counter/timer as the clock at HZ, and the interrupt vector.  The
 * SD card pins on the output port are not touched.
 */

  DUART->du_imr = duart_imr = 0;	/* everything masked */

  /* Channel A: the firmware's settings (firmware duart.asm), so a console
   * without firmware initialization works too.  Let the loader's last
   * characters go out first; the receiver and transmitter are not reset.
   */
  while ((DUART->du_sra & SR_TXEMT) == 0)
	;
  DUART->du_cra = CR_RSTERR;
  DUART->du_cra = CR_RSTMR;
  DUART->du_mra = MR1_NOPAR | MR1_CS8;	/* MR1A */
  DUART->du_mra = MR2_NORMAL | MR2_STOP1; /* MR2A */
  DUART->du_acr = ACR_BRG2 | ACR_TIMER16;
  DUART->du_cra = CR_XRXEXT;		/* XR extended rates: 115200 */
  DUART->du_cra = CR_XTXEXT;
  DUART->du_csra = CSR_115200_X;
  DUART->du_opset = OP_RTSA;		/* assert RTS A */
  DUART->du_cra = CR_RXEN | CR_TXEN;

  /* Channel B: disabled until rs_init() (roscrs232.c) sets it up. */
  DUART->du_crb = CR_RXDIS | CR_TXDIS;

  /* The clock: timer mode, crystal/16, HZ interrupts per second. */
  DUART->du_ctur = DUART_TIMER(HZ) >> 8;
  DUART->du_ctlr = DUART_TIMER(HZ) & 0xFF;
  (void) DUART->du_startcc;

  DUART->du_ivr = ROSCO_DUART_VEC;
  duart_imr_set(ISR_CTR);

/* warning: fs expects decimal numbers */
  k_putenv("rootdev", ul2a((unsigned long) DEV_RAM, 10));
  k_putenv("ramimagedev", ul2a((unsigned long) DEV_RAM, 10));
  k_putenv("ramsize", ul2a((unsigned long) 0, 10));	/* size of ramdisk */
  k_putenv("processor", ul2a((unsigned long) M68000, 10));
  k_putenv("NR_BUFS", ul2a((unsigned long) NR_BUFS, 10));
  k_putenv("DMA_SECTORS", ul2a((unsigned long) DMA_SECTORS, 10));
  k_putenv("ENABLE_SYMLINKS", ul2a((unsigned long) ENABLE_SYMLINKS, 10));
  k_putenv("ENABLE_SWAP", ul2a((unsigned long) ENABLE_SWAP, 10));
  k_putenv("NR_CONS", ul2a((unsigned long) NR_CONS, 10));
  k_putenv("NR_PTYS", ul2a((unsigned long) NR_PTYS, 10));
  mon_params = (phys_bytes) k_environ;
  mon_parmsize = ep - k_environ + 1;

  printf("rosco_m68k: 680%d0, %ldkB RAM, kernel %ldkB\n", proctyp,
	(long) rosco_memsize >> 10,
	(long) (sizes[0] + sizes[1]) << CLICK_SHIFT >> 10);
}

/*===========================================================================*
 *				duartint				     *
 *===========================================================================*/
PUBLIC void duartint(dummy)
int dummy;
{
/* The DUART interrupt (IPL 4, vector ROSCO_DUART_VEC).  Find out which of
 * the unmasked sources is active and serve it.  Called through genint with
 * interrupts disabled.
 */
  int isr;

  isr = DUART->du_isr & duart_imr;

  if (isr & ISR_CTR) {
	(void) DUART->du_stopcc;	/* timer mode: only clears ISR_CTR */
	clock_handler(0);
  }
  if (isr & (ISR_RXRDYA | ISR_TXRDYA | ISR_RXRDYB | ISR_TXRDYB))
	rs_duart_int(isr);
}

/*===========================================================================*
 *				duart_imr_set/clr			     *
 *===========================================================================*/
PUBLIC void duart_imr_set(bits)
int bits;
{
/* Enable DUART interrupt sources.  The IMR cannot be read back. */
  int s = lock();

  duart_imr |= bits;
  DUART->du_imr = duart_imr;
  restore(s);
}

PUBLIC void duart_imr_clr(bits)
int bits;
{
/* Disable DUART interrupt sources. */
  int s = lock();

  duart_imr &= ~bits;
  DUART->du_imr = duart_imr;
  restore(s);
}

/*===========================================================================*
 *				idle task				     *
 *===========================================================================*/
PUBLIC void idle_task()
{
  while (1);
}

/*===========================================================================*
 *			fake kernel environment stuff			     *
 *===========================================================================*/
PRIVATE void k_putenv(name, val)
_CONST char *name;
_CONST char *val;
{
  /* append "name=num\0" to k_environ */
  strcpy(ep, name);
  strcat(ep, "=");
  strcat(ep, val);
  while (*ep++)
	;
  *ep = '\0';
}

#define arraysize(a)             (sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)            ((a) + arraysize(a))

PRIVATE char *ul2a(n, b)
u32_t n;
unsigned b;
/* Transform a long number to ascii at base b, (b >= 8). */
{
  static char num[(CHAR_BIT * sizeof(n) + 2) / 3 + 1];
  char *a= arraylimit(num) - 1;
  static char hex[16] = "0123456789ABCDEF";

  do *--a = hex[(int) (n % b)]; while ((n/= b) > 0);
  return a;
}

/*===========================================================================*
 *				fake tasks				     *
 *===========================================================================*/
PRIVATE void fake_task(s)
char *s;
{
  message m;

  for (;;) {
	receive(ANY, &m);
	printf("%s received %d from %d\n", s, m.m_type, m.m_source);
  }
}

#if (NR_ACSI_DRIVES == 0)
PUBLIC void winchester_task()
{
  fake_task("winchester_task");
}
#endif

#if (NR_SCSI_DRIVES == 0)
PUBLIC void scsi_task()
{
  fake_task("scsi_task");
}
#endif

#if (NR_FD_DRIVES == 0)
PUBLIC void floppy_task()
{
  fake_task("floppy_task");
}
#endif

#endif /* MACHINE == ATARI && ATARI_TYPE == ROSCO */
