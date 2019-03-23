#define DEBUG
#include <linux/delay.h>
#include <linux/slab.h>

#include "battery_dts_support.h"
#include "mt_battery_common.h"

#define DUMP_DATA	0

void batt_meter_parse_node(const struct device_node *np,
				const char *node_srting, int *cust_val)
{
	u32 val;

	if (of_property_read_u32(np, node_srting, &val) == 0) {
		(*cust_val) = (int)val;
	} else {
		pr_debug("Get %s failed\n", node_srting);
	}
}

void batt_meter_parse_temperature_table(const struct device_node *np,
				const char *node_srting,
				struct BATT_TEMPERATURE **profile,
				int saddles)
{
	int addr, val, idx = 0;
	struct BATT_TEMPERATURE *profile_p = NULL;

	if (saddles <= 0) {
		pr_err("Table size is zero\n");
		return;
	}
	if (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		profile_p = kzalloc(sizeof(struct BATT_TEMPERATURE)*saddles, GFP_KERNEL);
		*profile = profile_p;
	} else {
		pr_err("Lost %s table data\n", node_srting);
		return;
	}

	while (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		idx++;
		of_property_read_u32_index(np, node_srting, idx, &val);
		profile_p->BatteryTemp = addr;
		profile_p->TemperatureR = val;

		/* dump parsing data */
		#if DUMP_DATA
		msleep(20);
		pr_debug("%s[%d]: <%d, %d>\n",
				node_srting, (idx/2),
				profile_p->BatteryTemp,
				profile_p->TemperatureR);
		#endif

		profile_p++;
		if ((++idx) >= (saddles * 2))
			break;
	}

	/* use last data to fill with the rest array
				if raw data is less than temp array */
	/* error handle */
	profile_p--;

	while (idx < (saddles * 2)) {
		profile_p++;
		profile_p->BatteryTemp = addr;
		profile_p->TemperatureR = val;
		idx = idx + 2;

		/* dump parsing data */
		#if DUMP_DATA
		msleep(20);
		pr_debug("%s[%d]: <%d, %d>\n",
				node_srting, (idx/2) - 1,
				profile_p->BatteryTemp,
				profile_p->TemperatureR);
		#endif
	}
}
void batt_meter_parse_table(const struct device_node *np,
				const char *node_srting,
				struct BATTERY_PROFILE_STRUCT **profile,
				int saddles)
{
	int addr, val, idx = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p = NULL;

	if (saddles <= 0) {
		pr_err("Table size is zero\n");
		return;
	}
	if (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		profile_p = kzalloc(sizeof(struct BATTERY_PROFILE_STRUCT)*saddles, GFP_KERNEL);
		*profile = profile_p;
	} else {
		pr_err("Lost %s table data\n", node_srting);
		return;
	}

	while (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		idx++;
		of_property_read_u32_index(np, node_srting, idx, &val);
		profile_p->percentage = addr;
		profile_p->voltage = val;

		profile_p++;
		if ((++idx) >= (saddles * 2))
			break;
	}

	profile_p--;

	while (idx < (saddles * 2)) {
		profile_p++;
		profile_p->percentage = addr;
		profile_p->voltage = val;
		idx = idx + 2;
	}
}
void batt_meter_parse_r_profile_table(const struct device_node *np,
				const char *node_srting,
				struct R_PROFILE_STRUCT **profile,
				int saddles)
{
	int addr, val, idx = 0;
	struct R_PROFILE_STRUCT *profile_p = NULL;

	if (saddles <= 0) {
		pr_err("Table size is zero\n");
		return;
	}
	if (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		profile_p = kzalloc(sizeof(struct R_PROFILE_STRUCT)*saddles, GFP_KERNEL);
		*profile = profile_p;
	} else {
		pr_err("Lost %s table data\n", node_srting);
		return;
	}

	while (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		idx++;
		of_property_read_u32_index(np, node_srting, idx, &val);
		profile_p->resistance = addr;
		profile_p->voltage = val;
		profile_p++;
		if ((++idx) >= (saddles * 2))
			break;
	}

	profile_p--;

	while (idx < (saddles * 2)) {
		profile_p++;
		profile_p->resistance = addr;
		profile_p->voltage = val;
		idx = idx + 2;
	}
}

int batt_meter_init_cust_data_from_dt(struct device_node *np,
		struct mt_battery_meter_custom_data *cust_data)
{
	int num = 0;
	int update_table = 0;

	if (!np) {
		pr_err("Not a valid device-tree node\n");
		return -ENODEV;
	}

	batt_meter_parse_node(np, "car_tune_value",
		&cust_data->car_tune_value);

	batt_meter_parse_node(np, "rbat_pull_up_r",
		&cust_data->rbat_pull_up_r);
	batt_meter_parse_node(np, "rbat_pull_down_r",
		&cust_data->rbat_pull_down_r);
	batt_meter_parse_node(np, "rbat_pull_up_volt",
		&cust_data->rbat_pull_up_volt);
	batt_meter_parse_node(np, "cust_r_fg_offset",
		&cust_data->cust_r_fg_offset);

	batt_meter_parse_node(np, "enable_ocv2cv_trans",
		&cust_data->enable_ocv2cv_trans);
	batt_meter_parse_node(np, "step_of_qmax",
		&cust_data->step_of_qmax);
	batt_meter_parse_node(np, "cv_current",
		&cust_data->cv_current);

