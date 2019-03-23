
/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
/* #define DEBUG */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/lp855x.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_data/mtk_thermal.h>

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mt_boot.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* LP8550/1/2/3/6 Registers */
#define LP855X_BRIGHTNESS_CTRL		0x00
#define LP855X_DEVICE_CTRL		0x01
#define LP855X_EEPROM_START		0xA0
#define LP855X_EEPROM_END		0xA7
#define LP8556_EPROM_START		0xA0
#define LP8556_EPROM_END		0xAF

/* LP8557 Registers */
#define LP8557_BL_CMD			0x00
#define LP8557_BL_MASK			0x01
#define LP8557_BL_ON			0x01
#define LP8557_BL_OFF			0x00
#define LP8557_BRIGHTNESS_CTRL		0x04
#define LP8557_CURRENT			0x11
#define LP8557_CONFIG			0x10
#define LP8557_EPROM_START		0x10
#define LP8557_EPROM_END		0x1E

#define BUF_SIZE		20
#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255

static int brightness_limit_cmdline = 255;
static unsigned char led_maxcurr_cmdline;
static int load_switch_support;

struct lp855x;

struct lp855x_device_config {
	u8 reg_brightness;
	u8 reg_devicectrl;
	int (*pre_init_device)(struct lp855x *);
	int (*post_init_device)(struct lp855x *);
};

struct  lp855x_led_data {
	struct led_classdev bl;
	struct lp855x *lp;
};
struct lp855x {
	const char *chipname;
	enum lp855x_chip_id chip_id;
	struct lp855x_device_config *cfg;
	struct i2c_client *client;
	struct lp855x_led_data *led_data;
	struct device *dev;
	struct lp855x_platform_data *pdata;
	u8 last_brightness;
	bool need_init;
	struct mutex bl_update_lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mtk_cooler_platform_data cool_dev;
};

static struct mtk_cooler_platform_data cooler = {
	.type = "lcd-backlight",
	.state = 0,
	.max_state = THERMAL_MAX_TRIPS,
	.level = 0,
	.levels = {
		255, 255, 255,
		255, 255, 255,
		175, 175, 175,
		175, 175, 175
	},
};

#ifdef CONFIG_HAS_EARLYSUSPEND
	static void lp855x_early_suspend(struct early_suspend *es);
	static void lp855x_late_resume(struct early_suspend *es);
#endif

