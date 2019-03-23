#ifndef __BATTERY_DTS_SUPPORT_H__
#define __BATTERY_DTS_SUPPORT_H__
#include <linux/device.h>
#include <linux/of.h>

#include "mt_battery_custom_data.h"

int batt_meter_init_cust_data_from_dt(struct device_node *np,
		struct mt_battery_meter_custom_data *cust_data);
#endif /* End __BATTERY_DTS_SUPPORT_H__ */
