#include <sys/devops.h>
#include <sys/conf.h>	
#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/file.h>	
#include <sys/errno.h>	
#include <sys/open.h>	
#include <sys/cred.h>	
#include <sys/uio.h>
#include <sys/stat.h>	
#include <sys/ksynch.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/time.h>

#include "constants.h"

#define THR_CNT_NAME				"thr_cnt"
#define THR_CNT_MAX_READ			255	//max # integers can be read at a time
//#define DEBUG

static int thr_cnt_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int thr_cnt_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int thr_cnt_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
	void **resultp);
static int thr_cnt_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
	int flags, char *name, caddr_t valuep, int *lengthp);
static int thr_cnt_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int thr_cnt_close(dev_t dev, int flag, int otyp, cred_t *cred);
static int thr_cnt_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int thr_cnt_write(dev_t dev, struct uio *uiop, cred_t *credp);

static int thr_cnt_pass_time_till(timespec_t *until);
static int thr_cnt_get_nthreads(void);
static int thr_cnt_avail_data_size(void);
static void thr_cnt_read_loop(void);

static int thr_cnt_threads_ar[THR_CNT_AR_SIZE];
static volatile int thr_cnt_write_pos;
static volatile int thr_cnt_read_pos;
static volatile int thr_cnt_is_reading;
dev_info_t	*devi;

// cb_ops structure thr_cnt
static struct cb_ops thr_cnt_cb_ops = {
	thr_cnt_open,
	thr_cnt_close,
	nodev,				// no strategy - nodev returns ENXIO thr_cnt
	nodev,				// no print thr_cnt
	nodev,				// no dump thr_cnt
	thr_cnt_read,
	thr_cnt_write,
	nodev,				// no ioctl
	nodev,				// no devmap thr_cnt
	nodev,				// no mmap thr_cnt
	nodev,				// no segmap thr_cnt
	nochpoll,		 	// returns ENXIO for non-pollable devices thr_cnt
	thr_cnt_prop_op,
	NULL,				// streamtab struct thr_cnt
	D_NEW | D_MP,		// compatibility flags: see conf.h thr_cnt
	CB_REV,				// cb_ops revision number thr_cnt
	nodev,				// no aread thr_cnt
	nodev				// no awrite thr_cnt
};

// dev_ops structure thr_cnt
static struct dev_ops thr_cnt_dev_ops = {
	DEVO_REV,
	0,						// reference count thr_cnt
	thr_cnt_getinfo,
	nulldev,				// no identify(9E) - nulldev returns 0 thr_cnt
	nulldev,				// no probe(9E) thr_cnt
	thr_cnt_attach,
	thr_cnt_detach,
	nodev,					// no reset - nodev returns ENXIO thr_cnt
	&thr_cnt_cb_ops,
	(struct bus_ops *)NULL,
	nodev
};

static struct modldrv modldrv = {
	&mod_driverops,
	"thr_cnt driver",
	&thr_cnt_dev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

// Loadable module configuration entry points thr_cnt
int
_init(void)
{
	thr_cnt_is_reading = 0;
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return(mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int retval;
	if ((retval = mod_remove(&modlinkage)) != 0)
			return (retval);
	return (retval);
}

// Device autoconfiguration entry points thr_cnt
static int
thr_cnt_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	
	switch (cmd) {
	case DDI_ATTACH:
		if (ddi_create_minor_node(dip, THR_CNT_NAME, S_IFCHR, instance,
		  DDI_PSEUDO, 0) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "Cannot create minor node for %d", instance);
			return (DDI_FAILURE);
		}
		devi = dip;
		return (DDI_SUCCESS);
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
thr_cnt_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	thr_cnt_is_reading = 0;
	switch (cmd) {
	case DDI_DETACH:
		ddi_remove_minor_node(dip, NULL);
		return (DDI_SUCCESS);
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
thr_cnt_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	int retval = DDI_FAILURE;
	ASSERT(resultp != NULL);
	switch(cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*resultp = devi;
		retval = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*resultp = NULL;// (void *)getminor((dev_t)arg);
		retval = DDI_SUCCESS;
		break;
	default:
		return DDI_FAILURE;
	}
	return (retval);
}

static int
thr_cnt_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
		int flags, char *name, caddr_t valuep, int *lengthp)
{
	return(ddi_prop_op(dev,dip,prop_op,flags,name,valuep,lengthp));
}

// Use context entry points thr_cnt
static int
thr_cnt_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	return (0);
}

static int
thr_cnt_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	return (0);
}