static int lp855x_write_byte(struct lp855x *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static int lp855x_read_byte(struct lp855x *lp, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

static int lp855x_update_bit(struct lp855x *lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return lp855x_write_byte(lp, reg, tmp);
}

static int __init setup_lp855x_brightness_limit(char *str)
{
	int bl_limit;

	if (get_option(&str, &bl_limit))
		brightness_limit_cmdline = bl_limit;
	return 0;
}
__setup("lp855x_bl_limit=", setup_lp855x_brightness_limit);

static int __init setup_lp855x_led_maxcurr(char *str)
{
	int maxcurr;

	if (get_option(&str, &maxcurr))
		led_maxcurr_cmdline = (unsigned char)maxcurr;
	return 0;
}
__setup("lp855x_maxcurr=", setup_lp855x_led_maxcurr);

static bool lp855x_is_valid_rom_area(struct lp855x *lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		dev_dbg(lp->dev, "Define LP855x rom area\n");
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		dev_dbg(lp->dev, "Define LP8556 rom area\n");
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	case LP8557:
		dev_dbg(lp->dev, "Define LP8557 rom area\n");
		start = LP8557_EPROM_START;
		end = LP8557_EPROM_END;
		break;
	default:
		return false;
	}

	return (addr >= start && addr <= end);
}

static int lp855x_check_init_device(struct lp855x *lp)
{
	struct lp855x_platform_data *pd = lp->pdata;
	int i, ret = 0;
	u8 addr, val;

	if (lp->need_init) {
		/*Check device control reg first*/
		ret = lp855x_read_byte(lp, lp->cfg->reg_devicectrl, &val);
		if (ret)
			goto err;

		if (val == pd->device_control) {
			lp->need_init = false;
			if (pd->load_new_rom_data && pd->size_program) {
				for (i = 0; i < pd->size_program; i++) {
					addr = pd->rom_data[i].addr;

					if (!lp855x_is_valid_rom_area(lp, addr))
						continue;

					ret = lp855x_read_byte(lp, addr, &val);
					if (ret)
						goto err;

					dev_dbg(lp->dev,
							"Check rom data 0x%02X 0x%02X\n",
							addr,
							val);

					if (val != pd->rom_data[i].val) {
						lp->need_init = true;
						break;
					}

				}
			}
		}
	}
	return 0;

err:
	return ret;
}

static int lp855x_init_device(struct lp855x *lp)
{
	struct lp855x_platform_data *pd = lp->pdata;
	int i;
	int ret = 1;
	u8 val;
	u8 addr;

	if (load_switch_support) {
		if (pd->gpio_en == -1)
			goto err;
		gpio_direction_output(pd->gpio_en, 1);
		msleep(20);
	}

	dev_dbg(lp->dev, "Power on LP855X\n");

	ret = lp855x_check_init_device(lp);
	if (ret)
		goto err;

	if (lp->need_init) {
		if (lp->cfg->pre_init_device) {
			ret = lp->cfg->pre_init_device(lp);
			if (ret) {
				dev_err(lp->dev,
				"pre init device err: %d\n",
				ret);
				goto err;
			}
		}

		val = pd->device_control;
		dev_dbg(lp->dev,
				"Set %s configuration 0x%02x: 0x%02x\n",
				(lp->cfg->reg_devicectrl == LP8557_CONFIG)
				? "LP8557" : "LP8550TO6",
				lp->cfg->reg_devicectrl,
				val);
		ret = lp855x_write_byte(lp, lp->cfg->reg_devicectrl, val);
		if (ret)
			goto err;

		if (pd->load_new_rom_data && pd->size_program) {
			for (i = 0; i < pd->size_program; i++) {
				addr = pd->rom_data[i].addr;
				val = pd->rom_data[i].val;
				if (!lp855x_is_valid_rom_area(lp, addr))
					continue;
				dev_dbg(lp->dev,
						"Load new rom data 0x%02X 0x%02X\n",
						addr,
						val);
				printk("[KE/BL] %s Load new rom data 0x%02X 0x%02X\n",
						__func__,
						addr,
						val);
				ret = lp855x_write_byte(lp, addr, val);
				if (ret)
					goto err;
			}
		}
	}
	/* T4 > 200ms, T4: latency between HS data sending and backlight on */
	msleep(30);
	if (lp->cfg->post_init_device) {
		ret = lp->cfg->post_init_device(lp);
		if (ret) {
			dev_err(lp->dev, "post init device err: %d\n", ret);
			goto err;
		}
	}

err:
	return ret;
}

static int lp8557_bl_off(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 0 before updating EPROM settings\n");
	/* BL_ON = 0 before updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_OFF);
}

static int lp8557_bl_on(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 1 after updating EPROM settings\n");
	/* BL_ON = 1 after updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_ON);
}

static struct lp855x_device_config lp8550to6_cfg = {
	.reg_brightness = LP855X_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP855X_DEVICE_CTRL,
};

static struct lp855x_device_config lp8557_cfg = {
	.reg_brightness = LP8557_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP8557_CONFIG,
	.pre_init_device = lp8557_bl_off,
	.post_init_device = lp8557_bl_on,
};

static int lp855x_configure(struct lp855x *lp)
{
	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
	case LP8556:
	   dev_dbg(lp->dev, "Load lp8550to6 configuration\n");
		lp->cfg = &lp8550to6_cfg;
		break;
	case LP8557:
	   dev_dbg(lp->dev, "Load lp8557 configuration\n");
		lp->cfg = &lp8557_cfg;
		break;
	default:
		return -EINVAL;
	}

	lp->need_init = true;

	return lp855x_init_device(lp);
}

static void _lp855x_led_set_brightness(
			struct lp855x *lp,
			enum led_brightness brightness)
{
	struct led_classdev *bl = &(lp->led_data->bl);
	enum lp855x_brightness_ctrl_mode mode = lp->pdata->mode;
	int gpio_en = lp->pdata->gpio_en;
	u8 last_brightness = lp->last_brightness;
	int ret;

	dev_dbg(lp->dev, "%s (last_br=%d new_br=%d)\n", __func__,
				last_brightness, brightness);

	if ((!last_brightness) && (brightness)) {
#if 0 /* TODO: Add synchronization with panel */
		/* Power Up */
		/* wait until LCD is panel is ready */
		wait_event_timeout(panel_init_queue, nt51012_panel_enabled,
						msecs_to_jiffies(500));
#endif

		/* last_brightness is zero, it means LP8557 ever cut power off,
		 * so need to re-init device.
		 */
		lp->need_init = true; /* force init sequence, rework required */
		ret = lp855x_init_device(lp);
		if (ret)
			goto exit;
		dev_dbg(lp->dev, "BL power up\n");
	} else if (last_brightness == brightness) {
		dev_dbg(lp->dev, "BL nothing to do\n");
		/*nothing to do*/
		goto exit;
	}

	/* make changes */
	if (mode == PWM_BASED) {
		struct lp855x_pwm_data *pd = &lp->pdata->pwm_data;
		int br = brightness;
		int max_br = bl->max_brightness;

		if (pd->pwm_set_intensity)
			pd->pwm_set_intensity(br, max_br);

	} else if (mode == REGISTER_BASED) {
		u8 val = brightness;
		if (brightness_limit_cmdline != 255)
			val = brightness * brightness_limit_cmdline / bl->max_brightness;
		dev_dbg(lp->dev, "Re-mapping brightness is %d\n", val);
		lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	}

	/* power down if required */
	if ((last_brightness) &&  (!brightness)) {
		lp8557_bl_off(lp);
		/* Power Down */
		if (load_switch_support)
			gpio_set_value(gpio_en, 0);

		/*backlight is off, so wake up lcd disable */

#if 0 /* TODO: Add notification mechanism to the panel */
		lp855x_bl_off  = 1;
		wake_up(&panel_fini_queue);
#endif
		dev_dbg(lp->dev, "BL power down\n");
		/* T7 > 200ms, T7: latency between backlight off and HS data stopped */
		msleep(120);
	}

	lp->last_brightness =  brightness;
	bl->brightness      =  brightness;
exit:
	return;
}

