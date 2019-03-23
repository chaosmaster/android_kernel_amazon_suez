/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/suspend.h>
#include <linux/thermal.h>
#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mt-plat/mt_boot.h>

#include "mt_charging.h"
#include "mt_battery_custom_data.h"
#include "mt_battery_common.h"
#include "mt_battery_meter.h"
#include <linux/irq.h>
#include <linux/reboot.h>

#if defined(CONFIG_AMAZON_METRICS_LOG)

#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/metricslog.h>

#include <linux/irq.h>
/* #include <mach/mt_pmic_irq.h> */
#include <linux/reboot.h>

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

extern signed int g_fg_dbg_bat_qmax;

enum BQ_FLAGS {
	BQ_STATUS_RESUMING = 0x2,
};

enum DOCK_STATE_TYPE {
	TYPE_DOCKED = 5,
	TYPE_UNDOCKED = 6,
};

struct battery_info {
	struct mutex lock;

	int flags;

	/* Time when system enters full suspend */
	struct timespec suspend_time;
	/* Time when system enters early suspend */
	struct timespec early_suspend_time;
	/* Battery capacity when system enters full suspend */
	int suspend_capacity;
	/* Battery capacity, relative and high-precision, when system enters full suspend */
	int suspend_bat_car;
	/* Battery capacity when system enters early suspend */
	int early_suspend_capacity;
#if defined(CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(CONFIG_FB)
	struct notifier_block notifier;
#endif
};

struct battery_info BQ_info;

/* set max time interval for metric to 10 years*/
#define MAX_TIME_INTERVAL       (3600*24*365*10)

struct metrics_charge {
	struct timespec charger_time;

	int battery_charge_ticks;
	unsigned int init_charging_vol;

	unsigned long battery_peak_virtual_temp;
	unsigned long battery_peak_battery_temp;
	unsigned long battery_average_virtual_temp;
	unsigned long battery_average_battery_temp;
	unsigned long usb_charger_voltage_peak;
};

static struct metrics_charge metrics_charge_info;

struct metrics_capacity {
	struct timespec above_95_time;
	struct timespec below_15_time;
	struct timespec charge_fault_start_time;

	bool bat_15_flag;
	bool bat_95_flag;
	bool bat_fault_flag;
	bool battery_below_15_fired;

	int low_battery_initial_soc;
	unsigned int low_battery_initial_voltage;
	unsigned int charger_fault_int_bat_vol;
	int device_was_plugged;
	unsigned int charger_fault_start_voltage;
	unsigned char charger_fault_type;
	unsigned char last_charger_fault_type;
	int battery_low_ticks;
};

extern void bq24297_get_fault_type(unsigned char *type);

extern unsigned long get_virtualsensor_temp(void);
#endif /* CONFIG_AMAZON_METRICS_LOG */

struct battery_common_data g_bat;

/* Battery Notify */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP
/* #define BATTERY_NOTIFY_CASE_0003_ICHARGING */
#define BATTERY_NOTIFY_CASE_0004_VBAT
#define BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME

/* Precise Tunning */
#define BATTERY_AVERAGE_DATA_NUMBER	3
#define BATTERY_AVERAGE_SIZE	30


/* ////////////////////////////////////////////////////////////////////////////// */
/* Battery Logging Entry */
/* ////////////////////////////////////////////////////////////////////////////// */
int Enable_BATDRV_LOG = BAT_LOG_ERROR;
char proc_bat_data[32];

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Smart Battery Structure */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
struct PMU_ChargerStruct BMT_status;


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Thermal related flags */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* 0:nothing, 1:enable batTT&chrTimer, 2:disable batTT&chrTimer, 3:enable batTT, disable chrTimer */
int g_battery_thermal_throttling_flag = 3;
int battery_cmd_thermal_test_mode = 0;
int battery_cmd_thermal_test_mode_value = 0;
int g_battery_tt_check_flag = 0;	/* 0:default enable check batteryTT, 1:default disable check batteryTT */


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Global Variable */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_common_id[] = {
	{.compatible = "mediatek,battery_common"},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_common_id);
#endif

struct wake_lock battery_suspend_lock;
unsigned int g_BatteryNotifyCode = 0x0000;
unsigned int g_BN_TestMode = 0x0000;
bool g_bat_init_flag = 0;
bool g_call_state = CALL_IDLE;
bool g_charging_full_reset_bat_meter = false;
int g_platform_boot_mode = 0;
static bool battery_meter_initilized;
bool g_cmd_hold_charging = false;
s32 g_custom_charging_current = -1;
signed int g_custom_charging_cv = -1;
signed int g_custom_fake_full = -1;
unsigned int g_custom_charging_mode = 0; /* 0=ratail unit, 1=demo unit */
static unsigned long g_custom_plugin_time;
bool battery_suspended = false;
bool g_refresh_ui_soc = false;
static bool fg_battery_shutdown;
struct mt_battery_charging_custom_data *p_bat_charging_data;

struct timespec chr_plug_in_time;
#define PLUGIN_THRESHOLD (7*86400)

static struct mt_battery_charging_custom_data default_charging_data = {

	.talking_recharge_voltage = 3800,
	.talking_sync_time = 60,

	/* Battery Temperature Protection */
	.max_discharge_temperature = 60,
	.min_discharge_temperature = -10,
	.max_charge_temperature = 50,
	.min_charge_temperature = 0,
	.err_charge_temperature = 0xFF,
	.use_avg_temperature = 1,

	/* Linear Charging Threshold */
	.v_pre2cc_thres = 3400,	/* mV */
	.v_cc2topoff_thres = 4050,
	.recharging_voltage = 4110,
	.charging_full_current = 150,	/* mA */

	/* CONFIG_USB_IF */
	.usb_charger_current_suspend = 0,
	.usb_charger_current_unconfigured = CHARGE_CURRENT_70_00_MA,
	.usb_charger_current_configured = CHARGE_CURRENT_500_00_MA,

	.usb_charger_current = CHARGE_CURRENT_500_00_MA,
	.ac_charger_current = 204800,
	.ac_charger_input_current = 180000,
	.non_std_ac_charger_current = CHARGE_CURRENT_500_00_MA,
	.charging_host_charger_current = CHARGE_CURRENT_650_00_MA,
	.apple_0_5a_charger_current = CHARGE_CURRENT_500_00_MA,
	.apple_1_0a_charger_current = CHARGE_CURRENT_1000_00_MA,
	.apple_2_1a_charger_current = CHARGE_CURRENT_2000_00_MA,

	/* Charger error check */
	/* BAT_LOW_TEMP_PROTECT_ENABLE */
	.v_charger_enable = 1,	/* 1:ON , 0:OFF */
	.v_charger_max = 6500,	/* 6.5 V */
	.v_charger_min = 4400,	/* 4.4 V */
	.battery_cv_voltage = BATTERY_VOLT_04_200000_V,

	/* Tracking time */
	.onehundred_percent_tracking_time = 10,	/* 10 second */
	.npercent_tracking_time = 20,	/* 20 second */
	.sync_to_real_tracking_time = 30,	/* 30 second */

	/* JEITA parameter */
	.cust_soc_jeita_sync_time = 60,
	.jeita_recharge_voltage = 4110,	/* for linear charging */
	.jeita_temp_above_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_pos_45_to_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_pos_10_to_pos_45_cv_voltage = BATTERY_VOLT_04_200000_V,
	.jeita_temp_pos_0_to_pos_10_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_neg_10_to_pos_0_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_below_neg_10_cv_voltage = BATTERY_VOLT_04_000000_V,
	.temp_pos_60_threshold = 50,
	.temp_pos_60_thres_minus_x_degree = 47,
	.temp_pos_45_threshold = 45,
	.temp_pos_45_thres_minus_x_degree = 39,
	.temp_pos_10_threshold = 10,
	.temp_pos_10_thres_plus_x_degree = 16,
	.temp_pos_0_threshold = 0,
	.temp_pos_0_thres_plus_x_degree = 6,
	.temp_neg_10_threshold = 0,
	.temp_neg_10_thres_plus_x_degree = 0,

	/* For JEITA Linear Charging Only */
	.jeita_neg_10_to_pos_0_full_current = 120,	/* mA */
	.jeita_temp_pos_45_to_pos_60_recharge_voltage = 4000,
	.jeita_temp_pos_10_to_pos_45_recharge_voltage = 4100,
	.jeita_temp_pos_0_to_pos_10_recharge_voltage = 4000,
	.jeita_temp_neg_10_to_pos_0_recharge_voltage = 3800,
	.jeita_temp_pos_45_to_pos_60_cc2topoff_threshold = 4050,
	.jeita_temp_pos_10_to_pos_45_cc2topoff_threshold = 4050,
	.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold = 4050,
	.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold = 3850,

	/* For Pump Express Plus */
	.ta_start_battery_soc = 1,
	.ta_stop_battery_soc = 95,
	.ta_ac_9v_input_current = CHARGE_CURRENT_1500_00_MA,
	.ta_ac_7v_input_current = CHARGE_CURRENT_1500_00_MA,
	.ta_ac_charging_current = CHARGE_CURRENT_2200_00_MA,
	.ta_9v_support = 1,
};

/* ////////////////////////////////////////////////////////////////////////////// */
/* Integrate with NVRAM */
/* ////////////////////////////////////////////////////////////////////////////// */
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
/* add for meta tool----------------------------------------- */

static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

int adc_cali_slop[14] = {
	1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000
};
int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int adc_cali_cal[1] = { 0 };
int battery_in_data[1] = { 0 };
int battery_out_data[1] = { 0 };
int charging_level_data[1] = { 0 };

bool g_ADC_Cali = false;
bool g_ftm_battery_flag = false;
static bool need_clear_current_window;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Thread related */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static atomic_t bat_thread_wakeup;
static bool chr_wake_up_bat;	/* charger in/out to wake up battery thread */
static bool bat_meter_timeout;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(usb_ready_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);

#if defined(CONFIG_AMAZON_METRICS_LOG)

#ifndef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
signed int gFG_aging_factor = 0;
signed int gFG_battery_cycle = 0;
signed int gFG_columb_sum = 0;
#else
extern signed int gFG_aging_factor;
extern signed int gFG_battery_cycle;
extern signed int gFG_columb_sum;
#endif

