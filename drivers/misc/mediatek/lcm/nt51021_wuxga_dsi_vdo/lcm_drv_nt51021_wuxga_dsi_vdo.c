#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/device.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#endif

#include "nt51021_wuxga_dsi_vdo.h"

#define LCM_DBG(fmt, arg...) \
	printk("[LCM-NT51021-WUXGA-DSI-VDO] %s (line:%d) :" fmt "\r\n", __func__, __LINE__, ## arg)

static struct regulator *lcm_vgp;

/* get(vgp6) LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret = 0;
	struct regulator *lcm_vgp_ldo;

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		LCM_DBG("failed to get reg-lcm LDO, %d", ret);
		return ret;
	}

	LCM_DBG("Get supply ok");

	/* get current voltage settings */
	LCM_DBG("lcm LDO voltage = %duV in LK stage", regulator_get_voltage(lcm_vgp_ldo));

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	if (NULL == lcm_vgp)
		return 0;

	/* set(vgp6) voltage to 3.3V */
	ret = regulator_set_voltage(lcm_vgp, 3300000, 3300000);
	if (ret != 0) {
		LCM_DBG("lcm failed to set lcm_vgp voltage: %d", ret);
		return ret;
	}

	/* get(vgp6) voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 3300000)
		LCM_DBG("Check regulator voltage=3300000 pass!");
	else
		LCM_DBG("Check regulator voltage=3300000 fail! (voltage: %d)", volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		LCM_DBG("Failed to enable lcm_vgp: %d", ret);
		return ret;
	}
	LCM_DBG("Succeed to enable lcm_vgp: %d", ret);
	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (NULL == lcm_vgp)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	LCM_DBG("lcm query regulator enable status[0x%d]", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			LCM_DBG("lcm failed to disable lcm_vgp: %d", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			LCM_DBG("lcm regulator disable pass");
	}

	return ret;
}

static int lcm_request_gpio_control(struct device *dev)
{
	int ret = 0;

	GPIO_LCD_RST_EN = of_get_named_gpio(dev->of_node, "gpio_lcm_rst_en", 0);

	ret = gpio_request(GPIO_LCD_RST_EN, "GPIO_LCD_RST_EN");
	if (ret) {
		LCM_DBG("gpio request GPIO_LCD_RST_EN = 0x%x fail with %d", GPIO_LCD_RST_EN, ret);
		goto out;
	}

	LCM_ID1_GPIO = of_get_named_gpio(dev->of_node, "lcm_id1_gpio", 0);

	ret = gpio_request(LCM_ID1_GPIO, "LCM_ID1_GPIO");
	if (ret) {
		LCM_DBG("gpio request LCM_ID1_GPIO = 0x%x fail with %d", LCM_ID1_GPIO, ret);
		goto out;
	}

	LCM_ID0_GPIO = of_get_named_gpio(dev->of_node, "lcm_id0_gpio", 0);

	ret = gpio_request(LCM_ID0_GPIO, "LCM_ID0_GPIO");
	if (ret) {
		LCM_DBG("gpio request LCM_ID0_GPIO = 0x%x fail with %d", LCM_ID0_GPIO, ret);
		goto out;
	}

out:
	return ret;
}

static int lcm_probe(struct device *dev)
{
	int ret;

	ret = lcm_request_gpio_control(dev);
	if (ret)
		return ret;

	ret = lcm_get_vgp_supply(dev);
	if (ret)
		return ret;

	ret = lcm_vgp_supply_enable();
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "mediatek,mt8173-lcm",},
	{}
};

static struct platform_driver lcm_driver = {
	.driver = {
		   .name = "nt51021_wuxga_dsi_vdo",
		   .owner = THIS_MODULE,
		   .probe = lcm_probe,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_init(void)
{
	LCM_DBG("Register panel driver for nt51021_wuxga_dsi_vdo");
	if (platform_driver_register(&lcm_driver)) {
		LCM_DBG("Failed to register this driver!");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	LCM_DBG("Unregister this driver done");
}

late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("Compal");
MODULE_DESCRIPTION("LCM display subsystem driver");
MODULE_LICENSE("GPL");