void lp855x_led_set_brightness(
		struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	struct lp855x_led_data *led_data = container_of(led_cdev,
			struct lp855x_led_data, bl);

	struct lp855x *lp = led_data->lp;
	unsigned long state;

	mutex_lock(&(lp->bl_update_lock));

	state = lp->cool_dev.state;

	/* Record the latest value from userspace */
	lp->cool_dev.level = brightness;

	if (state) {
		if (brightness > lp->cool_dev.levels[state - 1]) {
			/*
			 * LED core set led_cdev->brightness already before reaching here.
			 *
			 * We set it as the actual/throttled value.
			 * Otherwise, the one read from sys node will NOT be real.
			 */
			led_cdev->brightness = lp->cool_dev.levels[state - 1];
			/* When brightness is larger than cooling level, cannot just skip this case,
			 * double-check last_brightness, if it is zero, set brightness is needed.
			 */
			if (lp->last_brightness)
				goto out;
			else
				brightness = lp->cool_dev.levels[state - 1];
		}
	}
	_lp855x_led_set_brightness(lp, brightness);

out:
	mutex_unlock(&(lp->bl_update_lock));
}
EXPORT_SYMBOL(lp855x_led_set_brightness);

static enum led_brightness _lp855x_led_get_brightness(struct lp855x *lp)
{
	struct led_classdev *bl = &(lp->led_data->bl);

	return bl->brightness;
}

enum led_brightness lp855x_led_get_brightness(struct led_classdev *led_cdev)
{
	struct lp855x_led_data *led_data = container_of(led_cdev,
			struct lp855x_led_data, bl);
	struct lp855x *lp = led_data->lp;
	enum led_brightness ret = 0;

	mutex_lock(&(lp->bl_update_lock));

	ret = _lp855x_led_get_brightness(lp);

	mutex_unlock(&(lp->bl_update_lock));

	return ret;
}
EXPORT_SYMBOL(lp855x_led_get_brightness);

static int lp855x_backlight_register(struct lp855x *lp)
{
	int ret;

	struct lp855x_led_data *led_data;
	struct led_classdev *bl;

	struct lp855x_platform_data *pdata = lp->pdata;

	char *name = pdata->name ? : DEFAULT_BL_NAME;

	led_data = kzalloc(sizeof(struct lp855x_led_data), GFP_KERNEL);

	if (led_data == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	bl = &(led_data->bl);

	bl->name = name;
	bl->max_brightness = MAX_BRIGHTNESS;
	bl->brightness_set = lp855x_led_set_brightness;
	bl->brightness_get = lp855x_led_get_brightness;
	led_data->lp = lp;
	lp->led_data = led_data;

	if (pdata->initial_brightness > bl->max_brightness)
		pdata->initial_brightness = bl->max_brightness;

	bl->brightness = pdata->initial_brightness;

	ret = led_classdev_register(lp->dev, bl);
	if (ret < 0) {
		lp->led_data = NULL;
		kfree(led_data);
		return ret;
	}

	return 0;
}

static void lp855x_backlight_unregister(struct lp855x *lp)
{
	if (lp->led_data) {
		led_classdev_unregister(&(lp->led_data->bl));
		kfree(lp->led_data);
		lp->led_data = NULL;
	}
}

static ssize_t lp855x_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	return scnprintf(buf, BUF_SIZE, "%s\n", lp->chipname);
}