void metrics_charger(bool connect)
{
	int cap = BMT_status.UI_SOC;
	char buf[128];

	if (connect == true) {
		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_CHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	} else {
		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_DISCHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
}

void metrics_charger_update(int ac, int usb)
{
	static bool ischargeronline;
	int onceonline = 0;

	if (ac == 1)
		onceonline = 1;
	else if (usb == 1)
		onceonline = 1;
	if (ischargeronline != onceonline) {
		ischargeronline = onceonline;
		metrics_charger(ischargeronline);
	}
}
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
static void
battery_critical_voltage_check(void)
{
	static bool written;

	if (!BMT_status.charger_exist)
		return;
	if (BMT_status.bat_exist != true)
		return;
	if (BMT_status.UI_SOC != 0)
		return;
	if (written == false && BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE) {
		written = true;

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
		life_cycle_set_special_mode(LIFE_CYCLE_SMODE_LOW_BATTERY);
#endif
	}
}

static void
battery_metrics_locked(struct battery_info *info);

static void battery_capacity_check(void)
{
	char buf[256];
	long elaps_sec;
	struct timespec diff;
	struct timespec curr;
	static struct metrics_capacity metrics_cap_info;

	if (BMT_status.SOC > 95 && !metrics_cap_info.bat_95_flag) {
		metrics_cap_info.bat_95_flag = true;
		metrics_cap_info.above_95_time = current_kernel_time();
	}

	if (BMT_status.SOC < 95 && metrics_cap_info.bat_95_flag) {
		metrics_cap_info.bat_95_flag = false;
		curr = current_kernel_time();
		diff = timespec_sub(curr, metrics_cap_info.above_95_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec / NSEC_PER_SEC;
		if (elaps_sec > 0 && elaps_sec < MAX_TIME_INTERVAL) {
			snprintf(buf, sizeof(buf),
				"bq24297:def:time_soc95=1;CT;1,"
				"Elaps_Sec=%ld;CT;1:NA", elaps_sec);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	if (BMT_status.SOC < 15 && !metrics_cap_info.bat_15_flag) {
		metrics_cap_info.bat_15_flag = true;
		metrics_cap_info.below_15_time = current_kernel_time();
		metrics_cap_info.battery_low_ticks = 0;
		metrics_cap_info.low_battery_initial_voltage = BMT_status.bat_vol;
		metrics_cap_info.low_battery_initial_soc = BMT_status.SOC;
	} else if (BMT_status.SOC < 15) {
		/* Count Tickes for case when system
		* starts up from dead battery
		*/
		metrics_cap_info.battery_low_ticks++;
	}

	if (BMT_status.SOC > 15 && metrics_cap_info.bat_15_flag) {
		metrics_cap_info.bat_15_flag = false;
		metrics_cap_info.battery_below_15_fired = false;
		curr = current_kernel_time();
		diff = timespec_sub(curr, metrics_cap_info.below_15_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec / NSEC_PER_SEC;

		/* If system clock changed drastically use ticks instead.
		* If not clock is probably more accurate
		*/
		if (elaps_sec > metrics_cap_info.battery_low_ticks * 20)
			elaps_sec = metrics_cap_info.battery_low_ticks * 10;

		if (elaps_sec > 0 && elaps_sec < MAX_TIME_INTERVAL) {
			snprintf(buf, sizeof(buf),
				"bq24297:def:time_soc15_soc20=1;CT;1,"
				"Init_Vol=%u;CT;1,Init_SOC=%d;CT;1,"
				"Elaps_Sec=%ld;CT;1:NA",
				metrics_cap_info.low_battery_initial_voltage,
				metrics_cap_info.low_battery_initial_soc, elaps_sec);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	/* Catch Events for devices that may power off */
	if (BMT_status.SOC <= 2 && metrics_cap_info.battery_below_15_fired == false) {
		metrics_cap_info.battery_below_15_fired = true;
		diff = timespec_sub(current_kernel_time(), metrics_cap_info.below_15_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;
		if (elaps_sec > 0 && elaps_sec < MAX_TIME_INTERVAL) {
			snprintf(buf, sizeof(buf),
				"bq24297:def:time_soc15_soc0=1;CT;1,"
				"Init_Vol=%u;CT;1,Init_SOC=%d;CT;1,"
				"Elaps_Sec=%ld;CT;1:NA",
				metrics_cap_info.low_battery_initial_voltage,
				metrics_cap_info.low_battery_initial_soc, elaps_sec);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	/* Check charger fault */
	bq24297_get_fault_type(&metrics_cap_info.charger_fault_type);
	if (metrics_cap_info.charger_fault_type && !metrics_cap_info.bat_fault_flag) {
		metrics_cap_info.bat_fault_flag = true;
		metrics_cap_info.last_charger_fault_type = metrics_cap_info.charger_fault_type;
		metrics_cap_info.charge_fault_start_time = current_kernel_time();
		metrics_cap_info.charger_fault_start_voltage = BMT_status.charger_vol;
		metrics_cap_info.device_was_plugged = BMT_status.charger_exist;
		metrics_cap_info.charger_fault_int_bat_vol = BMT_status.bat_vol;
	}

	if (metrics_cap_info.last_charger_fault_type != metrics_cap_info.charger_fault_type
			&& metrics_cap_info.bat_fault_flag) {
		metrics_cap_info.bat_fault_flag = false;
		diff = timespec_sub(current_kernel_time(),
				metrics_cap_info.charge_fault_start_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec / NSEC_PER_SEC;

		snprintf(buf, sizeof(buf), "bq24297:def:"
			"charger_fault_type=%d;CT;1,Secs=%ld;CT;1,"
			"bVoltI=%u;CT;1,bVoltF=%u;CT;1,cVoltI=%d;CT;1,"
			"cVoltF=%d;CT;1,IsPlugged=%d;CT;1,"
			"WasPlugged=%d;CT;1:NA", metrics_cap_info.last_charger_fault_type,
			elaps_sec, metrics_cap_info.charger_fault_int_bat_vol,
			BMT_status.bat_vol, metrics_cap_info.charger_fault_start_voltage,
			BMT_status.charger_vol, BMT_status.charger_exist,
			metrics_cap_info.device_was_plugged);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
}

static void metrics_handle(void)
{
	struct battery_info *info = &BQ_info;

	mutex_lock(&info->lock);

	/* Check for critical battery voltage */
	battery_critical_voltage_check();


	if ((info->flags & BQ_STATUS_RESUMING)) {
		info->flags &= ~BQ_STATUS_RESUMING;
		battery_metrics_locked(info);
	}

	mutex_unlock(&info->lock);

	battery_capacity_check();
	return;
}

#if defined(CONFIG_EARLYSUSPEND) || defined(CONFIG_FB)
static void bq_log_metrics(struct battery_info *info, char *msg,
	char *metricsmsg)
{
	int value = BMT_status.UI_SOC;
	struct timespec curr = current_kernel_time();
	/* Compute elapsed time and determine screen off or on drainage */
	struct timespec diff = timespec_sub(curr,
			info->early_suspend_time);
	if (diff.tv_sec > 0 && diff.tv_sec < MAX_TIME_INTERVAL) {
		if (info->early_suspend_capacity != -1) {
			char buf[512];
			snprintf(buf, sizeof(buf),
				"%s:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
				metricsmsg,
				info->early_suspend_capacity - value,
				diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
			log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
		}
	}
	/* Cache the current capacity */
	info->early_suspend_capacity = BMT_status.UI_SOC;
	/* Mark the suspend or resume time */
	info->early_suspend_time = curr;
}
#endif

#if defined(CONFIG_EARLYSUSPEND)
static void battery_early_suspend(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;

	bq_log_metrics(info, "Screen on drainage", "screen_on_drain");

}

static void battery_late_resume(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;
	bq_log_metrics(info, "Screen off drainage", "screen_off_drain");
}
#endif

#if defined(CONFIG_FB)
/* frame buffer notifier block control the suspend/resume procedure */
static int batt_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;
	struct battery_info *info = &BQ_info;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			bq_log_metrics(info, "Screen off drainage", "screen_off_drain");

		} else if (*blank == FB_BLANK_POWERDOWN) {
			bq_log_metrics(info, "Screen on drainage", "screen_on_drain");
		}
	}

	return 0;
}
#endif

static int metrics_init(void)
{
	struct battery_info *info = &BQ_info;

	mutex_init(&info->lock);

	info->suspend_capacity = -1;
	info->early_suspend_capacity = -1;

#if defined(CONFIG_EARLYSUSPEND)
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->early_suspend.suspend = battery_early_suspend;
	info->early_suspend.resume = battery_late_resume;
	register_early_suspend(&info->early_suspend);
#endif
#if defined(CONFIG_FB)
	info->notifier.notifier_call = batt_fb_notifier_callback;
	fb_register_client(&info->notifier);
#endif
	mutex_lock(&info->lock);
	info->flags = 0;
	mutex_unlock(&info->lock);

	return 0;
}

static void metrics_uninit(void)
{
#if defined(CONFIG_EARLYSUSPEND)
	struct battery_info *info = &BQ_info;
	unregister_early_suspend(&info->early_suspend);
#endif
#if defined(CONFIG_FB)
	struct battery_info *info = &BQ_info;
	fb_unregister_client(&info->notifier);
#endif
}

static void metrics_suspend(void)
{
	struct battery_info *info = &BQ_info;

	/* Cache the current capacity */
	info->suspend_capacity = BMT_status.UI_SOC;
	info->suspend_bat_car = battery_meter_get_car();

	pr_info("%s: setting suspend_bat_car to %d\n",
			__func__, info->suspend_bat_car);

	battery_critical_voltage_check();

	/* Mark the suspend time */
	info->suspend_time = current_kernel_time();
}

#endif /* CONFIG_AMAZON_METRICS_LOG */

/* ////////////////////////////////////////////////////////////////////////////// */
/* FOR ANDROID BATTERY SERVICE */
/* ////////////////////////////////////////////////////////////////////////////// */
struct ac_data {
	struct power_supply psy;
	int AC_ONLINE;
};

struct usb_data {
	struct power_supply psy;
	int USB_ONLINE;
};

struct battery_data {
	struct power_supply psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
	/* Amazon Dock */
	struct switch_dev dock_state;
	/* Add for Battery Service */
	int BAT_VOLTAGE_NOW;
	int BAT_VOLTAGE_AVG;
	int BAT_TEMP;
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;
	struct mtk_cooler_platform_data *cool_dev;
#ifdef CONFIG_AMAZON_METRICS_LOG
	int old_CAR;    /* as read from hardware */
	int BAT_ChargeCounter;   /* monotonically declining */
	int BAT_ChargeFull;
	int BAT_SuspendDrain;
	int BAT_SuspendDrainHigh;
	int BAT_SuspendRealtime;
#endif
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_DOCK_PRESENT,
	/* Add for EM
	   POWER_SUPPLY_PROP_TemperatureR,
	   POWER_SUPPLY_PROP_TempBattVoltage,
	   POWER_SUPPLY_PROP_InstatVolt,
	   POWER_SUPPLY_PROP_BatteryAverageCurrent,
	   POWER_SUPPLY_PROP_BatterySenseVoltage,
	   POWER_SUPPLY_PROP_ISenseVoltage,
	   POWER_SUPPLY_PROP_ChargerVoltage,
	 */
};

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC PCHR Related APIs */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
__attribute__ ((weak)) bool mt_usb_pd_support(void) { return false; }
__attribute__ ((weak)) bool mt_is_power_sink(void) { return true; }


bool upmu_is_chr_det(void)
{
#if defined(CONFIG_POWER_EXT)
	/* return true; */
	return bat_charger_get_detect_status();
#else
	u32 tmp32;

	tmp32 = bat_charger_get_detect_status();

#if defined(CONFIG_ANALOGIX_OHIO)
	if (tmp32 == 0 && !(battery_meter_get_charger_voltage() >= 4300))
		return false;
#else
	if (tmp32 == 0)
		return false;
#endif

	if (mt_usb_pd_support()) {

		battery_log(BAT_LOG_FULL, "[upmu_is_chr_det] usb device mode(%d). power role(%d)\n",
			mt_usb_is_device(), mt_is_power_sink());
		if (mt_is_power_sink())
			return true;
		else
			return false;
	} else if (mt_usb_is_device()) {
		battery_log(BAT_LOG_FULL, "[upmu_is_chr_det] Charger exist and USB is not host\n");

		return true;
	}

	battery_log(BAT_LOG_FULL, "[upmu_is_chr_det] Charger exist but USB is host\n");
	return false;

#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);

static inline void _do_wake_up_bat_thread(void)
{
	atomic_inc(&bat_thread_wakeup);
	wake_up(&bat_thread_wq);
}

/* for charger plug-in/out */
void wake_up_bat(void)
{
	pr_debug("%s:\n", __func__);
	chr_wake_up_bat = true;
	_do_wake_up_bat_thread();
}
EXPORT_SYMBOL(wake_up_bat);

/* for meter update */
static void wake_up_bat_update_meter(void)
{
	pr_debug("%s:\n", __func__);
	bat_meter_timeout = true;
	_do_wake_up_bat_thread();
}

static ssize_t bat_log_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	if (copy_from_user(&proc_bat_data, buff, len)) {
		battery_log(BAT_LOG_FULL, "bat_log_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data[0] == '1') {
		battery_log(BAT_LOG_CRTI, "enable battery driver log system\n");
		Enable_BATDRV_LOG = 1;
	} else if (proc_bat_data[0] == '2') {
		battery_log(BAT_LOG_CRTI, "enable battery driver log system:2\n");
		Enable_BATDRV_LOG = 2;
	} else {
		battery_log(BAT_LOG_CRTI, "Disable battery driver log system\n");
		Enable_BATDRV_LOG = 0;
	}

	return len;
}

static const struct file_operations bat_proc_fops = {
	.write = bat_log_write,
};

int init_proc_log(void)
{
	int ret = 0;

	proc_create("batdrv_log", 0644, NULL, &bat_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create bat_proc_fops\n");

	return ret;
}



static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy, struct ac_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy, struct usb_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_POWER_EXT)
		/* #if 0 */
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
#else
		val->intval = data->USB_ONLINE;
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int battery_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int battery_get_property(struct power_supply *psy,
				enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data = container_of(psy, struct battery_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_VOLTAGE_NOW;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->BAT_TEMP;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = data->BAT_VOLTAGE_AVG;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (data->cool_dev)
			val->intval = data->cool_dev->state;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (data->cool_dev)
			val->intval = data->cool_dev->max_state;
		break;
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		val->intval = (switch_get_state(&data->dock_state)
			== TYPE_UNDOCKED) ? 0 : 1;
		break;
/*
	case POWER_SUPPLY_PROP_TemperatureR:
		val->intval = data->BAT_TemperatureR;
		break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
		val->intval = data->BAT_TempBattVoltage;
		break;
	case POWER_SUPPLY_PROP_InstatVolt:
		val->intval = data->BAT_InstatVolt;
		break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
		val->intval = data->BAT_BatteryAverageCurrent;
		break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
		val->intval = data->BAT_BatterySenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
		val->intval = data->BAT_ISenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
		val->intval = data->BAT_ChargerVoltage;
		break;
*/
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int battery_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int level, state = 0;
	unsigned long max_state;
	struct battery_data *data = container_of(psy, struct battery_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (!data->cool_dev)
			return 0;
		if (data->cool_dev->state == val->intval)
			return 0;
		if (!data->cool_dev->state)
			data->cool_dev->level = get_bat_charging_current_limit() * 100;

		max_state = data->cool_dev->max_state;
		state = (val->intval > max_state) ? max_state : val->intval;
		data->cool_dev->state = state;
		if (!data->cool_dev->state)
			level = data->cool_dev->level;
		else {
			level = data->cool_dev->levels[data->cool_dev->state - 1];
			if (level > data->cool_dev->level)
				return 0;
		}
		set_bat_charging_current_limit(level/100);
		break;
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		state = val->intval == 0 ? TYPE_UNDOCKED : TYPE_DOCKED;
		pr_debug("%s: report dock state: %d\n", __func__, state);
		switch_set_state(&data->dock_state, state);
		break;
	default:
		break;
	}
	return 0;
}

static struct mtk_cooler_platform_data cooler = {
	.type = "battery",
	.state = 0,
	.max_state = THERMAL_MAX_TRIPS,
	.level = 0,
	.levels = {
		CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1000_00_MA,
		CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1000_00_MA,
		CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1000_00_MA,
		CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_250_00_MA,
		CHARGE_CURRENT_250_00_MA, CHARGE_CURRENT_250_00_MA,
		CHARGE_CURRENT_250_00_MA, CHARGE_CURRENT_250_00_MA,
	},
};

/* ac_data initialization */
static struct ac_data ac_main = {
	.psy = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psy = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psy = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		.set_property = battery_set_property,
		.property_is_writeable = battery_property_is_writeable,
		},
/* CC: modify to have a full power supply status */
#if defined(CONFIG_POWER_EXT)
	.BAT_STATUS = POWER_SUPPLY_STATUS_FULL,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 100,
	.BAT_VOLTAGE_NOW = 4200000,
	.BAT_VOLTAGE_AVG = 4200000,
	.BAT_TEMP = 22,
#else
	.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	.BAT_CAPACITY = -1,
#else
	.BAT_CAPACITY = 50,
#endif
	.BAT_VOLTAGE_NOW = 0,
	.BAT_VOLTAGE_AVG = 0,
	.BAT_TEMP = 0,
#endif
};

#if defined(CONFIG_AMAZON_METRICS_LOG)

/* must be called with info->lock held */
static void
battery_metrics_locked(struct battery_info *info)
{
	struct timespec diff;

	diff = timespec_sub(current_kernel_time(), info->suspend_time);
	if (diff.tv_sec > 0 && diff.tv_sec < MAX_TIME_INTERVAL) {
		if (info->suspend_capacity != -1) {
			char buf[256];
			int drain_diff;
			int drain_diff_high;

			snprintf(buf, sizeof(buf),
				"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
				info->suspend_capacity - BMT_status.UI_SOC,
				diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
			log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);


			snprintf(buf, sizeof(buf),
				"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1,"
				"temp_g=%d;CT;1,charge=%d;CT;1,charge_design=%d;CT;1,"
				"aging_factor=%d;CT;1,battery_cycle=%d;CT;1,"
				"columb_sum=%d;CT;1:NR",
				BMT_status.UI_SOC, BMT_status.bat_vol,
				BMT_status.ICharging, BMT_status.temperature,
				gFG_BATT_CAPACITY_aging, /*battery_remaining_charge,?*/
				gFG_BATT_CAPACITY, /*battery_remaining_charge_design*/
				gFG_aging_factor, /* aging factor */
				gFG_battery_cycle,
				gFG_columb_sum
				);
			log_to_metrics(ANDROID_LOG_INFO, "bq24297", buf);

			/* These deltas may not always be positive.
			 * BMT_status.UI_SOC may be stale by as much as 10 seconds.
			 */
			drain_diff = info->suspend_capacity - BMT_status.UI_SOC;
			drain_diff_high = (info->suspend_bat_car - battery_meter_get_car())
					 * 10000 / g_fg_dbg_bat_qmax;
			if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_DISCHARGING) {
				battery_main.BAT_SuspendDrain += drain_diff;
				battery_main.BAT_SuspendDrainHigh += drain_diff_high;
				battery_main.BAT_SuspendRealtime += diff.tv_sec;

				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf),
					"bq24297:def:drain_diff_high=%d;CT;1,"
					"BAT_SuspendDrainHigh=%d;CT;1:NR",
					drain_diff_high, battery_main.BAT_SuspendDrainHigh);

				log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			}
		}
	}
}
#endif /* CONFIG_AMAZON_METRICS_LOG */

#if !defined(CONFIG_POWER_EXT)
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Charger_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	battery_log(BAT_LOG_CRTI, "[EM] show_ADC_Charger_Voltage : %d\n", BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664, show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664, show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664, show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664, show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664, show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664, show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664, show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664, show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664, show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664, show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664, show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope,
		   store_ADC_Channel_10_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope,
		   store_ADC_Channel_11_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope,
		   store_ADC_Channel_12_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope,
		   store_ADC_Channel_13_Slope);


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset,
		   store_ADC_Channel_0_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset,
		   store_ADC_Channel_1_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset,
		   store_ADC_Channel_2_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset,
		   store_ADC_Channel_3_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset,
		   store_ADC_Channel_4_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset,
		   store_ADC_Channel_5_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset,
		   store_ADC_Channel_6_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset,
		   store_ADC_Channel_7_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset,
		   store_ADC_Channel_8_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset,
		   store_ADC_Channel_9_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Offset : %d\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset,
		   store_ADC_Channel_10_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Offset : %d\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset,
		   store_ADC_Channel_11_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset,
		   store_ADC_Channel_12_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset,
		   store_ADC_Channel_13_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_Is_Calibration */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_Is_Calibration : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664, show_ADC_Channel_Is_Calibration,
		   store_ADC_Channel_Is_Calibration);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_On_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_On_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664, show_Power_Off_Voltage, store_Power_Off_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Charger_TopOff_Value */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	battery_log(BAT_LOG_CRTI, "[EM] Charger_TopOff_Value : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value,
		   store_Charger_TopOff_Value);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Charger_Type */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Charger_Type(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = CHARGER_UNKNOWN;

	ret_value = BMT_status.charger_type;
	battery_log(BAT_LOG_CRTI, "[EM] Charger_Type : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_Type(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_Type, 0664, show_Charger_Type,
		   store_Charger_Type);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_Battery_CurrentConsumption */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	battery_log(BAT_LOG_CRTI, "[EM] FG_Battery_CurrentConsumption : %d/10 mA\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_SW_CoulombCounter */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	s32 ret_value = 7777;

	ret_value = battery_meter_get_car();
	battery_log(BAT_LOG_CRTI, "[EM] FG_SW_CoulombCounter : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664, show_FG_SW_CoulombCounter,
		   store_FG_SW_CoulombCounter);


static ssize_t show_Charging_CallState(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret, call_state;

	ret = kstrtoint(buf, 0, &call_state);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	g_call_state = (call_state ? true : false);
	battery_log(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664, show_Charging_CallState, store_Charging_CallState);

static ssize_t show_Charging_Enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "hold charging = %d\n", g_cmd_hold_charging);
	return sprintf(buf, "%u\n", !g_cmd_hold_charging);
}

static ssize_t store_Charging_Enable(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int ret, charging_enable = 1;

	ret = kstrtoint(buf, 0, &charging_enable);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (charging_enable == 1)
		g_cmd_hold_charging = false;
	else if (charging_enable == 0)
		g_cmd_hold_charging = true;
	wake_up_bat_update_meter();
	battery_log(BAT_LOG_CRTI, "hold charging = %d\n", g_cmd_hold_charging);
	return size;
}

static DEVICE_ATTR(Charging_Enable, 0664, show_Charging_Enable, store_Charging_Enable);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static ssize_t show_Pump_Express(struct device *dev, struct device_attribute *attr, char *buf)
{
	int icount = 20;	/* max debouncing time 20 * 0.2 sec */

	if (true == ta_check_chr_type && STANDARD_CHARGER == BMT_status.charger_type &&
	    BMT_status.SOC >= p_bat_charging_data->ta_start_battery_soc &&
	    BMT_status.SOC < p_bat_charging_data->ta_stop_battery_soc) {
		battery_log(BAT_LOG_CRTI, "[%s]Wait for PE detection\n", __func__);
		do {
			icount--;
			msleep(200);
		} while (icount && ta_check_chr_type);
	}

	battery_log(BAT_LOG_CRTI, "Pump express = %d\n", is_ta_connect);
	return sprintf(buf, "%u\n", is_ta_connect);
}

static ssize_t store_Pump_Express(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int rv;
	u32 value;

	rv = kstrtouint(buf, 0, &value);
	if (rv != 1)
		return -EINVAL;
	is_ta_connect = (value != 0) ? true : false;
	battery_log(BAT_LOG_CRTI, "Pump express= %d\n", is_ta_connect);
	return size;
}

static DEVICE_ATTR(Pump_Express, 0664, show_Pump_Express, store_Pump_Express);
#endif

static ssize_t show_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	battery_log(BAT_LOG_CRTI, "custom charging current = %d\n", g_custom_charging_current);
	return sprintf(buf, "%d\n", g_custom_charging_current);
}

static ssize_t store_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	int ret, cur;

	ret = kstrtoint(buf, 0, &cur);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	g_custom_charging_current = cur;
	battery_log(BAT_LOG_CRTI, "custom charging current = %d\n", g_custom_charging_current);
	wake_up_bat_update_meter();
	return size;
}

static DEVICE_ATTR(Custom_Charging_Current, 0664, show_Custom_Charging_Current,
		   store_Custom_Charging_Current);

static ssize_t show_Custom_PlugIn_Time(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("custom plugin_time = %lu\n", g_custom_plugin_time);
	return sprintf(buf, "0");
}

static ssize_t store_Custom_PlugIn_Time(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	ret = kstrtoul(buf, 0, &g_custom_plugin_time);
	pr_notice("custom plugin_time = %lu\n", g_custom_plugin_time);
	if (g_custom_plugin_time > PLUGIN_THRESHOLD)
		g_custom_plugin_time = PLUGIN_THRESHOLD;

	wake_up_bat();
	return size;
}

static DEVICE_ATTR(Custom_PlugIn_Time, 0664, show_Custom_PlugIn_Time,
		   store_Custom_PlugIn_Time);

static ssize_t show_recharge_counter(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BMT_status.recharge_cnt);
}

static DEVICE_ATTR(recharge_counter, 0444, show_recharge_counter, NULL);

static ssize_t show_charger_plugin_counter(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BMT_status.charger_plugin_cnt);
}

static DEVICE_ATTR(charger_plugin_counter, 0444, show_charger_plugin_counter, NULL);


static ssize_t show_Custom_Charging_Mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("Charging mode = %u\n", g_custom_charging_mode);
	return sprintf(buf, "%u\n", g_custom_charging_mode);
}

static ssize_t store_Custom_Charging_Mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	ret = kstrtouint(buf, 0, &g_custom_charging_mode);
	pr_notice("Charging mode= %u\n", g_custom_charging_mode);

	return size;
}

static DEVICE_ATTR(Custom_Charging_Mode, 0660, show_Custom_Charging_Mode,
		   store_Custom_Charging_Mode);


static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;	/* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;	/* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;	/* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;	/* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;

	if ((BMT_status.UI_SOC == 100) && BMT_status.charger_exist)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	battery_log(BAT_LOG_CRTI,
		    "BAT_CAPACITY=1, due to define MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}

#ifdef CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT
#define LEAVE_TOP_OFF_THRES 70
static void reset_plug_in_timer(void)
{
	getrawmonotonic(&chr_plug_in_time);
	/* 12 days */
	g_custom_plugin_time = 12*86400;
	g_custom_charging_cv = -1;
}

static bool check_top_off_state(void)
{
	static bool top_off_flag;

	if (BMT_status.charger_exist && g_custom_charging_cv == BATTERY_VOLT_04_096000_V) {
		if (BMT_status.UI_SOC == 100)
			top_off_flag = true;

		if (top_off_flag && (BMT_status.SOC < LEAVE_TOP_OFF_THRES)) {
			top_off_flag = false;
			g_custom_fake_full = -1;
			reset_plug_in_timer();
		}

		pr_notice("[%s] %d, %d, %d, %lu, %d\r\n", __func__, BMT_status.UI_SOC,
			BMT_status.SOC, top_off_flag, g_custom_plugin_time, g_custom_fake_full);
	} else
		top_off_flag = false;

	return top_off_flag;
}
#endif

static bool mt_battery_100Percent_tracking_check(void)
{
	bool resetBatteryMeter = false;
	u32 cust_sync_time;
	static u32 timer_counter;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	cust_sync_time = p_bat_charging_data->cust_soc_jeita_sync_time;
	if (timer_counter == 0)
		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
#else
	cust_sync_time = p_bat_charging_data->onehundred_percent_tracking_time;
	if (timer_counter == 0)
		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (g_temp_status != TEMP_POS_10_TO_POS_45 && g_temp_status != TEMP_POS_0_TO_POS_10) {
		battery_log(BAT_LOG_FULL,
			    "Skip 100percent tracking due to not 4.2V full-charging.\n");
		return false;
	}
#endif

	if (BMT_status.bat_full == true) {	/* charging full first, UI tracking to 100% */

		if (BMT_status.bat_in_recharging_state == true) {
			if (BMT_status.UI_SOC >= 100)
				BMT_status.UI_SOC = 100;
			resetBatteryMeter = false;
		} else if (BMT_status.UI_SOC >= 100) {
			BMT_status.UI_SOC = 100;

			if ((g_charging_full_reset_bat_meter == true)
			    && (BMT_status.bat_charging_state == CHR_BATFULL)) {
				resetBatteryMeter = true;
				g_charging_full_reset_bat_meter = false;
			} else {
				resetBatteryMeter = false;
			}
		} else {
			/* increase UI percentage every xxs */
			if (timer_counter >= (cust_sync_time / BAT_TASK_PERIOD)) {
				timer_counter = 1;
				BMT_status.UI_SOC++;
			} else {
				timer_counter++;

				return resetBatteryMeter;
			}

			resetBatteryMeter = true;
		}

		battery_log(BAT_LOG_FULL,
			    "[Battery] mt_battery_100percent_tracking(), Charging full first UI(%d), reset(%d) \r\n",
			    BMT_status.UI_SOC, resetBatteryMeter);
#ifdef CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT
	} else if (g_custom_fake_full == 1){

		if (BMT_status.UI_SOC >= 100)
			BMT_status.UI_SOC = 100;
		else {
			/* increase UI percentage every xxs */
			if (timer_counter >= (cust_sync_time / BAT_TASK_PERIOD)) {
				timer_counter = 1;
				BMT_status.UI_SOC++;
			} else {
				timer_counter++;
				return resetBatteryMeter;
			}
		}

		battery_log(BAT_LOG_FULL,
			    "[Battery] [%s] Fake Charging full first UI(%d), reset(%d) \r\n",
			    __func__, BMT_status.UI_SOC, resetBatteryMeter);
#endif
	} else {
		/* charging is not full,  UI keep 99% if reaching 100%, */

		if (BMT_status.UI_SOC >= 99 && battery_meter_get_battery_current_sign()) {
			BMT_status.UI_SOC = 99;
			resetBatteryMeter = false;

			battery_log(BAT_LOG_FULL,
				    "[Battery] mt_battery_100percent_tracking(), UI full first, keep (%d) \r\n",
				    BMT_status.UI_SOC);
		}

		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);

	}

	return resetBatteryMeter;
}