	batt_meter_parse_node(np, "poweron_low_capacity_tolerance",
		&cust_data->poweron_low_capacity_tolerance);

	batt_meter_parse_node(np, "temperature_t0",
		&cust_data->temperature_t0);
	batt_meter_parse_node(np, "temperature_t1",
		&cust_data->tempearture_t1);
	batt_meter_parse_node(np, "temperature_t1_5",
		&cust_data->temperature_t1_5);
	batt_meter_parse_node(np, "temperature_t2",
		&cust_data->temperature_t2);
	batt_meter_parse_node(np, "temperature_t3",
		&cust_data->temperature_t3);

	batt_meter_parse_node(np, "q_max_pos_50",
		&cust_data->q_max_pos_50);
	batt_meter_parse_node(np, "q_max_pos_25",
		&cust_data->q_max_pos_25);
	batt_meter_parse_node(np, "q_max_pos_10",
		&cust_data->q_max_pos_10);
	batt_meter_parse_node(np, "q_max_pos_0",
		&cust_data->q_max_pos_0);
	batt_meter_parse_node(np, "q_max_neg_10",
		&cust_data->q_max_neg_10);

	batt_meter_parse_node(np, "q_max_pos_50_h_current",
		&cust_data->q_max_pos_50_h_current);
	batt_meter_parse_node(np, "q_max_pos_25_h_current",
		&cust_data->q_max_pos_25_h_current);
	batt_meter_parse_node(np, "q_max_pos_10_h_current",
		&cust_data->q_max_pos_10_h_current);
	batt_meter_parse_node(np, "q_max_pos_0_h_current",
		&cust_data->q_max_pos_0_h_current);

	batt_meter_parse_node(np, "fg_meter_resistance",
		&cust_data->fg_meter_resistance);

	batt_meter_parse_node(np, "update_table",
		&update_table);
	if (update_table) {
		batt_meter_parse_node(np, "batt_temperature_table_num", &num);
		batt_meter_parse_temperature_table(np, "batt_temperature_table",
			(struct BATT_TEMPERATURE **) &cust_data->p_batt_temperature_table,
			num);
		cust_data->battery_ntc_table_saddles = num;

		batt_meter_parse_node(np, "battery_profile_t0_num", &num);
		batt_meter_parse_table(np, "battery_profile_t0",
			(struct BATTERY_PROFILE_STRUCT **) &cust_data->p_battery_profile_t0,
			num);

		batt_meter_parse_node(np, "battery_profile_t1_num", &num);
		batt_meter_parse_table(np, "battery_profile_t1",
			(struct BATTERY_PROFILE_STRUCT **) &cust_data->p_battery_profile_t1,
			num);

		num = 0;
		batt_meter_parse_node(np, "battery_profile_t1_5_num", &num);
		batt_meter_parse_table(np, "battery_profile_t1_5",
			(struct BATTERY_PROFILE_STRUCT **) &cust_data->p_battery_profile_t1_5,
			num);

		batt_meter_parse_node(np, "battery_profile_t2_num", &num);
		batt_meter_parse_table(np, "battery_profile_t2",
			(struct BATTERY_PROFILE_STRUCT **) &cust_data->p_battery_profile_t2,
			num);

		batt_meter_parse_node(np, "battery_profile_t3_num", &num);
		batt_meter_parse_table(np, "battery_profile_t3",
			(struct BATTERY_PROFILE_STRUCT **) &cust_data->p_battery_profile_t3,
			num);

		cust_data->battery_profile_saddles = num;
		cust_data->p_battery_profile_temperature =
			kzalloc(sizeof(struct BATTERY_PROFILE_STRUCT)*num, GFP_KERNEL);


		batt_meter_parse_node(np, "r_profile_t0_num",	&num);
		batt_meter_parse_r_profile_table(np, "r_profile_t0",
			(struct R_PROFILE_STRUCT **) &cust_data->p_r_profile_t0,
			num);

		batt_meter_parse_node(np, "r_profile_t1_num",	&num);
		batt_meter_parse_r_profile_table(np, "r_profile_t1",
			(struct R_PROFILE_STRUCT **) &cust_data->p_r_profile_t1,
			num);

		num = 0;
		batt_meter_parse_node(np, "r_profile_t1_5_num",	&num);
		batt_meter_parse_r_profile_table(np, "r_profile_t1_5",
			(struct R_PROFILE_STRUCT **) &cust_data->p_r_profile_t1_5,
			num);

		batt_meter_parse_node(np, "r_profile_t2_num",	&num);
		batt_meter_parse_r_profile_table(np, "r_profile_t2",
			(struct R_PROFILE_STRUCT **) &cust_data->p_r_profile_t2,
			num);

		batt_meter_parse_node(np, "r_profile_t3_num",	&num);
		batt_meter_parse_r_profile_table(np, "r_profile_t3",
			(struct R_PROFILE_STRUCT **) &cust_data->p_r_profile_t3,
			num);
		cust_data->battery_r_profile_saddles = num;
		cust_data->p_r_profile_temperature =
			kzalloc(sizeof(struct R_PROFILE_STRUCT)*num, GFP_KERNEL);
	}

	return 0;
}