static int
thr_cnt_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	int retval, read_length, timeout_cnt;
	timespec_t timeout_timer;
	static int timeout_sec = THR_CNT_AR_SIZE / 4
	  * (THR_CNT_SAMPLING_PERIOD_NS / 100000) / 10000;

	timeout_cnt = 0;
	//wait if the available data is less than the amount collected in a second
	while((read_length = thr_cnt_avail_data_size()) == 0
	  || (thr_cnt_read_pos < THR_CNT_AR_SIZE - THR_CNT_SAMPLING_RATE
	      && read_length < THR_CNT_SAMPLING_RATE) ) {
		//sleep for 1/4th of the total array time
		gethrestime(&timeout_timer);
		timeout_timer.tv_sec += timeout_sec;
		thr_cnt_pass_time_till(&timeout_timer);
		
		timeout_cnt++;
		if(timeout_cnt > 2) {
#ifdef DEBUG
			cmn_err(CE_NOTE, "Timeout reached with %d seconds!\n", timeout_sec);
#endif
			return 0;
		}
	}
	
	//OS does not read longer than THR_CNT_MAX_READ at a time
	if (read_length > THR_CNT_MAX_READ)
		read_length = THR_CNT_MAX_READ;

	//read
#ifdef DEBUG
	cmn_err(CE_WARN, "Reading %d elements from position %d\n", read_length, thr_cnt_read_pos);
#endif
	retval = uiomove((void*)(&thr_cnt_threads_ar[thr_cnt_read_pos]), read_length * sizeof(int), UIO_READ, uiop);
	
	//update read position
	thr_cnt_read_pos += read_length;
	thr_cnt_read_pos %= THR_CNT_AR_SIZE;
	
	return (retval);
}

static int
thr_cnt_write(dev_t dev, struct uio *uiop, cred_t *credp)
{
	int retval, errno;
	char dummy[10];
	
	//dummy write
	errno = uiop->uio_resid;
	if (errno > 9)
		errno = 9;
	retval = uiomove(dummy, errno, UIO_WRITE, uiop);
	if (uiop->uio_resid > 9)
		return retval;

	//record threads
	if (thr_cnt_is_reading == 1) {
		thr_cnt_is_reading = 0;
		return retval;
	}
	thr_cnt_is_reading = 1;

	thr_cnt_read_pos = 0;
	thr_cnt_write_pos = 0;

	cmn_err(CE_NOTE, "Recording number of threads\n");
	thr_cnt_read_loop();
	
	return retval;
}

//non-blocking sleep
//block causes us to wait for the next scheduler tick (up to 10ms)
static int
thr_cnt_pass_time_till(timespec_t *until)
{
	timespec_t now;
	for (;;) {
		gethrestime(&now);
		if (now.tv_sec > until->tv_sec || 
		  (now.tv_sec == until->tv_sec && now.tv_nsec >= until->tv_nsec) )
			break;
	}
	return 0;
}

int
thr_cnt_get_nthreads(void)
{
	int i, nthreads = 0;
	for (i = 0; i < ncpus; i++) {
		cpu_t *cp = cpu[i];
		if (CPU_ACTIVE(cp)){
			if (cp->cpu_thread != cp->cpu_idle_thread &&
			  cp->cpu_thread != cp->cpu_pause_thread)
				nthreads++;
			nthreads += cp->cpu_disp->disp_q->dq_sruncnt;
		}
	}
	return nthreads;
}

static int
thr_cnt_avail_data_size(void)
{
	int wr_pos = thr_cnt_write_pos;
	if (wr_pos == THR_CNT_AR_SIZE)
		wr_pos = 0;
	int avail_data = wr_pos - thr_cnt_read_pos;
	
	//do not wrap the array
	if (avail_data < 0)
		avail_data = THR_CNT_AR_SIZE - thr_cnt_read_pos;
	
#ifdef DEBUG
	cmn_err(CE_NOTE, "avail_data=%d \n", avail_data,
	  thr_cnt_read_pos);
#endif	
	
	return avail_data;
}

static void
thr_cnt_read_loop(void)
{
	timespec_t next_sample_time;
	int passed_seconds = 0;
	gethrestime(&next_sample_time);
	while (thr_cnt_is_reading != 0) {
		if (next_sample_time.tv_nsec < (THR_CNT_1E9 - THR_CNT_SAMPLING_PERIOD_NS)) {
			next_sample_time.tv_nsec += THR_CNT_SAMPLING_PERIOD_NS;
		} else {
			next_sample_time.tv_nsec += THR_CNT_SAMPLING_PERIOD_NS;
			next_sample_time.tv_nsec -= THR_CNT_1E9;
			next_sample_time.tv_sec += 1;
			passed_seconds++;
		}
		thr_cnt_threads_ar[thr_cnt_write_pos] = thr_cnt_get_nthreads();
#ifdef DEBUG
		if (thr_cnt_write_pos % 1000 == 0)
			cmn_err(CE_NOTE, "write_pos:%d, element:%d\n", thr_cnt_write_pos, thr_cnt_threads_ar[thr_cnt_write_pos]);
#endif
		thr_cnt_write_pos++;
		thr_cnt_write_pos %= THR_CNT_AR_SIZE;
		thr_cnt_pass_time_till(&next_sample_time);
	}
	cmn_err(CE_NOTE, "Recording finished in %d seconds\n", passed_seconds);
}