static bool mt_battery_nPercent_tracking_check(void)
{
	bool resetBatteryMeter = false;
#if defined(CONFIG_SOC_BY_HW_FG)
	static u32 timer_counter;

	if (timer_counter == 0)
		timer_counter = (p_bat_charging_data->npercent_tracking_time / BAT_TASK_PERIOD);

	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return false;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV)
	    && (BMT_status.UI_SOC > BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter ==
		    (p_bat_charging_data->npercent_tracking_time / BAT_TASK_PERIOD)) {
			BMT_status.UI_SOC--;	/* every x sec decrease UI percentage */
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = true;

		battery_log(BAT_LOG_CRTI,
			    "[Battery]mt_battery_nPercent_tracking_check(), ZCV(%d) <= BMT_status.nPercent_ZCV(%d), UI_SOC=%d., tracking UI_SOC=%d \r\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV)
		   && (BMT_status.UI_SOC == BMT_status.nPrecent_UI_SOC_check_point)) {
		/* UI less than 15 , but fuel gague is more than 15, hold UI 15% */
		timer_counter = (p_bat_charging_data->npercent_tracking_time / BAT_TASK_PERIOD);
		resetBatteryMeter = true;

		battery_log(BAT_LOG_CRTI,
			    "[Battery]mt_battery_nPercent_tracking_check() ZCV(%d) > BMT_status.nPercent_ZCV(%d) and UI SOC (%d), then keep %d. \r\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter = (p_bat_charging_data->npercent_tracking_time / BAT_TASK_PERIOD);
	}
#endif
	return resetBatteryMeter;

}

static bool mt_battery_0Percent_tracking_check(void)
{
	bool resetBatteryMeter = true;

	if (BMT_status.UI_SOC <= 0)
		BMT_status.UI_SOC = 0;
	else
		BMT_status.UI_SOC--;

	battery_log(BAT_LOG_CRTI,
		    "[Battery] mt_battery_0Percent_tracking_check(), VBAT < %d UI_SOC = (%d)\r\n",
		    SYSTEM_OFF_VOLTAGE, BMT_status.UI_SOC);

	return resetBatteryMeter;
}


static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static u32 timer_counter;

	if (BMT_status.bat_in_recharging_state == true) {
		BMT_status.UI_SOC = 100;
		return;
	}

#ifdef CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT
	if (check_top_off_state()) {
		BMT_status.UI_SOC = 100;
		return;
	}

	if (g_custom_fake_full == 1)
		return;
#endif

	if ((BMT_status.UI_SOC > BMT_status.SOC) && ((BMT_status.UI_SOC != 1))) {
		/* reduce after xxs */
		if (g_refresh_ui_soc
		    || timer_counter ==
		    (p_bat_charging_data->sync_to_real_tracking_time / BAT_TASK_PERIOD)) {
			BMT_status.UI_SOC--;
			timer_counter = 0;
			g_refresh_ui_soc = false;
		} else {
			timer_counter++;
		}

		battery_log(BAT_LOG_CRTI,
			    "Sync UI percentage to Real one, BMT_status.UI_SOC=%d, BMT_status.SOC=%d, counter = %d\r\n",
			    BMT_status.UI_SOC, BMT_status.SOC, timer_counter);
	} else {
		timer_counter = 0;

		if (BMT_status.UI_SOC == -1)
			BMT_status.UI_SOC = BMT_status.SOC;
		else if (BMT_status.charger_exist && BMT_status.bat_charging_state != CHR_ERROR) {
			if (BMT_status.UI_SOC < BMT_status.SOC
			    && (BMT_status.SOC - BMT_status.UI_SOC > 1))
				BMT_status.UI_SOC++;
			else
				BMT_status.UI_SOC = BMT_status.SOC;
		}
	}

	if (BMT_status.bat_full != true && BMT_status.UI_SOC == 100) {
		battery_log(BAT_LOG_CRTI, "[Sync_UI] keep UI_SOC at 99 due to battery not full yet.\r\n");
		BMT_status.UI_SOC = 99;
	}

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 1;
		battery_log(BAT_LOG_CRTI, "[Battery]UI_SOC get 0 first (%d)\r\n",
			    BMT_status.UI_SOC);
	}
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = &bat_data->psy;
	bool resetBatteryMeter = false;
	char buf[256] = {0};
	static int bat_status_old = POWER_SUPPLY_STATUS_UNKNOWN;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_VOLTAGE_AVG = BMT_status.bat_vol * 1000;
	bat_data->BAT_VOLTAGE_NOW = BMT_status.bat_vol * 1000;	/* voltage_now unit is microvolt */
	bat_data->BAT_TEMP = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if (BMT_status.charger_exist && (BMT_status.bat_charging_state != CHR_ERROR)
	    && !g_cmd_hold_charging) {
		if (BMT_status.bat_exist) {	/* charging */
			if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
				resetBatteryMeter = mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter = mt_battery_100Percent_tracking_check();

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		} else {	/* No Battery, Only Charger */

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			BMT_status.UI_SOC = 0;
		}

	} else {		/* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
			resetBatteryMeter = mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter = mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == true)
		battery_meter_reset(true);
	else
		mt_battery_Sync_UI_Percentage_to_Real();

	battery_log(BAT_LOG_FULL, "UI_SOC=(%d), resetBatteryMeter=(%d)\n",
		    BMT_status.UI_SOC, resetBatteryMeter);

	if (battery_meter_ocv2cv_trans_support()) {
		/* We store capacity before loading compenstation in RTC */
		if (battery_meter_get_battery_soc() <= 1)
			set_rtc_spare_fg_value(1);
		else
			set_rtc_spare_fg_value(battery_meter_get_battery_soc());	/*use battery_soc */
	} else {
		/* set RTC SOC to 1 to avoid SOC jump in charger boot. */
		if (BMT_status.UI_SOC <= 1)
			set_rtc_spare_fg_value(1);
		else
			set_rtc_spare_fg_value(BMT_status.UI_SOC);
	}
	battery_log(BAT_LOG_FULL, "RTC_SOC=(%d)\n", get_rtc_spare_fg_value());

	mt_battery_update_EM(bat_data);

