/*
 *  board-common-tmp103.c  - Common tmp103 thermal board file
 *
 *  Copyright (C) 2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *  Copyright (C) 2015 Akwasi Boateng <boatenga@lab126.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 */

#include <linux/platform_data/mtk_thermal.h>
//#include <mach/charging.h>

#define MIN_CPU_POWER (594)
#define MAX_CPU_POWER (4600)
#define MAX_CHARGING_LIMIT (CHARGE_CURRENT_MAX)
#define MIN_CHARGING_LIMIT (0)
#define COOLERS_COUNT 3
#define MASK (0x0FFF)

static char *coolers[COOLERS_COUNT] = {
	"thermal_budget",
	"lcd-backlight",
	"battery",
};

static int match(struct thermal_zone_device *tz,
	  struct thermal_cooling_device *cdev)
{
	int i;
	const struct thermal_zone_params *tzp = tz->tzp;

	if (strncmp(tz->type, "tmp103", strlen("tmp103")))
		return -EINVAL;

	if (!tzp)
		return -EINVAL;

	for (i = 0; i < tzp->num_tbps; i++) {
		if (tzp->tbp[i].cdev) {
			if (!strcmp(tzp->tbp[i].cdev->type, cdev->type))
				return -EEXIST;
		}
	}

	for (i = 0; i < COOLERS_COUNT; i++) {
		if (!strncmp(cdev->type, coolers[i], strlen(coolers[i])))
			return 0;
	}
	return -EINVAL;
}

static struct thermal_bind_params tbp[] = {
	{.cdev = NULL, .weight = 0, .trip_mask = MASK, .match = match},
	{.cdev = NULL, .weight = 0, .trip_mask = MASK, .match = match},
	{.cdev = NULL, .weight = 0, .trip_mask = MASK, .match = match},
};

static struct mtk_thermal_platform_data tmp103_thermal_data = {
	.num_trips = 7,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.tzp = {
		.governor_name = "tmp103",
		.num_tbps = 3,
		.tbp = tbp
	},
	.trips[0] = {.temp = 44500, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[1] = {.temp = 44900, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[2] = {.temp = 46000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[3] = {.temp = 49000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[4] = {.temp = 52000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[5] = {.temp = 54000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[6] = {.temp = 56000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.trips[7] = {.temp = 46000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.trips[8] = {.temp = 47000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.trips[9] = {.temp = 48000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.trips[10] = {.temp = 49000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.trips[11] = {.temp = 50000, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
	.num_cdevs = 3,
	.cdevs[0] = "thermal_budget",
	.cdevs[1] = "lcd-backlight",
	.cdevs[2] = "battery",
};

static struct platform_device tmp103_cooler_device = {
	.name   = "tmp103-cooling",
	.id     = -1,
	.dev    = {
		.platform_data = NULL,
	},
};

static struct platform_device tmp103_thermal_zone_device = {
	.name   = "tmp103-thermal",
	.id     = -1,
	.dev    = {
		.platform_data = &tmp103_thermal_data,
	},
};

static int __init board_common_tmp103_init(void)
{
	int err;

	err = platform_device_register(&tmp103_cooler_device);
	if (err) {
		pr_err("%s: Failed to register device %s\n", __func__,
			tmp103_cooler_device.name);
		return err;
	}

	err = platform_device_register(&tmp103_thermal_zone_device);
	if (err) {
		pr_err("%s: Failed to register device %s\n", __func__,
		       tmp103_thermal_zone_device.name);
		return err;
	}
	return err;
}

static void __exit board_common_tmp103_exit(void)
{
	platform_device_unregister(&tmp103_cooler_device);
	platform_device_unregister(&tmp103_thermal_zone_device);
}

module_init(board_common_tmp103_init);
module_exit(board_common_tmp103_exit);

MODULE_DESCRIPTION("TMP103 thermal platform driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@lab126.com>");
MODULE_LICENSE("GPL");

