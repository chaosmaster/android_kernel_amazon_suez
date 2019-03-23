#ifndef _MTK_THERMAL_H_
#define _MTK_THERMAL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/notifier.h>
#include <linux/leds.h>
#include <linux/thermal_framework.h>

struct tmp103_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
};

struct mtk_cooler_platform_data {
	char type[THERMAL_NAME_LENGTH];
	unsigned long state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int level;
	int levels[THERMAL_MAX_TRIPS];
};

struct trip_t {
	unsigned long temp;
	enum thermal_trip_type type;
	unsigned long hyst;
};

struct mtk_thermal_platform_data {
	int num_trips;
	enum thermal_device_mode mode;
	int polling_delay;
	struct thermal_zone_params tzp;
	struct trip_t trips[THERMAL_MAX_TRIPS];
	int num_cdevs;
	char cdevs[THERMAL_MAX_TRIPS][THERMAL_NAME_LENGTH];
};

struct mtk_thermal_platform_data_wrapper {
	struct mtk_thermal_platform_data *data;
	struct thermal_dev_params params;
};

void last_kmsg_thermal_shutdown(void);

#endif /* _MTK_THERMAL_H_ */