#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_data->BAT_ChargeFull = g_fg_dbg_bat_qmax;
	/*
	 * Correctness of the BatteryStats High-Precision drain metrics
	 * depend on the BatteryStatsExtension plug event handler
	 * reading a value from /sys/.../BAT_ChargeCounter that:
	 *   1. Differs from previous value read only by reflecting
	 *      battery discharge over the interim, not (a) any charging
	 *      or (b) counter resets.
	 *   2. Is reasonably up-to-date relative to the hardware.
	 * Correctness depends on the above for lack of a way to synchronize
	 * the BatteryStats reads with this driver's resets of the hardware
	 * Charge Counter (CAR).
	 *
	 * Satisfy (1a) by ignoring any non-negative new CAR value.
	 * Satisfy (1b) by noting that CAR resets will always be to zero, and
	 * ignoring any non-negative new CAR value.
	 * Satisfy (2) by reading directly from the CAR hardware -- battery_meter_get_car();.
	 */
	{
		int new_CAR = battery_meter_get_car();
/*
		battery_xlog_printk(BAT_LOG_FULL,
			"reading CAR: new_CAR %d, bat_data->old_CAR %d\n",
			new_CAR, bat_data->old_CAR);
*/
		{
			snprintf(buf, sizeof(buf),
				"reading CAR: new_CAR %d, bat_data->old_CAR %d\n",
			new_CAR, bat_data->old_CAR);

			/* log_to_metrics(ANDROID_LOG_INFO, "battery", buf); */
		}

		if (new_CAR < 0) {
			bat_data->BAT_ChargeCounter += (new_CAR - bat_data->old_CAR);
/*
			battery_xlog_printk(BAT_LOG_FULL,
				"setting BAT_ChargeCounter to %d\n",
				bat_data->BAT_ChargeCounter);
*/
			{

				snprintf(buf, sizeof(buf),
					"setting BAT_ChargeCounter to %d\n",
				bat_data->BAT_ChargeCounter);
				memset(buf, 0, 256);

				/* log_to_metrics(ANDROID_LOG_INFO, "battery", buf); */
			}
		}
		bat_data->old_CAR = new_CAR;
	}
	metrics_handle();

#endif

	/* Check recharge */
	if (bat_status_old == POWER_SUPPLY_STATUS_FULL
		&& bat_data->BAT_STATUS == POWER_SUPPLY_STATUS_CHARGING) {
		BMT_status.recharge_cnt += 1;
		pr_info("%s: start recharge: counter=%d, Vbat=%d\n", __func__,
				BMT_status.recharge_cnt, BMT_status.bat_vol);
#if defined(CONFIG_AMAZON_METRICS_LOG)
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf),
			"%s:recharge:detected=1;CT;1,vbat=%d;CT;1:NR",
			__func__, BMT_status.bat_vol);
		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
#endif
	}
	bat_status_old = bat_data->BAT_STATUS;

	power_supply_changed(bat_psy);
}

static void ac_update(struct ac_data *ac_data)
{
	static int ac_status = -1;
	struct power_supply *ac_psy = &ac_data->psy;

	if (BMT_status.charger_exist) {
		if (((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
			(BMT_status.charger_type == STANDARD_CHARGER) ||
			(BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
			(BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
			(BMT_status.charger_type == TYPEC_1_5A_CHARGER) ||
			(BMT_status.charger_type == TYPEC_3A_CHARGER) ||
			(BMT_status.charger_type == TYPEC_PD_5V_CHARGER) ||
			(BMT_status.charger_type == TYPEC_PD_12V_CHARGER))&&
			(BMT_status.bat_charging_state != CHR_ERROR)) {
			ac_data->AC_ONLINE = 1;
			ac_psy->type = POWER_SUPPLY_TYPE_MAINS;
		} else
			ac_data->AC_ONLINE = 0;

	} else {
		ac_data->AC_ONLINE = 0;
	}

	if (ac_status != ac_data->AC_ONLINE) {
		ac_status = ac_data->AC_ONLINE;
		power_supply_changed(ac_psy);
	}
}

static void usb_update(struct usb_data *usb_data)
{
	static int usb_status = -1;
	struct power_supply *usb_psy = &usb_data->psy;

	if (BMT_status.charger_exist) {
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psy->type = POWER_SUPPLY_TYPE_USB;
		} else
			usb_data->USB_ONLINE = 0;
	} else {
		usb_data->USB_ONLINE = 0;
	}

	if (usb_status != usb_data->USB_ONLINE) {
		usb_status = usb_data->USB_ONLINE;
		power_supply_changed(usb_psy);
	}
}

#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Temprature Parameters and functions */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == true)
		return true;

	battery_log(BAT_LOG_CRTI, "[pmic_chrdet_status] No charger\r\n");
	return false;
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Pulse Charging Algorithm */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
bool bat_is_charger_exist(void)
{
	return bat_charger_get_detect_status();
}


bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == true) && (BMT_status.bat_in_recharging_state == false))
		return true;
	else
		return false;
}


u32 bat_get_ui_percentage(void)
{
	/* for plugging out charger in recharge phase, using SOC as UI_SOC */
	if (chr_wake_up_bat == true)
		return BMT_status.SOC;
	else
		return BMT_status.UI_SOC;
}

/* Full state --> recharge voltage --> full state */
u32 bat_is_recharging_phase(void)
{
	if (BMT_status.bat_in_recharging_state || BMT_status.bat_full == true)
		return true;
	else
		return false;
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage();
#endif

	return ret_val;
}


