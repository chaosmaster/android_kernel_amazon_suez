#include <asm/page.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <mt-plat/mt_io.h>
/* #include <asm/system.h> */
#include <linux/hrtimer.h>

#include "core/met_drv.h"
#include "core/trace.h"

#include "mt_gpufreq.h"

#include "mt_gpu_metmonitor.h"
#include "plf_trace.h"


/* define if the hal implementation might re-schedule, cannot run inside softirq */
/* undefine this is better for sampling jitter if HAL support it */
#define GPU_HAL_RUN_PREMPTIBLE

#ifdef GPU_HAL_RUN_PREMPTIBLE
static struct delayed_work gpu_dwork;
static struct delayed_work gpu_pwr_dwork;
#endif

enum MET_GPU_PROFILE_INDEX {
	eMET_GPU_LOADING = 0,
	eMET_GPU_BLOCK,
	eMET_GPU_IDLE,
	eMET_GPU_GP_LOADING,
	eMET_GPU_PP_LOADING,
	eMET_GPU_PROFILE_CNT
};

static unsigned long g_u4AvailableInfo;

static noinline void GPULoading(unsigned char cnt, unsigned int *value)
{
	switch (cnt) {
	case 1:
		MET_PRINTK("%u\n", value[0]);
		break;

	case 2:
		MET_PRINTK("%u,%u\n", value[0], value[1]);
		break;

	case 3:
		MET_PRINTK("%u,%u,%u\n", value[0], value[1], value[2]);
		break;

	case 4:
		MET_PRINTK("%u,%u,%u,%u\n", value[0], value[1], value[2], value[3]);
		break;

	case 5:
		MET_PRINTK("%u,%u,%u,%u,%u\n", value[0], value[1], value[2], value[3], value[4]);
		break;

	case 6:
		MET_PRINTK("%u,%u,%u,%u,%u,%u\n", value[0], value[1], value[2], value[3], value[4],
			   value[5]);
		break;

	default:
		break;
	}
}

