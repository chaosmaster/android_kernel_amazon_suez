#include <asm/page.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>

#include <mach/mt_thermal.h>

#include "../../core/met_drv.h"
#include "../../core/trace.h"

#include "thermal.h"
#include "plf_trace.h"

/* define if the thermal sensor driver use its own timer to sampling the code , otherwise undefine it */
/* define it is better for sampling jitter if thermal sensor driver supports */
/* #define MET_USE_THERMALDRIVER_TIMER */

static unsigned int CheckAvailableThermalSensor(unsigned int a_u4DoCheck)
{
	static unsigned int u4AvailableSensor;

	unsigned int u4Index;

	if (!a_u4DoCheck)
		return u4AvailableSensor;

	/* Do check */
	if (MTK_THERMAL_SENSOR_COUNT > 32)
		return 0;

	u4AvailableSensor = 0;

	for (u4Index = 0; u4Index < MTK_THERMAL_SENSOR_COUNT; u4Index++) {
		if (mtk_thermal_get_temp(u4Index) == (-127000))
			u4AvailableSensor &= (~(1 << u4Index));
		else
			u4AvailableSensor |= (1 << u4Index);
	}

	return u4AvailableSensor;
}

static int do_thermal(void)
{
	static int do_thermal = -1;

	if (do_thermal != -1)
		return do_thermal;

	if (met_thermal.mode == 0)
		do_thermal = 0;
	else
		do_thermal = met_thermal.mode;

	return do_thermal;
}

static unsigned int get_thermal(unsigned int *value)
{
	int j = -1;
	int i;
	unsigned int u4ValidSensors = 0;

	u4ValidSensors = CheckAvailableThermalSensor(0);

	for (i = 0; i < MTK_THERMAL_SENSOR_COUNT; i++) {
		if (u4ValidSensors & (1 << i))
			value[++j] = mtk_thermal_get_temp(i);
	}

	return j + 1;
}

static void wq_get_thermal(struct work_struct *work)
{
	unsigned char count = 0;
	unsigned int thermal_value[MTK_THERMAL_SENSOR_COUNT];	/* Note here */

	int cpu;
	unsigned long long stamp;
	/* return; */
	cpu = smp_processor_id();
	if (do_thermal()) {
		stamp = cpu_clock(cpu);
		count = get_thermal(thermal_value);

		if (count)
			ms_th(stamp, count, thermal_value);
	}
}

#ifdef MET_USE_THERMALDRIVER_TIMER
static void thermal_start(void)
{
	CheckAvailableThermalSensor(1);
	mt_thermalsampler_registerCB(wq_get_thermal);

	return;
}

static void thermal_stop(void)
{
	mt_thermalsampler_registerCB(NULL);
	return;
}

#else

struct delayed_work dwork;
static void thermal_start(void)
{
	CheckAvailableThermalSensor(1);
/* printk("Thermal Sample:0x%x\n",CheckAvailableThermalSensor(0)); */
	INIT_DELAYED_WORK(&dwork, wq_get_thermal);

	return;
}

static void thermal_stop(void)
{
	cancel_delayed_work_sync(&dwork);
	return;
}

static void thermal_polling(unsigned long long stamp, int cpu)
{
	schedule_delayed_work(&dwork, 0);
}

#endif

static const char help[] = "  --thermal                             monitor thermal\n";
static int thermal_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static const char g_pThermalHeader[] = "met-info [000] 0.0: ms_ud_sys_header: ms_th,timestamp,";
/* "cpu,gpu,pmic,battery,d,d,d,d\n"; */
static int thermal_print_header(char *buf, int len)
{
	char buffer[256];
	unsigned long u4Cnt = 0;
	unsigned long u4Index = 0;
	unsigned int u4ValidSensor = 0;

	u4ValidSensor = CheckAvailableThermalSensor(0);

	strcpy(buffer, g_pThermalHeader);
	if ((1 << MTK_THERMAL_SENSOR_CPU) & u4ValidSensor) {
		strcat(buffer, "CPU,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_ABB) & u4ValidSensor) {
		strcat(buffer, "ABB,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_PMIC) & u4ValidSensor) {
		strcat(buffer, "PMIC,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_BATTERY) & u4ValidSensor) {
		strcat(buffer, "BATTERY,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_MD1) & u4ValidSensor) {
		strcat(buffer, "MD1,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_MD2) & u4ValidSensor) {
		strcat(buffer, "MD2,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_WIFI) & u4ValidSensor) {
		strcat(buffer, "WIFI,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_BATTERY2) & u4ValidSensor) {
		strcat(buffer, "BATTERY2,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_BUCK) & u4ValidSensor) {
		strcat(buffer, "BUCK,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_AP) & u4ValidSensor) {
		strcat(buffer, "AP,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_PCB1) & u4ValidSensor) {
		strcat(buffer, "PCB1,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_PCB2) & u4ValidSensor) {
		strcat(buffer, "PCB2,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_SKIN) & u4ValidSensor) {
		strcat(buffer, "SKIN,");
		u4Cnt += 1;
	}

	if ((1 << MTK_THERMAL_SENSOR_XTAL) & u4ValidSensor) {
		strcat(buffer, "XTAL,");
		u4Cnt += 1;
	}

	for (u4Index = 0; u4Index < u4Cnt; u4Index += 1) {
		strcat(buffer, "d");
		if ((u4Index + 1) != u4Cnt)
			strcat(buffer, ",");
	}

	strcat(buffer, "\n");
	return snprintf(buf, PAGE_SIZE, buffer);
}

/*
static int thermal_process_argument(const char *arg, int len)
{
	printk("Thermal Argument(l=%d):%s\n", len, arg);
	return 0;
}
*/

struct metdevice met_thermal = {
	.name = "thermal",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.start = thermal_start,
	.stop = thermal_stop,
#ifdef MET_USE_THERMALDRIVER_TIMER
#else
	.polling_interval = 50,	/* ms */
	.timed_polling = thermal_polling,
	.tagged_polling = thermal_polling,
#endif
	.print_help = thermal_print_help,
	.print_header = thermal_print_header,
/* .process_argument = thermal_process_argument */
};