static void mt_battery_average_method_init(u32 *bufferdata, u32 data, s32 *sum)
{
	u32 i;
	static bool batteryBufferFirst = true;
	static bool previous_charger_exist;
	static bool previous_in_recharge_state;
	static u8 index;

	/* reset charging current window while plug in/out */
	if (need_clear_current_window) {
		if (BMT_status.charger_exist) {
			if (previous_charger_exist == false) {
				batteryBufferFirst = true;
				previous_charger_exist = true;
				if ((BMT_status.charger_type == STANDARD_CHARGER)
				    || (BMT_status.charger_type == APPLE_1_0A_CHARGER)
				    || (BMT_status.charger_type == APPLE_2_1A_CHARGER)
				    || (BMT_status.charger_type == CHARGING_HOST))
					data = 650;	/* mA */
				else	/* USB or non-stadanrd charger */
					data = 450;	/* mA */
			} else if ((previous_in_recharge_state == false)
				   && (BMT_status.bat_in_recharging_state == true)) {
				batteryBufferFirst = true;
				if ((BMT_status.charger_type == STANDARD_CHARGER)
				    || (BMT_status.charger_type == APPLE_1_0A_CHARGER)
				    || (BMT_status.charger_type == APPLE_2_1A_CHARGER)
				    || (BMT_status.charger_type == CHARGING_HOST))
					data = 650;	/* mA */
				else	/* USB or non-stadanrd charger */
					data = 450;	/* mA */
			}

			previous_in_recharge_state = BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == true) {
				batteryBufferFirst = true;
				previous_charger_exist = false;
				data = 0;
			}
		}
	}

	battery_log(BAT_LOG_FULL, "batteryBufferFirst =%d, data= (%d)\n", batteryBufferFirst, data);

	if (batteryBufferFirst == true) {
		for (i = 0; i < BATTERY_AVERAGE_SIZE; i++)
			bufferdata[i] = data;

		*sum = data * BATTERY_AVERAGE_SIZE;
	}

	index++;
	if (index >= BATTERY_AVERAGE_DATA_NUMBER) {
		index = BATTERY_AVERAGE_DATA_NUMBER;
		batteryBufferFirst = false;
	}
}