static ssize_t lp855x_get_bl_ctl_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	enum lp855x_brightness_ctrl_mode mode = lp->pdata->mode;
	char *strmode = NULL;

	if (mode == PWM_BASED)
		strmode = "pwm based";
	else if (mode == REGISTER_BASED)
		strmode = "register based";

	return scnprintf(buf, BUF_SIZE, "%s\n", strmode);
}

static int lp855x_get_max_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct lp855x *lp = cdev->devdata;
	*state = lp->cool_dev.max_state;
	return 0;
}

static int lp855x_get_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct lp855x *lp = cdev->devdata;
	*state = lp->cool_dev.state;
	return 0;
}

static int lp855x_set_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long state)
{
	struct lp855x *lp = cdev->devdata;
	int level;
	unsigned long max_state;

	if (!lp)
		return 0;

	mutex_lock(&(lp->bl_update_lock));

	if (!lp->cool_dev.state)
		lp->cool_dev.level = lp->last_brightness;

	if (lp->cool_dev.state == state)
		goto out;

	max_state = lp->cool_dev.max_state;
	lp->cool_dev.state = (state > max_state) ? max_state : state;

	if (!lp->cool_dev.state)
		level = lp->cool_dev.level;
	else {
		level = lp->cool_dev.levels[lp->cool_dev.state - 1];
		if (level > lp->cool_dev.level)
			goto out;
	}
	_lp855x_led_set_brightness(lp, level);

out:
	mutex_unlock(&(lp->bl_update_lock));

	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct lp855x *lp = cdev->devdata;
	struct mtk_cooler_platform_data *cool_dev = &(lp->cool_dev);
	int i;
	int offset = 0;

	if (!lp)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i+1, cool_dev->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct lp855x *lp = cdev->devdata;
	struct mtk_cooler_platform_data *cool_dev = &(lp->cool_dev);

	if (!lp)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	cool_dev->levels[state] = level;
	return count;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = lp855x_get_max_state,
	.get_cur_state = lp855x_get_cur_state,
	.set_cur_state = lp855x_set_cur_state,
};

static DEVICE_ATTR(chip_id, S_IRUGO, lp855x_get_chip_id, NULL);
static DEVICE_ATTR(bl_ctl_mode, S_IRUGO, lp855x_get_bl_ctl_mode, NULL);
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_bl_ctl_mode.attr,
	NULL,
};

static const struct attribute_group lp855x_attr_group = {
	.attrs = lp855x_attributes,
};