#ifdef GPU_HAL_RUN_PREMPTIBLE
static void GPULoading_OUTTER(struct work_struct *work)
{
	unsigned int pu4Value[eMET_GPU_PROFILE_CNT];
	unsigned long u4Index = 0;

	if ((1 << eMET_GPU_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_BLOCK) & g_u4AvailableInfo) {
		mtk_get_gpu_block(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_IDLE) & g_u4AvailableInfo) {
		mtk_get_gpu_idle(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_GP_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_PP_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if (g_u4AvailableInfo) {
		GPULoading(u4Index, pu4Value);
	}
}

#else

static void GPULoading_OUTTER(unsigned long long stamp, int cpu)
{
	unsigned int pu4Value[eMET_GPU_PROFILE_CNT];
	unsigned long u4Index = 0;

	if ((1 << eMET_GPU_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_BLOCK) & g_u4AvailableInfo) {
		mtk_get_gpu_block(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_IDLE) & g_u4AvailableInfo) {
		mtk_get_gpu_idle(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_GP_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if ((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo) {
		mtk_get_gpu_PP_loading(&pu4Value[u4Index]);
		u4Index += 1;
	}

	if (g_u4AvailableInfo) {
		GPULoading(u4Index, pu4Value);
	}
}
#endif

static void gpu_monitor_start(void)
{
	/* Check what information provided now */
	unsigned int u4Value = 0;
	if (0 == g_u4AvailableInfo) {
		if (mtk_get_gpu_loading(&u4Value))
			g_u4AvailableInfo |= (1 << eMET_GPU_LOADING);

		if (mtk_get_gpu_block(&u4Value))
			g_u4AvailableInfo |= (1 << eMET_GPU_BLOCK);

		if (mtk_get_gpu_idle(&u4Value))
			g_u4AvailableInfo |= (1 << eMET_GPU_IDLE);

		if (mtk_get_gpu_GP_loading(&u4Value))
			g_u4AvailableInfo |= (1 << eMET_GPU_GP_LOADING);

		if (mtk_get_gpu_PP_loading(&u4Value))
			g_u4AvailableInfo |= (1 << eMET_GPU_PP_LOADING);
	}
#ifdef GPU_HAL_RUN_PREMPTIBLE
	INIT_DELAYED_WORK(&gpu_dwork, GPULoading_OUTTER);
#endif

	return;
}

#ifdef GPU_HAL_RUN_PREMPTIBLE
static void gpu_monitor_stop(void)
{
	cancel_delayed_work_sync(&gpu_dwork);
}

static void GPULoadingNotify(unsigned long long stamp, int cpu)
{
	schedule_delayed_work(&gpu_dwork, 0);
}
#endif

static const char help[] = "  --gpu monitor                         monitor gpu status\n";
static int gpu_status_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static const char g_pComGPUStatusHeader[] =
"met-info [000] 0.0: ms_ud_sys_header: GPULoading,";

static int gpu_status_print_header(char *buf, int len)
{
	char buffer[1024];
	unsigned long u4Cnt = 0;
	unsigned long u4Index = 0;

	strncpy(buffer, g_pComGPUStatusHeader, 1024);
	if ((1 << eMET_GPU_LOADING) & g_u4AvailableInfo) {
		strncat(buffer, "Loading,", PAGE_SIZE);
		u4Cnt += 1;
	}

	if ((1 << eMET_GPU_BLOCK) & g_u4AvailableInfo) {
		strncat(buffer, "Block,", PAGE_SIZE);
		u4Cnt += 1;
	}

	if ((1 << eMET_GPU_IDLE) & g_u4AvailableInfo) {
		strncat(buffer, "Idle,", PAGE_SIZE);
		u4Cnt += 1;
	}

	if ((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo) {
		strncat(buffer, "GP Loading,", PAGE_SIZE);
		u4Cnt += 1;
	}

	if ((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo) {
		strncat(buffer, "PP Loading,", PAGE_SIZE);
		u4Cnt += 1;
	}

	for (u4Index = 0; u4Index < u4Cnt; u4Index += 1) {
		strncat(buffer, "d", PAGE_SIZE);
		if ((u4Index + 1) != u4Cnt)
			strncat(buffer, ",", PAGE_SIZE);
	}

	strncat(buffer, "\n", PAGE_SIZE);

	return snprintf(buf, PAGE_SIZE, buffer);
}

struct metdevice met_gpu = {
	.name = "gpu",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.start = gpu_monitor_start,
	.mode = 0,
	.polling_interval = 10,	/* ms */
#ifdef GPU_HAL_RUN_PREMPTIBLE
	.timed_polling = GPULoadingNotify,
	.stop = gpu_monitor_stop,
#else
	.timed_polling = GPULoading_OUTTER,
#endif
	.print_help = gpu_status_print_help,
	.print_header = gpu_status_print_header,
};

/* GPU power monitor */
static unsigned long g_u4PowerProfileIsOn;

static const char gpu_pwr_help[] = "  --gpu-pwr                             monitor gpu power status\n";
static int gpu_pwr_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, gpu_pwr_help);
}

#ifdef GPU_HAL_RUN_PREMPTIBLE
static noinline void GPU_Power(struct work_struct *work)
{
	unsigned int u4Value;

	mtk_get_gpu_power_loading(&u4Value);
	MET_PRINTK("%d\n", u4Value);
}

static void GPU_PowerNotify(unsigned long long stamp, int cpu)
{
	if (1 == g_u4PowerProfileIsOn) {
		schedule_delayed_work(&gpu_pwr_dwork, 0);
	}
}
#else
static noinline void GPU_Power(unsigned long long stamp, int cpu)
{
	unsigned int u4Value;

	if (1 == g_u4PowerProfileIsOn) {
		mtk_get_gpu_power_loading(&u4Value);
		MET_PRINTK("%d\n", u4Value);
	}
}
#endif

static void gpu_Power_monitor_start(void)
{
	/* Check what information provided now */
	unsigned int u4Value = 0;

	if (mtk_get_gpu_power_loading(&u4Value))
		g_u4PowerProfileIsOn = 1;

#ifdef GPU_HAL_RUN_PREMPTIBLE
	INIT_DELAYED_WORK(&gpu_pwr_dwork, GPU_Power);
#endif
}

static void gpu_Power_monitor_stop(void)
{
	g_u4PowerProfileIsOn = 0;

#ifdef GPU_HAL_RUN_PREMPTIBLE
	cancel_delayed_work_sync(&gpu_pwr_dwork);
#endif
}

static const char g_pComGPUPowerHeader[] =
"met-info [000] 0.0: ms_ud_sys_header: GPU_Power,GPU_Power,d\n";
static int gpu_Power_status_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, g_pComGPUPowerHeader);
}

struct metdevice met_gpupwr = {
	.name = "gpu-pwr",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.start = gpu_Power_monitor_start,
	.stop = gpu_Power_monitor_stop,
	.mode = 0,
	.polling_interval = 10,	/* ms */
#ifdef GPU_HAL_RUN_PREMPTIBLE
	.timed_polling = GPU_PowerNotify,
#else
	.timed_polling = GPU_Power,
#endif
	.print_help = gpu_pwr_print_help,
	.print_header = gpu_Power_status_print_header,
};

/*
 * FIXME : GPUDVFS() using a workaround to generate rectangular waveform.
 *	It generates double information. Fix it after frontend start to support.
 */
static unsigned int g_u4GPUFreq;
noinline void GPUDVFS(unsigned int a_u4Freq)
{
	MET_PRINTK("%u\n", g_u4GPUFreq);

	g_u4GPUFreq = a_u4Freq;

	MET_PRINTK("%u\n", a_u4Freq);
}

static void gpu_dvfs_monitor_start(void)
{
/*	g_u4GPUFreq = mt_gpufreq_get_cur_freq(); */
	mtk_get_gpu_freq(&g_u4GPUFreq);
	GPUDVFS(g_u4GPUFreq);
	mt_gpufreq_setfreq_registerCB(GPUDVFS);
}

static void gpu_dvfs_monitor_stop(void)
{
	mt_gpufreq_setfreq_registerCB(NULL);
	mtk_get_gpu_freq(&g_u4GPUFreq);
	GPUDVFS(g_u4GPUFreq);
/*	GPUDVFS(mt_gpufreq_get_cur_freq());	*/
}

static const char gpu_dvfs_help[] = "  --gpu_dvfs                            Measure GPU freq\n";
static int ptpod_gpudvfs_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, gpu_dvfs_help);
}

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static const char gpu_dvfs_header[] = "met-info [000] 0.0: ms_ud_sys_header: GPUDVFS,freq(kHz),d\n";
static int ptpod_gpudvfs_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, gpu_dvfs_header);
}

struct metdevice met_gpudvfs = {
	.name = "GPU-DVFS",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.start = gpu_dvfs_monitor_start,
	.stop = gpu_dvfs_monitor_stop,
	.print_help = ptpod_gpudvfs_print_help,
	.print_header = ptpod_gpudvfs_print_header,
};
EXPORT_SYMBOL(met_gpudvfs);
