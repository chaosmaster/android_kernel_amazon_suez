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
#include "mach/mt_thermal.h"

#define PMIC_SENSOR_NAME "mtktsgpu_sensor"

static int mtktsgpu_read_temp(struct thermal_dev *tdev)
{
	return get_immediate_ts1_wrap();
}

static struct thermal_dev_ops mtktsgpu_sensor_fops = {
	.get_temp = mtktsgpu_read_temp,
};

struct thermal_dev_params mtktsgpu_sensor_tdp = {
	.offset = 0,
	.alpha = 0,
	.weight = 0
};

static ssize_t mtktsgpu_sensor_show_temp(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	int temp = get_immediate_ts1_wrap();
	return sprintf(buf, "%d\n", temp);
}

static ssize_t mtktsgpu_sensor_show_params(struct device *dev,
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

static ssize_t mtktsgpu_sensor_store_params(struct device *dev,
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

static DEVICE_ATTR(params, 0644, mtktsgpu_sensor_show_params, mtktsgpu_sensor_store_params);
static DEVICE_ATTR(temp, 0444, mtktsgpu_sensor_show_temp, NULL);

static int mtktsgpu_probe(struct platform_device *pdev)
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
                tmp103->therm_fw->name = PMIC_SENSOR_NAME;
                tmp103->therm_fw->dev = tmp103->dev;
                tmp103->therm_fw->dev_ops = &mtktsgpu_sensor_fops;
                tmp103->therm_fw->tdp = &mtktsgpu_sensor_tdp;
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

static int mtktsgpu_remove(struct platform_device *pdev)
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

static struct platform_driver mtktsgpu_driver = {
	.probe = mtktsgpu_probe,
	.remove = mtktsgpu_remove,
	.driver     = {
		.name  = PMIC_SENSOR_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktsgpu_device = {
	.name = PMIC_SENSOR_NAME,
	.id = -1,
};

static int __init mtktsgpu_sensor_init(void)
{
	int ret;

	ret = platform_device_register(&mtktsgpu_device);
	if (ret) {
		pr_err("Unable to register mtktsgpu thermal device (%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&mtktsgpu_driver);
	if (ret) {
		pr_err("Unable to register mtktsgpu driver (%d)\n", ret);
		goto err_unreg;
	}
	return 0;

err_unreg:
	platform_device_unregister(&mtktsgpu_device);
	return ret;
}

static void __exit mtktsgpu_sensor_exit(void)
{
	platform_device_unregister(&mtktsgpu_device);
	platform_driver_unregister(&mtktsgpu_driver);
}

module_init(mtktsgpu_sensor_init);
module_exit(mtktsgpu_sensor_exit);