#ifdef CONFIG_OF
static int lp855x_parse_dt(struct device *dev, struct device_node *node)
{
	struct lp855x_platform_data *pdata;
	static struct lp855x_rom_data lp8557_eeprom[6];
	/* int rom_length; */
	int mode;
	u8 val;
	int i;

	if (!node) {
		dev_err(dev, "no platform data\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	of_property_read_string(node, "bl-name", (const char **)&pdata->name);
	of_property_read_u8(node, "dev-ctrl", &pdata->device_control);
	of_property_read_u8(node, "init-brt", &pdata->initial_brightness);

	of_property_read_u32(node, "is_load-switch_supported", &load_switch_support);
	pdata->gpio_en = of_get_named_gpio(node, "gpio_en", 0);

	of_property_read_u32(node, "mode", &mode);
	pdata->mode = mode ? REGISTER_BASED : PWM_BASED;
	pdata->load_new_rom_data = 1;
	pdata->size_program = ARRAY_SIZE(lp8557_eeprom);
	pdata->rom_data = lp8557_eeprom;
	if(of_property_read_u8_array(node, "eeprom_data", (u8*)lp8557_eeprom, 12))
		pdata->load_new_rom_data = 0;

	/* Fill ROM platform data if defined */
	/*rom_length = of_get_child_count(node);
	if (rom_length > 0) {
		struct lp855x_rom_data *rom;
		struct device_node *child;
		int i = 0;

		rom = devm_kzalloc(dev, sizeof(*rom) * rom_length, GFP_KERNEL);
		if (!rom)
			return -ENOMEM;

		for_each_child_of_node(node, child) {
			of_property_read_u8(child, "rom-addr", &rom[i].addr);
			of_property_read_u8(child, "rom-val", &rom[i].val);
			i++;
		}

		pdata->size_program = rom_length;
		pdata->rom_data = &rom[0];
	}*/

	if (pdata->load_new_rom_data && led_maxcurr_cmdline) {
		for (i = 0; i < pdata->size_program; i++) {
			if (pdata->rom_data[i].addr == LP8557_CURRENT) {
				val = pdata->rom_data[i].val;
				switch (led_maxcurr_cmdline) {
				case 5:
					val = (val & 0xF8);
					break;
				case 10:
					val = (val & 0xF8) | 0x01;
					break;
				case 13:
					val = (val & 0xF8) | 0x02;
					break;
				case 15:
					val = (val & 0xF8) | 0x03;
					break;
				case 18:
					val = (val & 0xF8) | 0x04;
					break;
				case 20:
					val = (val & 0xF8) | 0x05;
					break;
				case 23:
					val = (val & 0xF8) | 0x06;
					break;
				case 25:
					val = (val & 0xF8) | 0x07;
					break;
				}
				pdata->rom_data[i].val = val;

			}
		}
	}

	dev->platform_data = pdata;

	return 0;
}
#else
static int lp855x_parse_dt(struct device *dev, struct device_node *node)
{
	return -EINVAL;
}
#endif

static int lp855x_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp855x *lp;
	struct lp855x_platform_data *pdata = cl->dev.platform_data;
	struct device_node *node = cl->dev.of_node;
	int ret, val;
	#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	enum boot_mode_t boot_mode = UNKNOWN_BOOT;
	#endif

	if (!pdata) {
		ret = lp855x_parse_dt(&cl->dev, node);
		if (ret < 0) {
		   dev_err(&cl->dev, "no platform data supplied\n");
		   return ret;
		}
		pdata = cl->dev.platform_data;
	}

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->pdata = pdata;
	lp->chipname = id->name;
	lp->chip_id = id->driver_data;
	lp->cool_dev = cooler;
	i2c_set_clientdata(cl, lp);
	mutex_init(&(lp->bl_update_lock));

	if (lp->pdata->gpio_en >= 0 && load_switch_support) {
		ret = gpio_request(lp->pdata->gpio_en, "backlight_lp855x_gpio");
		if (ret != 0) {
			pr_err("backlight gpio request failed, %d\n", lp->pdata->gpio_en);
			 /* serve as a flag as well */
			lp->pdata->gpio_en = -EINVAL;
		}
	}

	ret = lp855x_configure(lp);
	if (ret) {
		dev_err(lp->dev, "device config err: %d\n", ret);
		goto err_dev;
	}

	#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	boot_mode = get_boot_mode();
	if( boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		lp->pdata->initial_brightness = 0;
	}
	#endif

	val = lp->pdata->initial_brightness;
	dev_dbg(lp->dev, "Initial %s brightness\n",
			(lp->cfg->reg_brightness == LP855X_BRIGHTNESS_CTRL)
			? "LP8550TO6" : "LP8557");

	ret = lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	if (ret)
		goto err_dev;

	ret = lp855x_backlight_register(lp);
	if (ret) {
		dev_err(lp->dev,
			" failed to register backlight. err: %d\n", ret);
		goto err_dev;
	}

	lp855x_led_set_brightness(
				&(lp->led_data->bl),
				pdata->initial_brightness);

	#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	boot_mode = get_boot_mode();
	if( boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		lp8557_bl_off(lp);
	}
	#endif

	ret = sysfs_create_group(&lp->dev->kobj, &lp855x_attr_group);
	if (ret) {
		dev_err(lp->dev, " failed to register sysfs. err: %d\n", ret);
		goto err_sysfs;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	lp->early_suspend.suspend = lp855x_early_suspend;
	lp->early_suspend.resume = lp855x_late_resume;
	register_early_suspend(&lp->early_suspend);
#endif

	lp->cool_dev.cdev = thermal_cooling_device_register(lp->cool_dev.type,
							     (void *)lp,
							     &cooling_ops);
	if (!lp->cool_dev.cdev)
		return -EINVAL;
	device_create_file(&lp->cool_dev.cdev->device, &dev_attr_levels);

	return 0;

err_sysfs:
	lp855x_backlight_unregister(lp);
err_dev:
	mutex_destroy(&(lp->bl_update_lock));
	return ret;
}

static int lp855x_remove(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);

	mutex_lock(&(lp->bl_update_lock));

	_lp855x_led_set_brightness(lp, 0);
	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group);
	lp855x_backlight_unregister(lp);
	mutex_destroy(&(lp->bl_update_lock));

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lp->early_suspend);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int lp855x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lp855x *lp = i2c_get_clientdata(client);

	mutex_lock(&(lp->bl_update_lock));

	dev_dbg(lp->dev, "lp855x_suspend\n");

	_lp855x_led_set_brightness(lp, 0);

	mutex_unlock(&(lp->bl_update_lock));

	if (load_switch_support) {
		if (lp->pdata->gpio_en == -1)
			return -1;

		gpio_set_value(lp->pdata->gpio_en, 0);
	}

	return 0;
}