static u32 mt_battery_average_method(u32 *bufferdata, u32 data, s32 *sum, u8 batteryIndex)
{
	u32 avgdata;

	mt_battery_average_method_init(bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	battery_log(BAT_LOG_FULL, "bufferdata[%d]= (%d)\n", batteryIndex, bufferdata[batteryIndex]);
	return avgdata;
}

static int filter_battery_temperature(int instant_temp)
{
	int check_count;

	/* recheck 3 times for critical temperature */
	for (check_count = 0; check_count < 3; check_count++) {
		if (instant_temp > p_bat_charging_data->max_discharge_temperature
			|| instant_temp < p_bat_charging_data->min_discharge_temperature) {

			instant_temp = battery_meter_get_battery_temperature();
			pr_warn("recheck battery temperature result: %d\n", instant_temp);
			msleep(20);
			continue;
		}
	}
	return instant_temp;
}

void mt_battery_GetBatteryData(void)
{
	u32 bat_vol, charger_vol, Vsense, ZCV;
	s32 ICharging, temperature, temperatureR, temperatureV, SOC;
	s32 avg_temperature;
	static s32 bat_sum, icharging_sum, temperature_sum;
	static s32 batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static s32 batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static s32 batteryTempBuffer[BATTERY_AVERAGE_SIZE];
	static u8 batteryIndex;
	static s32 previous_SOC = -1;

	bat_vol = battery_meter_get_battery_voltage();
	Vsense = battery_meter_get_VSense();
	ICharging = battery_meter_get_charging_current();
	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);

	if (bat_meter_timeout == true) {
		SOC = battery_meter_get_battery_percentage();
		bat_meter_timeout = false;
	} else {
		if (previous_SOC == -1)
			SOC = battery_meter_get_battery_percentage();
		else
			SOC = previous_SOC;
	}

	ZCV = battery_meter_get_battery_zcv();

	need_clear_current_window = true;
	BMT_status.ICharging =
	    mt_battery_average_method(&batteryCurrentBuffer[0], ICharging, &icharging_sum,
				      batteryIndex);
	need_clear_current_window = false;
#if 1
	if (previous_SOC == -1 && bat_vol <= SYSTEM_OFF_VOLTAGE) {
		battery_log(BAT_LOG_CRTI,
			    "battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol =
		    mt_battery_average_method(&batteryVoltageBuffer[0], ZCV, &bat_sum,
					      batteryIndex);
	} else {
		BMT_status.bat_vol =
		    mt_battery_average_method(&batteryVoltageBuffer[0], bat_vol, &bat_sum,
					      batteryIndex);
	}
#else
	BMT_status.bat_vol =
	    mt_battery_average_method(&batteryVoltageBuffer[0], bat_vol, &bat_sum, batteryIndex);
#endif
	avg_temperature =
	    mt_battery_average_method(&batteryTempBuffer[0], temperature, &temperature_sum,
				      batteryIndex);

	if (p_bat_charging_data->use_avg_temperature)
		BMT_status.temperature = avg_temperature;
	else
		BMT_status.temperature = filter_battery_temperature(temperature);

	if ((g_battery_thermal_throttling_flag == 1) || (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature = battery_cmd_thermal_test_mode_value;
			battery_log(BAT_LOG_FULL,
				    "[Battery] In thermal_test_mode , Tbat=%d\n",
				    BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (BMT_status.temperature > p_bat_charging_data->max_discharge_temperature
			|| BMT_status.temperature < p_bat_charging_data->min_discharge_temperature) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = &bat_data->psy;

			pr_warn("[Battery] instant Tbat(%d) out of range, power down device.\n",
				BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need reset system */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();
			else
				orderly_poweroff(true);
		}
#endif
	}

	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.SOC = SOC;
	BMT_status.ZCV = ZCV;

	if (BMT_status.charger_exist == false && !battery_meter_ocv2cv_trans_support()) {
		if (BMT_status.SOC > previous_SOC && previous_SOC >= 0)
			BMT_status.SOC = previous_SOC;
	}

	previous_SOC = BMT_status.SOC;

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;



	battery_log(BAT_LOG_CRTI,
		    "AvgVbat=(%d),bat_vol=(%d),AvgI=(%d),I=(%d),VChr=(%d),AvgT=(%d),T=(%d),pre_SOC=(%d),SOC=(%d),ZCV=(%d)\n",
		    BMT_status.bat_vol, bat_vol, BMT_status.ICharging, ICharging,
		    BMT_status.charger_vol, BMT_status.temperature, temperature,
		    previous_SOC, BMT_status.SOC, BMT_status.ZCV);


}


static int mt_battery_CheckBatteryTemp(void)
{
	int status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	battery_log(BAT_LOG_CRTI, "[BATTERY] support JEITA, temperature=%d\n",
		    BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else

#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature < p_bat_charging_data->min_charge_temperature)
	    || (BMT_status.temperature == p_bat_charging_data->err_charge_temperature)) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif
	if (BMT_status.temperature >= p_bat_charging_data->max_charge_temperature) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Battery Over Temperature !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif

	return status;
}


static int mt_battery_CheckChargerVoltage(void)
{
	int status = PMU_STATUS_OK;

	if (BMT_status.charger_exist) {
		if (p_bat_charging_data->v_charger_enable == 1) {
			if (BMT_status.charger_vol <= p_bat_charging_data->v_charger_min) {
				battery_log(BAT_LOG_CRTI, "[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}
		if (BMT_status.charger_vol >= p_bat_charging_data->v_charger_max) {
			battery_log(BAT_LOG_CRTI, "[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}


static int mt_battery_CheckChargingTime(void)
{
	int status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		battery_log(BAT_LOG_CRTI,
			    "[TestMode] Disable Safety Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			    g_battery_thermal_throttling_flag,
			    battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME) {
			battery_log(BAT_LOG_CRTI, "[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;

}

#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
static int mt_battery_CheckCallState(void)
{
	int status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE)
	    && (BMT_status.bat_vol > p_bat_charging_data->v_cc2topoff_thres))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	if (mt_battery_CheckBatteryTemp() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (mt_battery_CheckChargerVoltage() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
	if (mt_battery_CheckCallState() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_HOLD;
		return;
	}
#endif

	if (mt_battery_CheckChargingTime() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (g_cmd_hold_charging == true) {
		BMT_status.bat_charging_state = CHR_CMD_HOLD;
		return;
	} else if (BMT_status.bat_charging_state == CHR_CMD_HOLD) {
		BMT_status.bat_charging_state = CHR_PRE;
		return;
	}

	if (BMT_status.bat_charging_state == CHR_ERROR)
		BMT_status.bat_charging_state = CHR_PRE;
}


static void mt_battery_notify_TatalChargingTime_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME)
	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		battery_log(BAT_LOG_CRTI, "[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME)
			/* if(BMT_status.total_charging_time >= 60) //test */
		{
			g_BatteryNotifyCode |= 0x0010;
			battery_log(BAT_LOG_CRTI, "[BATTERY] Charging Over Time\n");
		} else {
			g_BatteryNotifyCode &= ~(0x0010);
		}
	}

	battery_log(BAT_LOG_FULL,
		    "[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350)
		/* if(BMT_status.bat_vol > 3800) //test */
	{
		g_BatteryNotifyCode |= 0x0008;
		battery_log(BAT_LOG_CRTI, "[BATTERY] bat_vlot(%d) > 4350mV\n", BMT_status.bat_vol);
	} else {
		g_BatteryNotifyCode &= ~(0x0008);
	}

	battery_log(BAT_LOG_FULL, "[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) && (BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		battery_log(BAT_LOG_CRTI, "[BATTERY] I_charging(%ld) > 1000mA\n",
			    BMT_status.ICharging);
	} else {
		g_BatteryNotifyCode &= ~(0x0004);
	}

	battery_log(BAT_LOG_CRTI, "[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((BMT_status.temperature >= p_bat_charging_data->max_charge_temperature)
	    || (BMT_status.temperature < p_bat_charging_data->temp_neg_10_threshold)) {
#else
#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature >= p_bat_charging_data->max_charge_temperature)
	    || (BMT_status.temperature < p_bat_charging_data->min_charge_temperature)) {
#else
	if (BMT_status.temperature >= p_bat_charging_data->max_charge_temperature) {
#endif
#endif				/* #if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT) */
		g_BatteryNotifyCode |= 0x0002;
		battery_log(BAT_LOG_CRTI, "[BATTERY] bat_temp(%d) out of range\n",
			    BMT_status.temperature);
	} else {
		g_BatteryNotifyCode &= ~(0x0002);
	}

	battery_log(BAT_LOG_CRTI, "[BATTERY] BATTERY_NOTIFY_CASE_0002_VBATTEMP (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VCharger_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	if (BMT_status.charger_vol > p_bat_charging_data->v_charger_max) {
		g_BatteryNotifyCode |= 0x0001;
		battery_log(BAT_LOG_CRTI, "[BATTERY] BMT_status.charger_vol(%d) > %d mV\n",
			    BMT_status.charger_vol, p_bat_charging_data->v_charger_max);
	} else {
		g_BatteryNotifyCode &= ~(0x0001);
	}

	battery_log(BAT_LOG_FULL, "[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
		    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		battery_log(BAT_LOG_CRTI, "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		battery_log(BAT_LOG_CRTI, "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		battery_log(BAT_LOG_CRTI, "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Unknown BN_TestMode Code : %x\n",
			    g_BN_TestMode);
	}
}


void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) {	/* for normal case */
		battery_log(BAT_LOG_FULL, "[BATTERY] mt_battery_notify_check\n");

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TatalChargingTime_check();
	} else {		/* for UI test */

		mt_battery_notify_UI_test();
	}
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) || (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature = battery_cmd_thermal_test_mode_value;
			battery_log(BAT_LOG_FULL,
				    "[Battery] In thermal_test_mode , Tbat=%d\n",
				    BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (BMT_status.temperature > p_bat_charging_data->max_discharge_temperature
			|| BMT_status.temperature < p_bat_charging_data->min_discharge_temperature) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = &bat_data->psy;

			pr_warn("[Battery] Tbat(%d)out of range, power down device.\n",
				BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need reset system */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();
			else
				orderly_poweroff(true);
		}
#else
		if (BMT_status.temperature >= p_bat_charging_data->max_discharge_temperature) {
#if defined(CONFIG_POWER_EXT)
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
			{
				if ((g_platform_boot_mode == META_BOOT)
				    || (g_platform_boot_mode == ADVMETA_BOOT)
				    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
					battery_log(BAT_LOG_FULL,
						    "[BATTERY] boot mode = %d, bypass temperature check\n",
						    g_platform_boot_mode);
				} else {
					struct battery_data *bat_data = &battery_main;
					struct power_supply *bat_psy = &bat_data->psy;

					battery_log(BAT_LOG_ERROR,
						    "[Battery] Tbat(%d)>=%d, system need power down.\n",
						    p_bat_charging_data->max_discharge_temperature,
						    BMT_status.temperature);

					bat_data->BAT_CAPACITY = 0;

					power_supply_changed(bat_psy);

					/* can not power down due to charger exist, so need reset system */
					if (BMT_status.charger_exist)
						bat_charger_set_platform_reset();
					else
						orderly_poweroff(true);
				}
			}
#endif
		}
#endif

	}

}

bool bat_check_usb_ready(void)
{
	bool is_ready = false;
	mutex_lock(&usb_ready_mutex);
	is_ready = g_bat.usb_connect_ready;
	mutex_unlock(&usb_ready_mutex);
	return is_ready;
}

int bat_read_charger_type(void)
{
	int type = 0;

	mutex_lock(&charger_type_mutex);
	type = bat_charger_get_charger_type();
	mutex_unlock(&charger_type_mutex);

	return type;
}

int bat_charger_type_detection(void)
{
	mutex_lock(&charger_type_mutex);
	if (BMT_status.charger_type == CHARGER_UNKNOWN)
		BMT_status.charger_type = bat_charger_get_charger_type();
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

void bat_update_charger_type(int new_type)
{
	mutex_lock(&charger_type_mutex);
	BMT_status.charger_type = new_type;
	mutex_unlock(&charger_type_mutex);
	battery_log(BAT_LOG_CRTI, "update new charger type: %d\n", new_type);
	wake_up_bat();
}

static void mt_battery_charger_detect_check(void)
{
	static bool fg_first_detect;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128] = {0};
	static const char * const charger_type_text[] = {
		"UNKNOWN", "STANDARD_HOST", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "STANDARD_CHARGER",
		"APPLE_2_1A_CHARGER","APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER",
		"TYPEC_1_5A_CHARGER", "TYPEC_3A_CHARGER", "TYPEC_PD_5V_CHARGER",
		"TYPEC_PD_12V_CHARGER"
	};
#endif

	if (upmu_is_chr_det() == true) {

		if (!BMT_status.charger_exist)
			wake_lock(&battery_suspend_lock);

		BMT_status.charger_exist = true;

		/* re-detect once after 10s if it is non-standard type */
		if (BMT_status.charger_type == NONSTANDARD_CHARGER && fg_first_detect) {
			mutex_lock(&charger_type_mutex);
			BMT_status.charger_type = bat_charger_get_charger_type();
			mutex_unlock(&charger_type_mutex);
			fg_first_detect = false;
			if (BMT_status.charger_type != NONSTANDARD_CHARGER)
				pr_warn("Update charger type to %d!\n", BMT_status.charger_type);
		}

		if (BMT_status.charger_type == CHARGER_UNKNOWN) {

			getrawmonotonic(&chr_plug_in_time);
			g_custom_plugin_time = 0;
			g_custom_charging_cv = -1;

			bat_charger_type_detection();
			BMT_status.charger_plugin_cnt++;
			if ((BMT_status.charger_type == STANDARD_HOST)
			    || (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
			if (BMT_status.charger_type != CHARGER_UNKNOWN)
				fg_first_detect = true;
#ifdef CONFIG_AMAZON_METRICS_LOG
			memset(buf, '\0', sizeof(buf));
			if (BMT_status.charger_type > CHARGER_UNKNOWN
				&& BMT_status.charger_type <= TYPEC_PD_12V_CHARGER) {
				snprintf(buf, sizeof(buf),
					"%s:bq24297:chg_type_%s=1;CT;1:NR",
					__func__,
					charger_type_text[BMT_status.charger_type]);
				log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
			}
#endif

		}

		battery_log(BAT_LOG_FULL, "[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
			    BMT_status.charger_type);

	} else {
		if (BMT_status.charger_exist) {
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
				wake_lock(&battery_suspend_lock);
			else
				wake_lock_timeout(&battery_suspend_lock, HZ / 2);
		}
		fg_first_detect = false;

		BMT_status.charger_exist = false;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		BMT_status.bat_full = false;
		BMT_status.bat_in_recharging_state = false;
		BMT_status.bat_charging_state = CHR_PRE;
		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {

			/* in case of pd hw reset. wait for vbus re-assert */
			if (mt_usb_pd_support())
				msleep(1000);
			if (upmu_is_chr_det() == false) {
				pr_warn("Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				orderly_poweroff(true);
			}
		}
#endif

		if (g_cmd_hold_charging) {
			g_cmd_hold_charging = false;
			bat_charger_enable_power_path(true);
		}

		g_custom_fake_full = -1;
		battery_log(BAT_LOG_FULL, "[BAT_thread]Cable out \r\n");
		mt_usb_disconnect();
	}
}

#if defined(CONFIG_AMAZON_METRICS_LOG)
void metrics_battery_save_data(void)
{
	unsigned long temp = get_virtualsensor_temp();

	/* Save Data for Next Metric */
	metrics_charge_info.init_charging_vol = BMT_status.bat_vol;

	/* Set Peak temperatures to current temperature */
	metrics_charge_info.battery_peak_battery_temp = BMT_status.temperature;
	metrics_charge_info.battery_peak_virtual_temp = get_virtualsensor_temp();

	/* Save Current USB Charging Voltage as Peak */
	metrics_charge_info.usb_charger_voltage_peak = BMT_status.charger_vol;

	metrics_charge_info.battery_average_virtual_temp = temp;
	metrics_charge_info.battery_average_battery_temp = BMT_status.temperature;

	/* Set Charging Ticks to 1 */
	metrics_charge_info.battery_charge_ticks = 1;

	/* Set Time start for Next Metric */
	metrics_charge_info.charger_time = current_kernel_time();
}

static void battery_charge_metric(void)
{
	unsigned long virtual_temp = get_virtualsensor_temp();
	char buf[512];
	long elaps_sec;
	struct timespec diff;
	static bool batt_metrics_first_run = true;
	static int battery_was_charging;
	static int battery_was_charging_ticks;
	static unsigned int last_vol;
	static unsigned int usb_charger_voltage_previous;

	if (batt_metrics_first_run) {
		batt_metrics_first_run = false;

		/* Default data for first boot */
		metrics_battery_save_data();
	}

	/* Record Peak  Battery temperature during charge or discharge cycle */
	if (BMT_status.temperature > metrics_charge_info.battery_peak_battery_temp)
		metrics_charge_info.battery_peak_battery_temp = BMT_status.temperature;

	/* Record Peak Virtual temperature during charge or discharge cycle */
	if (virtual_temp > metrics_charge_info.battery_peak_virtual_temp)
		metrics_charge_info.battery_peak_virtual_temp = virtual_temp;

	if (metrics_charge_info.usb_charger_voltage_peak < BMT_status.charger_vol)
		metrics_charge_info.usb_charger_voltage_peak = BMT_status.charger_vol;

	/* Record Average Temperature Values */
	metrics_charge_info.battery_average_virtual_temp =
		(((metrics_charge_info.battery_average_virtual_temp *
		metrics_charge_info.battery_charge_ticks) +
		virtual_temp) / (metrics_charge_info.battery_charge_ticks + 1));

	metrics_charge_info.battery_average_battery_temp =
		((metrics_charge_info.battery_average_battery_temp *
		metrics_charge_info.battery_charge_ticks) +
		BMT_status.temperature) / (metrics_charge_info.battery_charge_ticks + 1);

	/* Increase Charging Ticks */
	metrics_charge_info.battery_charge_ticks++;

	/* Check to see of Charging event Status has changed.
	 * Add a 6 cycle (1 min) de-bounce to prevent extra
	 * events from firing.
	 */
	if (battery_was_charging != battery_main.BAT_STATUS)
		battery_was_charging_ticks++;
	else {
		if (battery_was_charging_ticks > 0)
			battery_was_charging_ticks--;
		else {
			/* Save Last Voltage - Most of the time this
			 * voltage will be from Before Charge Started,
			 * or Before Charge Stopped. On average 8 seconds
			 * before. Since there is a few microscend delay.
			 * Between Actual charge start/Stop and FW knowing
			 * charge start/stop less then 1% of devices
			 */
			last_vol = BMT_status.bat_vol;
			usb_charger_voltage_previous = BMT_status.charger_vol;
		}
	}

	if (battery_was_charging_ticks > 5) {
		/* Reset ticks to 0; */
		battery_was_charging_ticks = 0;

		/* Calculate Elapsed Time */
		diff = timespec_sub(current_kernel_time(), metrics_charge_info.charger_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;

		/* Handle Case where Clock changes during Charging,
		 * be very generous with timing as clock may be
		 * more accurate then ticks.
		 */
		if (elaps_sec > metrics_charge_info.battery_charge_ticks * 30)
			elaps_sec = metrics_charge_info.battery_charge_ticks * 15;

		if (elaps_sec > 0 && elaps_sec < MAX_TIME_INTERVAL) {
			/* Log Metric for Charge / Discharge Cycle */
			snprintf(buf, sizeof(buf),
				"bq24297:def:Charger_Status=%d;CT;1,Elaps_Sec=%ld;CT;1,"
				"iVol=%d;CT;1,fVol=%d;CT;1,lVol=%d;CT;1,SOC=%d;CT;1,"
				"Bat_aTemp=%d;CT;1,Vir_aTemp=%d;CT;1,Bat_pTemp=%ld;CT;1,"
				"Vir_pTemp=%ld;CT;1,bTemp=%d;CT;1,Cycles=%d;CT;1,"
				"pVUsb=%ld;CT;1,fVUsb=%d;CT;1:NA", battery_was_charging,
				elaps_sec, metrics_charge_info.init_charging_vol,
				BMT_status.bat_vol, last_vol, BMT_status.SOC,
				(int)metrics_charge_info.battery_average_battery_temp,
				(int)metrics_charge_info.battery_average_virtual_temp,
				metrics_charge_info.battery_peak_battery_temp,
				metrics_charge_info.battery_peak_virtual_temp,
				BMT_status.temperature, gFG_battery_cycle,
				metrics_charge_info.usb_charger_voltage_peak,
				usb_charger_voltage_previous);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}

		/*Save data for Next Metric */
		metrics_battery_save_data();

		/* Set Was charging Status */
		battery_was_charging = battery_main.BAT_STATUS;
	}
}
#endif

static void mt_battery_update_status(void)
{
#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI, "[BATTERY] CONFIG_POWER_EXT, no update Android.\n");
#else
	{
		if (battery_meter_initilized == true)
			battery_update(&battery_main);

		ac_update(&ac_main);
		usb_update(&usb_main);
#if defined(CONFIG_AMAZON_METRICS_LOG)
		metrics_charger_update(ac_main.AC_ONLINE, usb_main.USB_ONLINE);
		battery_charge_metric();
#endif
	}
#endif
}

extern void ssusb_rerun_dock_detection(void);
static void do_chrdet_int_task(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128] = {0};
#endif

	if (g_bat_init_flag == true) {

		/* Recheck if unpowered dock is powered again */
		ssusb_rerun_dock_detection();

		if (upmu_is_chr_det() == true) {
			pr_debug("[do_chrdet_int_task] charger exist!\n");

#ifdef CONFIG_AMAZON_METRICS_LOG
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_on=1;CT;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

			if (!BMT_status.charger_exist)
				wake_lock(&battery_suspend_lock);
			BMT_status.charger_exist = true;

#if defined(CONFIG_POWER_EXT)
			bat_charger_type_detection();
			mt_usb_connect();
			battery_log(BAT_LOG_CRTI,
				    "[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
#endif
		} else {
			pr_debug("[do_chrdet_int_task] charger NOT exist!\n");

#ifdef CONFIG_AMAZON_METRICS_LOG
			memset(buf, '\0', sizeof(buf));
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_off=1;CT;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

			if (BMT_status.charger_exist) {
				if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
					wake_lock(&battery_suspend_lock);
				else
					wake_lock_timeout(&battery_suspend_lock, HZ / 2);
			}
			BMT_status.charger_exist = false;
			BMT_status.charger_type = CHARGER_UNKNOWN;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				/* in case of pd hw reset. wait for vbus re-assert */
				if (mt_usb_pd_support())
					msleep(1000);
				if (upmu_is_chr_det() == false) {
					pr_warn("Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
					orderly_poweroff(true);
				}
			}
#endif

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			battery_log(BAT_LOG_CRTI,
				    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			is_ta_connect = false;
			ta_check_chr_type = true;
			ta_cable_out_occur = true;
#endif
		}

		if (BMT_status.UI_SOC == 100 && BMT_status.charger_exist) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
		}
		mt_battery_update_status();
		wake_up_bat();
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
	}

}

irqreturn_t ops_chrdet_int_handler(int irq, void *dev_id)
{
	pr_debug("[Power/Battery][chrdet_bat_int_handler]....\n");

	do_chrdet_int_task();

	return IRQ_HANDLED;
}

void BAT_thread(void)
{
	struct timespec now_time;
	unsigned long total_time_plug_in;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[256] = {0};
	unsigned long virtual_temp = get_virtualsensor_temp();
	static bool bat_14days_flag;
	static bool bat_demo_flag;
#endif

	if (battery_meter_initilized == false) {
		battery_meter_initial();	/* move from battery_probe() to decrease booting time */
		BMT_status.nPercent_ZCV = battery_meter_get_battery_nPercent_zcv();
#ifdef CONFIG_MTK_BATTERY_CVR_SUPPORT
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		gFG_CV_Battery_Voltage = p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cv_voltage;
#else
		gFG_CV_Battery_Voltage = 4200000;
#endif
		gFG_CV_Voltage_Reduction_Supported = 1;
#endif
		battery_meter_initilized = true;

	}

	if (g_bat.usb_connect_ready)
		mt_battery_charger_detect_check();

	if (fg_battery_shutdown)
		return;

	mt_battery_GetBatteryData();

	if (fg_battery_shutdown)
		return;

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if (BMT_status.charger_exist && !fg_battery_shutdown) {
		getrawmonotonic(&now_time);

		total_time_plug_in = g_custom_plugin_time;
		if ((now_time.tv_sec - chr_plug_in_time.tv_sec) > 0)
			total_time_plug_in += now_time.tv_sec - chr_plug_in_time.tv_sec;

		if (total_time_plug_in > PLUGIN_THRESHOLD) {
			g_custom_charging_cv = BATTERY_VOLT_04_096000_V;
#ifdef CONFIG_AMAZON_METRICS_LOG
			if (!bat_14days_flag) {
				bat_14days_flag = true;
				snprintf(buf, sizeof(buf),
					"bq24297:def:Charging_Over_14days=%d;CT;1,Total_Plug_Time=%ld;CT;1,"
					"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1,"
					"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
					1, total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
					BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

				log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
				memset(buf, 0, 256);
			}

			if (total_time_plug_in <= PLUGIN_THRESHOLD && bat_14days_flag)
				bat_14days_flag = false;
#endif
		}
		else
			g_custom_charging_cv = -1;

#ifdef CONFIG_AMAZON_METRICS_LOG
		if (g_custom_charging_mode == 1 && !bat_demo_flag) {
			bat_demo_flag = true;
			snprintf(buf, sizeof(buf),
				"bq24297:def:Store_Demo_Mode=%d;CT;1,Total_Plug_Time=%ld;CT;1,"
				"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1,"
				"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
				g_custom_charging_mode, total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
				BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		}

		if (g_custom_charging_mode != 1 && bat_demo_flag)
			bat_demo_flag = false;
#endif

		pr_notice("total_time_plug_in(%lu), cv(%d)\r\n",
				total_time_plug_in, g_custom_charging_cv);

		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	if (!BMT_status.charger_exist)
		bat_charger_enable(false);

	bat_charger_reset_watchdog_timer();

	mt_battery_update_status();
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		int ret;

		/* check usb ready once at boot time */
		mutex_lock(&usb_ready_mutex);
		if (g_bat.usb_connect_ready == false) {
			while (!mt_usb_is_ready()) {
				pr_info("wait usb connection ready..\n");
				msleep(100);
			}
			g_bat.usb_connect_ready = true;
		}
		mutex_unlock(&usb_ready_mutex);

		mutex_lock(&bat_mutex);

		if (g_bat.init_done && !battery_suspended)
			BAT_thread();

		mutex_unlock(&bat_mutex);

		ret = wait_event_hrtimeout(bat_thread_wq, atomic_read(&bat_thread_wakeup), ktime);
		if (ret == -ETIME)
			bat_meter_timeout = true;
		else
			atomic_dec(&bat_thread_wakeup);

		pr_debug("%s: waking up: on %s; wake_flag=%d\n",
			 __func__, ret == -ETIME ? "timer" : "event",
			 atomic_read(&bat_thread_wakeup));

		if (!fg_battery_shutdown)
			ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */

		if (chr_wake_up_bat == true) {	/* for charger plug in/ out */

			if (g_bat.init_done)
				battery_meter_reset_aging();
			chr_wake_up_bat = false;
		}
	}
	mutex_lock(&bat_mutex);
	g_bat.down = true;
	mutex_unlock(&bat_mutex);

	return 0;
}

/*
 * This is charger interface to USB OTG code.
 * If OTG is host, charger functionality, and charger interrupt
 * must be disabled
 * */
void bat_detect_set_usb_host_mode(bool usb_host_mode)
{
	mutex_lock(&bat_mutex);
	/* Don't change the charger event state
	 * if charger logic is not running */
	if (g_bat.init_done) {
		if (usb_host_mode && !g_bat.usb_host_mode)
			disable_irq(g_bat.irq);
		if (!usb_host_mode && g_bat.usb_host_mode)
			enable_irq(g_bat.irq);

		g_bat.usb_host_mode = usb_host_mode;
	}
	mutex_unlock(&bat_mutex);
}
EXPORT_SYMBOL(bat_detect_set_usb_host_mode);

static int bat_setup_charger_locked(void)
{
	int ret = -EAGAIN;

	if (g_bat.common_init_done && g_bat.charger && !g_bat.init_done) {

		/* AP:
		 * Both common_battery and charger code are ready to go.
		 * Finalize init of common_battery.
		 */
		g_platform_boot_mode = bat_charger_get_platform_boot_mode();
		battery_log(BAT_LOG_CRTI, "[BAT_probe] g_platform_boot_mode = %d\n ",
			    g_platform_boot_mode);

		/* AP:
		 * MTK implementation requires that BAT_thread() be called at least once
		 * before battery event is enabled.
		 * Although this should not be necessary, we maintain compatibility
		 * until rework is complete.
		 */

		BAT_thread();
		g_bat.init_done = true;

		ret = irq_set_irq_wake(g_bat.irq, true);
		if (ret)
			pr_err("%s: irq_set_irq_wake err = %d\n", __func__, ret);

		enable_irq(g_bat.irq);

		pr_warn("%s: charger setup done\n", __func__);
	}

	/* if there is no external charger, we just enable detect irq */
#if defined(CONFIG_POWER_EXT) && defined(NO_EXTERNAL_CHARGER)
	ret = irq_set_irq_wake(g_bat.irq, true);
	if (ret)
		pr_err("%s: irq_set_irq_wake err = %d\n", __func__, ret);

	enable_irq(g_bat.irq);
	pr_warn("%s: no charger. just enable detect irq.\n", __func__);
#endif

	return ret;
}

int bat_charger_register(CHARGING_CONTROL ctrl)
{
	int ret;

	mutex_lock(&bat_mutex);
	g_bat.charger = ctrl;
	ret = bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return ret;
}
EXPORT_SYMBOL(bat_charger_register);

#if defined(CONFIG_AMAZON_METRICS_LOG)
static void metrics_resume(void)
{
	struct battery_info *info = &BQ_info;
	/* invalidate all the measurements */
	mutex_lock(&info->lock);
	info->flags |= BQ_STATUS_RESUMING;
	mutex_unlock(&info->lock);
}
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	mutex_lock(&bat_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;	/* enable calibration after setting ADC_CALI_Cal */
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) || (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		/*
		   for (i = 0; i < 14; i++)
		   battery_log(BAT_LOG_CRTI, "adc_cali_slop[%d] = %d\n", i,
		   *(adc_cali_slop + i));
		   battery_log(BAT_LOG_FULL,
		   "**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		 */
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = false;	/* enable calibration after setting ADC_CALI_Cal */
		/*
		   for (i = 0; i < 14; i++)
		   battery_log(BAT_LOG_CRTI, "adc_cali_offset[%d] = %d\n", i,
		   *(adc_cali_offset + i));
		   battery_log(BAT_LOG_FULL,
		   "**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		 */
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = true;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = true;
		else
			g_ADC_Cali = false;

		for (i = 0; i < 1; i++)
			battery_log(BAT_LOG_CRTI, "adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		battery_log(BAT_LOG_FULL, "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = false; // 20100508 Infinity */
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);	/* 2*int = 2*4 */

		if (adc_in_data[0] == 0)	/* I_SENSE */
			adc_out_data[0] = battery_meter_get_VSense() * adc_in_data[1];
		else if (adc_in_data[0] == 1)	/* BAT_SENSE */
			adc_out_data[0] = battery_meter_get_battery_voltage() * adc_in_data[1];
		else if (adc_in_data[0] == 3)	/* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() * adc_in_data[1];
		else if (adc_in_data[0] == 30)	/* V_Bat_temp magic number */
			adc_out_data[0] = battery_meter_get_battery_temperature() * adc_in_data[1];
		else if (adc_in_data[0] == 66) {
			adc_out_data[0] = (battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() == true)
				adc_out_data[0] = 0 - adc_out_data[0];	/* charging */

		} else {
			battery_log(BAT_LOG_FULL, "unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		battery_log(BAT_LOG_CRTI,
			    "**** unlocked_ioctl : Channel %d * %d times = %d\n",
			    adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;

		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		battery_log(BAT_LOG_CRTI, "**** unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;

	case Set_Charger_Current:	/* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = true;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;

		wake_up_bat();
		battery_log(BAT_LOG_CRTI, "**** unlocked_ioctl : set_Charger_Current:%d\n",
			    charging_level_data[0]);
		break;
		/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.bat_vol;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	case Get_META_BAT_SOC:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.UI_SOC;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
		/* add bing meta tool------------------------------- */

	default:
		g_ADC_Cali = false;
		break;
	}

	mutex_unlock(&bat_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
	.open = adc_cali_open,
	.release = adc_cali_release,
};


void check_battery_exist(void)
{
#if defined(CONFIG_CONFIG_DIS_CHECK_BATTERY)
	battery_log(BAT_LOG_FULL, "[BATTERY] Disable check battery exist.\n");
#else
	u32 baton_count = 0;
	u32 i;

	for (i = 0; i < 3; i++)
		baton_count += bat_charger_get_battery_status();

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)
		    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			battery_log(BAT_LOG_FULL,
				    "[BATTERY] boot mode = %d, bypass battery check\n",
				    g_platform_boot_mode);
		} else {
			battery_log(BAT_LOG_FULL,
				    "[BATTERY] Battery is not exist, power off FAN5405 and system (%d)\n",
				    baton_count);

			bat_charger_enable(false);
			bat_charger_set_platform_reset();
		}
	}
#endif
}

static void bat_parse_node(struct device_node *np, char *name, int *cust_val)
{
	u32 val;

	if (of_property_read_u32(np, name, &val) == 0) {
		(*cust_val) = (int)val;
		pr_debug("%s get %s :%d\n", __func__, name, *cust_val);
	}
}

static void init_charging_data_from_dt(struct device_node *np)
{
	bat_parse_node(np, "battery_cv_voltage", &p_bat_charging_data->battery_cv_voltage);
	bat_parse_node(np, "v_charger_max", &p_bat_charging_data->v_charger_max);
	bat_parse_node(np, "v_charger_min", &p_bat_charging_data->v_charger_min);
	bat_parse_node(np, "max_discharge_temperature", &p_bat_charging_data->max_discharge_temperature);
	bat_parse_node(np, "min_discharge_temperature", &p_bat_charging_data->min_discharge_temperature);
	bat_parse_node(np, "max_charge_temperature", &p_bat_charging_data->max_charge_temperature);
	bat_parse_node(np, "min_charge_temperature", &p_bat_charging_data->min_charge_temperature);
	bat_parse_node(np, "use_avg_temperature", &p_bat_charging_data->use_avg_temperature);
	bat_parse_node(np, "usb_charger_current", &p_bat_charging_data->usb_charger_current);
	bat_parse_node(np, "ac_charger_current", &p_bat_charging_data->ac_charger_current);
	bat_parse_node(np, "ac_charger_input_current", &p_bat_charging_data->ac_charger_input_current);
	bat_parse_node(np, "non_std_ac_charger_current", &p_bat_charging_data->non_std_ac_charger_current);
	bat_parse_node(np, "charging_host_charger_current", &p_bat_charging_data->charging_host_charger_current);
	bat_parse_node(np, "apple_0_5a_charger_current", &p_bat_charging_data->apple_0_5a_charger_current);
	bat_parse_node(np, "apple_1_0a_charger_current", &p_bat_charging_data->apple_1_0a_charger_current);
	bat_parse_node(np, "apple_2_1a_charger_current", &p_bat_charging_data->apple_2_1a_charger_current);
	bat_parse_node(np, "ta_start_battery_soc", &p_bat_charging_data->ta_start_battery_soc);
	bat_parse_node(np, "ta_stop_battery_soc", &p_bat_charging_data->ta_stop_battery_soc);
	bat_parse_node(np, "ta_ac_9v_input_current", &p_bat_charging_data->ta_ac_9v_input_current);
	bat_parse_node(np, "ta_ac_7v_input_current", &p_bat_charging_data->ta_ac_7v_input_current);
	bat_parse_node(np, "ta_ac_charging_current", &p_bat_charging_data->ta_ac_charging_current);
	bat_parse_node(np, "ta_9v_support", &p_bat_charging_data->ta_9v_support);
	bat_parse_node(np, "temp_pos_60_threshold", &p_bat_charging_data->temp_pos_60_threshold);
	bat_parse_node(np, "temp_pos_60_thres_minus_x_degree", &p_bat_charging_data->temp_pos_60_thres_minus_x_degree);
	bat_parse_node(np, "temp_pos_45_threshold", &p_bat_charging_data->temp_pos_45_threshold);
	bat_parse_node(np, "temp_pos_45_thres_minus_x_degree", &p_bat_charging_data->temp_pos_45_thres_minus_x_degree);
	bat_parse_node(np, "temp_pos_10_threshold", &p_bat_charging_data->temp_pos_10_threshold);
	bat_parse_node(np, "temp_pos_10_thres_plus_x_degree", &p_bat_charging_data->temp_pos_10_thres_plus_x_degree);
	bat_parse_node(np, "temp_pos_0_threshold ", &p_bat_charging_data->temp_pos_0_threshold);
	bat_parse_node(np, "temp_pos_0_thres_plus_x_degree", &p_bat_charging_data->temp_pos_0_thres_plus_x_degree);
	bat_parse_node(np, "temp_neg_10_threshold", &p_bat_charging_data->temp_neg_10_threshold);
	bat_parse_node(np, "temp_neg_10_thres_plus_x_degree",
		&p_bat_charging_data->temp_neg_10_thres_plus_x_degree);
	bat_parse_node(np, "jeita_temp_above_pos_60_cv_voltage ",
		&p_bat_charging_data->jeita_temp_above_pos_60_cv_voltage);
	bat_parse_node(np, "jeita_temp_pos_45_to_pos_60_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_45_to_pos_60_cv_voltage);
	bat_parse_node(np, "jeita_temp_pos_10_to_pos_45_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cv_voltage);
	bat_parse_node(np, "jeita_temp_pos_0_to_pos_10_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_0_to_pos_10_cv_voltage);
	bat_parse_node(np, "jeita_temp_neg_10_to_pos_0_cv_voltage",
		&p_bat_charging_data->jeita_temp_neg_10_to_pos_0_cv_voltage);
	bat_parse_node(np, "jeita_temp_below_neg_10_cv_voltage",
		&p_bat_charging_data->jeita_temp_below_neg_10_cv_voltage);

}


#ifdef CONFIG_MTK_BATTERY_CVR_SUPPORT
void init_jeita_cv_voltage_from_sysfs(void)
{
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cv_voltage = gFG_CV_Battery_Voltage;
#endif
}
#endif

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct battery_data *data = &battery_main;
	struct mtk_cooler_platform_data *cool_dev = data->cool_dev;
	int i = 0;
	int offset = 0;

	if (!data || !cool_dev)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i+1, cool_dev->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level, state;
	struct battery_data *data = &battery_main;
	struct mtk_cooler_platform_data *cool_dev = data->cool_dev;

	if (!data || !cool_dev)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	cool_dev->levels[state] = level;
	return count;
}
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static int battery_probe(struct platform_device *pdev)
{
	struct class_device *class_dev = NULL;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_debug("******** battery driver probe!! ********\n");

	/* AP:
	 * Use PMIC events as interrupts through kernel IRQ API.
	 */
	atomic_set(&bat_thread_wakeup, 0);

	g_bat.irq = platform_get_irq(pdev, 0);
	if (g_bat.irq <= 0)
		return -EINVAL;

	p_bat_charging_data = (struct mt_battery_charging_custom_data *)dev_get_platdata(dev);
	if (!p_bat_charging_data) {
		pr_err("%s: no platform data. replace with default settings.\n", __func__);
		p_bat_charging_data = &default_charging_data;

		/* populate property here */
		init_charging_data_from_dt(pdev->dev.of_node);
	}

	irq_set_status_flags(g_bat.irq, IRQ_NOAUTOEN);
	ret = request_threaded_irq(g_bat.irq, NULL,
				   ops_chrdet_int_handler,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "ops_mt6397_chrdet", pdev);
	if (ret) {
		pr_err("%s: request_threaded_irq err = %d\n", __func__, ret);
		return ret;
	}

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		battery_log(BAT_LOG_CRTI, "Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		battery_log(BAT_LOG_CRTI, "adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
							 NULL,
							 adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	battery_log(BAT_LOG_CRTI, "[BAT_probe] adc_cali prepare : done !!\n ");

	wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND, "battery suspend wakelock");
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	wake_lock_init(&TA_charger_suspend_lock, WAKE_LOCK_SUSPEND, "TA charger suspend wakelock");
#endif

	battery_main.dock_state.name = "dock";
	battery_main.dock_state.index = 0;
	battery_main.dock_state.state = TYPE_UNDOCKED;
	ret = switch_dev_register(&battery_main.dock_state);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			"[BAT_probe] switch_dev_register dock_state Fail !!\n");
		return ret;
	}

	/* Integrate with Android Battery Service */
	ret = power_supply_register(dev, &ac_main.psy);
	if (ret) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Fail !!\n");
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Success !!\n");

	ret = power_supply_register(dev, &usb_main.psy);
	if (ret) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register USB Fail !!\n");
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register USB Success !!\n");

	battery_main.cool_dev = &cooler;
	ret = power_supply_register(dev, &battery_main.psy);
	if (ret) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register Battery Fail !!\n");
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register Battery Success !!\n");
	device_create_file(&battery_main.psy.tcd->device, &dev_attr_levels);

#if !defined(CONFIG_POWER_EXT)
	/* For EM */
	{
		int ret_device_file = 0;

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Charger_Voltage);

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_0_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_1_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_2_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_3_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_4_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_5_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_6_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_7_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_8_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_9_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_10_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_11_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_12_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_13_Slope);

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_0_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_1_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_2_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_3_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_4_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_5_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_6_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_7_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_8_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_9_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_13_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file = device_create_file(dev, &dev_attr_Power_On_Voltage);
		ret_device_file = device_create_file(dev, &dev_attr_Power_Off_Voltage);
		ret_device_file = device_create_file(dev, &dev_attr_Charger_TopOff_Value);
		ret_device_file = device_create_file(dev, &dev_attr_Charger_Type);
		ret_device_file = device_create_file(dev, &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file = device_create_file(dev, &dev_attr_FG_SW_CoulombCounter);
		ret_device_file = device_create_file(dev, &dev_attr_Charging_CallState);
		ret_device_file = device_create_file(dev, &dev_attr_Charging_Enable);
		ret_device_file = device_create_file(dev, &dev_attr_Custom_Charging_Current);
		ret_device_file = device_create_file(dev, &dev_attr_Custom_PlugIn_Time);
		ret_device_file = device_create_file(dev, &dev_attr_Custom_Charging_Mode);
		ret_device_file = device_create_file(dev, &dev_attr_recharge_counter);
		ret_device_file = device_create_file(dev, &dev_attr_charger_plugin_counter);
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		ret_device_file = device_create_file(dev, &dev_attr_Pump_Express);
#endif
	}

	/* battery_meter_initial();      //move to mt_battery_GetBatteryData() to decrease booting time */

	/* Initialization BMT Struct */
	BMT_status.bat_exist = true;	/* phone must have battery */
	BMT_status.charger_exist = false;	/* for default, no charger */
	BMT_status.bat_vol = 0;
	BMT_status.ICharging = 0;
	BMT_status.temperature = 0;
	BMT_status.charger_vol = 0;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.SOC = 0;
	BMT_status.UI_SOC = -1;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = false;
	BMT_status.bat_full = false;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point = battery_meter_get_battery_nPercent_UI_SOC();
#ifdef CONFIG_MTK_BATTERY_CVR_SUPPORT
	BMT_status.cv_voltage_changed = false;
#endif

	kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	battery_log(BAT_LOG_CRTI, "[battery_probe] bat_thread_kthread Done\n");

	getrawmonotonic(&chr_plug_in_time);
	g_custom_plugin_time = 0;
	g_custom_charging_cv = -1;

	/*LOG System Set */
	init_proc_log();

#endif
	g_bat_init_flag = true;

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_init();
#endif

	mutex_lock(&bat_mutex);
	g_bat.common_init_done = true;
	bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return 0;
}

static void battery_timer_pause(void)
{
	/* pr_notice("******** battery driver suspend!! ********\n" ); */

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_suspend();
#endif
}

static void battery_timer_resume(void)
{

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_resume();
#endif
}

static int battery_remove(struct platform_device *dev)
{
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_uninit();
#endif
	battery_log(BAT_LOG_CRTI, "******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *pdev)
{
#if !defined(CONFIG_POWER_EXT)
	int count = 0;
#endif
	pr_debug("******** battery driver shutdown!! ********\n");

	disable_irq(g_bat.irq);

	mutex_lock(&bat_mutex);
	fg_battery_shutdown = true;
	wake_up_bat_update_meter();

#if !defined(CONFIG_POWER_EXT)
	while (!g_bat.down && count < 5) {
		mutex_unlock(&bat_mutex);
		msleep(20);
		count++;
		mutex_lock(&bat_mutex);
	}

	if (!g_bat.down)
		pr_err("%s: failed to terminate battery thread\n", __func__);
#endif
	/* turn down interrupt thread and wakeup ability */

	if (g_bat.init_done)
		irq_set_irq_wake(g_bat.irq, false);
	free_irq(g_bat.irq, pdev);

	mutex_unlock(&bat_mutex);
}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	disable_irq(g_bat.irq);

	mutex_lock(&bat_mutex);
	battery_suspended = true;
	mutex_unlock(&bat_mutex);

	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	battery_suspended = false;
	g_refresh_ui_soc = true;
	if (bat_charger_is_pcm_timer_trigger())
		wake_up_bat_update_meter();

	enable_irq(g_bat.irq);

	return 0;
}


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Notify API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#if !defined(CONFIG_POWER_EXT)
static ssize_t show_BatteryNotify(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[Battery] show_BatteryNotify : %x\n", g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int reg_BatteryNotifyCode, ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &reg_BatteryNotifyCode);
		if (ret) {
			pr_err("wrong format!\n");
			return size;
		}
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		battery_log(BAT_LOG_CRTI, "[Battery] store code : %x\n", g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664, show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	unsigned int reg_BN_TestMode, ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &reg_BN_TestMode);
		if (ret) {
			pr_err("wrong format!\n");
			return size;
		}
		g_BN_TestMode = reg_BN_TestMode;
		battery_log(BAT_LOG_CRTI, "[Battery] store g_BN_TestMode : %x\n", g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // platform_driver API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#if 0
static int battery_cmd_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p,
		     "g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		     g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		     battery_cmd_thermal_test_mode_value);

	*start = buf + off;

	len = p - buf;
	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}
#endif

static ssize_t battery_cmd_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	int len = 0, bat_tt_enable = 0, bat_thr_test_mode = 0, bat_thr_test_value = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &bat_tt_enable, &bat_thr_test_mode, &bat_thr_test_value) == 3) {
		g_battery_thermal_throttling_flag = bat_tt_enable;
		battery_cmd_thermal_test_mode = bat_thr_test_mode;
		battery_cmd_thermal_test_mode_value = bat_thr_test_value;

		battery_log(BAT_LOG_CRTI,
			    "bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			    g_battery_thermal_throttling_flag,
			    battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

		return count;
	}

	battery_log(BAT_LOG_CRTI,
		    "  bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");

	return -EINVAL;
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		   "=> g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		   g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		   battery_cmd_thermal_test_mode_value);
	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations battery_cmd_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
	.write = battery_cmd_write,
};

static int mt_batteryNotify_probe(struct platform_device *pdev)
{
#if defined(CONFIG_POWER_EXT)
#else
	struct device *dev = &pdev->dev;
	int ret_device_file = 0;
	/* struct proc_dir_entry *entry = NULL; */
	struct proc_dir_entry *battery_dir = NULL;

	battery_log(BAT_LOG_CRTI, "******** mt_batteryNotify_probe!! ********\n");


	ret_device_file = device_create_file(dev, &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(dev, &dev_attr_BN_TestMode);

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		pr_err("[%s]: mkdir /proc/mtk_battery_cmd failed\n", __func__);
	} else {
#if 1
		proc_create("battery_cmd", S_IRUGO | S_IWUSR, battery_dir, &battery_cmd_proc_fops);
		battery_log(BAT_LOG_CRTI, "proc_create battery_cmd_proc_fops\n");
#else
		entry = create_proc_entry("battery_cmd", S_IRUGO | S_IWUSR, battery_dir);
		if (entry) {
			entry->read_proc = battery_cmd_read;
			entry->write_proc = battery_cmd_write;
		}
#endif
	}

	battery_log(BAT_LOG_CRTI, "******** mtk_battery_cmd!! ********\n");
#endif
	return 0;

}

#if 0				/* move to board-common-battery.c */
struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	/* #ifdef CONFIG_PM */
	.suspend = battery_suspend,
	.resume = battery_resume,
	/* #endif */
	.driver = {
		   .name = "battery",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(mt_battery_common_id),
#endif
		   },
};

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery",
	.id = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
	.probe = mt_batteryNotify_probe,
	.driver = {
		   .name = "mt-battery",
		   },
};

static int battery_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
	case PM_RESTORE_PREPARE:	/* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:	/* Hibernation finished */
	case PM_POST_SUSPEND:	/* Suspend finished */
	case PM_POST_RESTORE:	/* Restore failed */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block battery_pm_notifier_block = {
	.notifier_call = battery_pm_event,
	.priority = 0,
};

static int __init battery_init(void)
{
	int ret;

#if 0				/* move to board-common-battery.c */
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_driver] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* battery notofy UI */
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[mt_batteryNotify] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[mt_batteryNotify] Unable to register driver (%d)\n", ret);
		return ret;
	}
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		pr_debug("[%s] failed to register PM notifier %d\n", __func__, ret);

	battery_log(BAT_LOG_CRTI, "****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

static void __exit battery_exit(void)
{
}

/* move to late_initcall to ensure battery_meter probe first */
/* module_init(battery_init); */
late_initcall(battery_init);
module_exit(battery_exit);

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
