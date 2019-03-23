/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/i2c.h>

#include <linux/input/tmp103_temp_sensor.h>
#include <linux/thermal_framework.h>
#include "inc/mtk_ts_bts.h"
#include "mach/mt_thermal.h"

#define VIRTUAL_SENSOR_THERMISTOR_NAME "virtual_sensor_thermistor"

static int mtktsthermistor_read_temp(struct thermal_dev *tdev)
{
        struct device_node *node = tdev->dev->of_node;
	int aux_channel;

        of_property_read_u32(node, "aux_channel_num",
                                        &aux_channel);
	return mtkts_bts_get_hw_temp(aux_channel);
}

static struct thermal_dev_ops mtktsthermistor_sensor_fops = {
	.get_temp = mtktsthermistor_read_temp,
};

static ssize_t mtktsthermistor_sensor_show_temp(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
        struct device_node *node = dev->of_node;
	int aux_channel, temp;

        of_property_read_u32(node, "aux_channel_num",
                                        &aux_channel);
	temp = mtkts_bts_get_hw_temp(aux_channel);
	return sprintf(buf, "%d\n", temp);
}

static ssize_t mtktsthermistor_sensor_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tmp103_temp_sensor *tmp103 = platform_get_drvdata(pdev);

	if (!tmp103)
		return -EINVAL;

	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tmp103->therm_fw->tdp->offset,
		       tmp103->therm_fw->tdp->alpha,
		       tmp103->therm_fw->tdp->weight);
}

static ssize_t mtktsthermistor_sensor_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct tmp103_temp_sensor *tmp103 = platform_get_drvdata(pdev);

	if (!tmp103)
		return -EINVAL;

	if (sscanf(buf, "%19s %d", param, &value) == 2) {
		if (!strcmp(param, "offset"))
			tmp103->therm_fw->tdp->offset = value;
		if (!strcmp(param, "alpha"))
			tmp103->therm_fw->tdp->alpha = value;
		if (!strcmp(param, "weight"))
			tmp103->therm_fw->tdp->weight = value;
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(params, 0644, mtktsthermistor_sensor_show_params, mtktsthermistor_sensor_store_params);
static DEVICE_ATTR(temp, 0444, mtktsthermistor_sensor_show_temp, NULL);

struct thermal_dev_params *thermistor_dt_to_params(
                                struct device *dev)
{
        struct device_node *node = dev->of_node;
        struct thermal_dev_params *params;

        params = devm_kzalloc(dev, sizeof(*params), GFP_KERNEL);
        if (!params) {
                dev_err(dev, "unable to allocate params data\n");
                return NULL;
        }

        of_property_read_u32(node, "thermistor,offset",
                                        &params->offset);
	if (of_property_match_string (node,
			"thermistor,offset.sign", "plus") < 0)
		params->offset = 0 - params->offset;

        of_property_read_u32(node, "thermistor,alpha",
                                        &params->alpha);
        of_property_read_u32(node, "thermistor,weight",
                                        &params->weight);
	if (of_property_match_string (node,
			"thermistor,weight.sign", "plus") < 0)
		params->weight = 0 - params->weight;

        return params;
}

static int mtktsthermistor_probe(struct platform_device *pdev)
{
	struct tmp103_temp_sensor *tmp103;
	int ret = 0;

        tmp103 = kzalloc(sizeof(struct tmp103_temp_sensor), GFP_KERNEL);
        if (!tmp103)
                return -ENOMEM;

	mutex_init(&tmp103->sensor_mutex);
	tmp103->dev = &pdev->dev;
	platform_set_drvdata(pdev, tmp103);

        tmp103->last_update = jiffies - HZ;

        tmp103->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
        if (tmp103->therm_fw) {
                tmp103->therm_fw->name = VIRTUAL_SENSOR_THERMISTOR_NAME;
                tmp103->therm_fw->dev = tmp103->dev;
                tmp103->therm_fw->dev_ops = &mtktsthermistor_sensor_fops;
                tmp103->therm_fw->tdp = thermistor_dt_to_params(&pdev->dev);
#ifdef CONFIG_TMP103_THERMAL
                ret = thermal_dev_register(tmp103->therm_fw);
                if (ret) {
                        dev_err(&pdev->dev, "error registering therml device\n");
			return -EINVAL;
                }
#endif
        } else {
                ret = -ENOMEM;
                goto therm_fw_alloc_err;
        }

	ret = device_create_file(&pdev->dev, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	ret = device_create_file(&pdev->dev, &dev_attr_temp);
	if (ret)
		pr_err("%s Failed to create temp attr\n", __func__);

	return 0;

therm_fw_alloc_err:
        mutex_destroy(&tmp103->sensor_mutex);
        kfree(tmp103);
        return ret;
}

static int mtktsthermistor_remove(struct platform_device *pdev)
{
	struct tmp103_temp_sensor *tmp103 = platform_get_drvdata(pdev);

        if (tmp103->therm_fw)
		kfree(tmp103->therm_fw);
        if (tmp103)
		kfree(tmp103);

	device_remove_file(&pdev->dev, &dev_attr_params);
	device_remove_file(&pdev->dev, &dev_attr_temp);
	return 0;
}

static struct of_device_id thermistor_of_match[] = {
        {.compatible = "amazon,virtual_sensor_thermistor", },
        { },
};

static struct platform_driver mtktsthermistor_driver = {
	.probe = mtktsthermistor_probe,
	.remove = mtktsthermistor_remove,
	.driver     = {
		.name  = VIRTUAL_SENSOR_THERMISTOR_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
                   .of_match_table = thermistor_of_match,
#endif
	},
};

static int __init mtktsthermistor_sensor_init(void)
{
	int ret;

	ret = platform_driver_register(&mtktsthermistor_driver);
	if (ret) {
		pr_err("Unable to register mtktsthermistor driver (%d)\n", ret);
	}
	return 0;
}

static void __exit mtktsthermistor_sensor_exit(void)
{
	platform_driver_unregister(&mtktsthermistor_driver);
}

module_init(mtktsthermistor_sensor_init);
module_exit(mtktsthermistor_sensor_exit);