static int lp855x_resume(struct i2c_client *client)
{
#if 0
	struct lp855x *lp = i2c_get_clientdata(client);
	struct lp855x_platform_data *pdata = lp->pdata;

	if (load_switch_support)
		if (pdata->gpio_en == -1)
			return 0;

	mutex_lock(&(lp->bl_update_lock));

	if (load_switch_support) {
		gpio_set_value(pdata->gpio_en, 1);
		msleep(20);
	}

	if (pdata->load_new_rom_data && pdata->size_program) {
		int i;
		for (i = 0; i < pdata->size_program; i++) {
			addr = pdata->rom_data[i].addr;
			val = pdata->rom_data[i].val;
			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;

			ret |= lp855x_write_byte(lp, addr, val);
		}
	}

	if (ret)
		dev_err(lp->dev, "i2c write err\n");

	mutex_unlock(&(lp->bl_update_lock));
#endif

	return 0;
}

static lp855x_shutdown(struct i2c_client *client)
{
	struct lp855x *lp = i2c_get_clientdata(client);

	mutex_lock(&(lp->bl_update_lock));
	dev_dbg(lp->dev, "lp855x_shutdown\n");
	_lp855x_led_set_brightness(lp, 0);
	mutex_unlock(&(lp->bl_update_lock));
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lp855x_early_suspend(struct early_suspend *es)
{

	struct lp855x *lp;
	lp = container_of(es, struct lp855x, early_suspend);

	if (lp855x_suspend(lp->client, PMSG_SUSPEND) != 0)
		dev_err(lp->dev, "%s: failed\n", __func__);

}

static void lp855x_late_resume(struct early_suspend *es)
{

	struct lp855x *lp;
	lp = container_of(es, struct lp855x, early_suspend);

	if (lp855x_resume(lp->client) != 0)
		dev_err(lp->dev, "%s: failed\n", __func__);
}

#else
static const struct dev_pm_ops lp855x_pm_ops = {
	.suspend	= lp855x_suspend,
	.resume		= lp855x_resume,
};
#endif
#endif

static const struct of_device_id lp855x_dt_ids[] = {
	{ .compatible = "ti,lp8550_led", },
	{ .compatible = "ti,lp8551_led", },
	{ .compatible = "ti,lp8552_led", },
	{ .compatible = "ti,lp8553_led", },
	{ .compatible = "ti,lp8556_led", },
	{ .compatible = "ti,lp8557_led", },
	{ }
};
MODULE_DEVICE_TABLE(of, lp855x_dt_ids);

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550_led", LP8550},
	{"lp8551_led", LP8551},
	{"lp8552_led", LP8552},
	{"lp8553_led", LP8553},
	{"lp8556_led", LP8556},
	{"lp8557_led", LP8557},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp855x_driver = {
	.driver = {
		   .name = "lp855x_led",
		   .of_match_table = of_match_ptr(lp855x_dt_ids),
		   },
	.probe = lp855x_probe,
/*#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm	= &lp855x_pm_ops,
#endif*/
	.suspend = lp855x_suspend,
	.resume = lp855x_resume,
	.remove = lp855x_remove,
	.shutdown = lp855x_shutdown,
	.id_table = lp855x_ids,
};

module_i2c_driver(lp855x_driver);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
