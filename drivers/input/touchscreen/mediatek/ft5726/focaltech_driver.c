/*
 * Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/************************************************************************
* File Name: focaltech_driver.c
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: Function for driver initial, report point, resume, suspend
*
************************************************************************/
#include "tpd_custom_fts.h"
#include "focaltech_ex_fun.h"
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#include <linux/power_supply.h>
#endif

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#ifdef FTS_CHARGER_DETECT
#include <mach/upmu_sw.h>
#define TPD_CHARGER_CHECK_CIRCLE 200
static struct delayed_work fts_charger_check_work;
static struct workqueue_struct *fts_charger_check_workqueue;
static void fts_charger_check_func(struct work_struct *);
static u8 fts_charger_mode;
#endif
#ifdef GTP_ESD_PROTECT
#define TPD_ESD_CHECK_CIRCLE	200
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
static int count_irq;
static unsigned long esd_check_circle = TPD_ESD_CHECK_CIRCLE;
static u8 run_check_91_register;
#endif

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif

struct fts_touch_info {
	u8 x_msb:4, rev:2, event:2;
	u8 x_lsb;
	u8 y_msb:4, id:4;
	u8 y_lsb;
	u8 weight;
	u8 speed:2, direction:2, area:4;
};

struct fts_packet_info {
	u8 gesture;
	u8 fingers:4, frame:4;
	struct fts_touch_info touch[CFG_MAX_TOUCH_POINTS];
};

/*touch event info*/
struct ts_event {
	/*x coordinate */
	u16 au16_x[CFG_MAX_TOUCH_POINTS];
	/*y coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];
	/*touch event: 0 -- down; 1-- up; 2 -- contact */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];
	/*touch ID */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];
	u16 pressure[CFG_MAX_TOUCH_POINTS];
	u16 area[CFG_MAX_TOUCH_POINTS];
	u8 touch_point;
	u8 touch_point_num;
};

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
static DEFINE_MUTEX(i2c_rw_access);
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);
static int tpd_focal_probe(struct i2c_client *client,
			   const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_focal_remove(struct i2c_client *client);
static void tpd_focal_shutdown(struct i2c_client *client);
static int touch_event_handler(void *unused);
static int tpd_register_powermanger(void);

struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
static int tpd_halt;
static int tpd_rst_gpio_number;
static int tpd_int_gpio_number;
unsigned int touch_irq = 0;
static int IICErrorCountor;
static bool focal_suspend_flag;
static bool enable_vdd;
static int tpd_flag;
bool is_update = false;
int current_touchs;
unsigned char hw_rev;
unsigned char ft_vendor_id = 0;
unsigned char ft_routing_type = 0;

#if   defined(CONFIG_FB)
	static int screen_on = 1;
#endif

/************************************************************************
* Name: ftxxxx_i2c_Read
* Brief: i2c read
* Input: i2c info, write buf, write len, read buf, read len
* Output: get data in the 3rd buf
* Return: fail <0
***********************************************************************/
int ftxxxx_i2c_Read(struct i2c_client *client, char *writebuf, int writelen,
		    char *readbuf, int readlen)
{
	int ret;
	int retry = 0;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		for (retry = 0; retry < IICReadWriteRetryTime; retry++) {
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret >= 0)
				break;

			msleep(1);
		}
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		for (retry = 0; retry < IICReadWriteRetryTime; retry++) {
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret >= 0)
				break;

			msleep(1);
		}
	}

	if (retry == IICReadWriteRetryTime) {
		dev_err(&client->dev,
			"[focal] %s: i2c read error. err code = %d\n", __func__,
			ret);
		IICErrorCountor += 1;
		if (IICErrorCountor >= 10) {
			dev_err(&client->dev,
				"[focal] %s: i2c read/write error over 10 times\n",
				__func__);
			dev_err(&client->dev,
				"[focal] %s: excute reset IC process\n",
				__func__);
			return ret;
		}
		return ret;
	}
	IICErrorCountor = 0;
	return ret;
}

/************************************************************************
* Name: ftxxxx_i2c_Write
* Brief: i2c write
* Input: i2c info, write buf, write len
* Output: no
* Return: fail <0
***********************************************************************/
int ftxxxx_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;
	int retry = 0;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	for (retry = 0; retry < IICReadWriteRetryTime; retry++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0)
			break;
		msleep(30);
	}

	if (retry == IICReadWriteRetryTime) {
		dev_err(&client->dev,
			"[focal] %s: i2c write error. err code = %d\n",
			__func__, ret);

		IICErrorCountor += 1;

		if (IICErrorCountor >= 10) {
			pr_err
			    ("[focal] %s: i2c read/write error over 10 times\n",
			     __func__);
			pr_err("[focal] %s: excute reset IC process\n",
			       __func__);
			return ret;
		}
		return ret;
	}

	IICErrorCountor = 0;

	return ret;
}

/************************************************************************
* Name: fts_write_reg
* Brief: write register
* Input: i2c info, reg address, reg value
* Output: no
* Return: fail <0
***********************************************************************/
int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = { 0 };

	buf[0] = regaddr;
	buf[1] = regvalue;
	return ftxxxx_i2c_Write(client, buf, sizeof(buf));
}

/************************************************************************
* Name: fts_read_reg
* Brief: read register
* Input: i2c info, reg address, reg value
* Output: get reg value
* Return: fail <0
***********************************************************************/
int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return ftxxxx_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

/*register driver and device info*/
static const struct of_device_id focal_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};

MODULE_DEVICE_TABLE(of, focal_dt_match);

static const struct i2c_device_id tpd_id[] = { {TPD_DEVICE, 0}, {} };
static unsigned short force[] = { 0, 0x70, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

static struct i2c_driver tpd_i2c_driver = {

	.driver = {
		   .name = "ft5726",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(focal_dt_match),
		   },
	.probe = tpd_focal_probe,
	.remove = tpd_focal_remove,
	.shutdown = tpd_focal_shutdown,
	.id_table = tpd_id,
	.detect = tpd_detect,
	.address_list = (const unsigned short *)forces,
};

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
static void tpd_wakeup_handler(void)
{
	kpd_tpd_wakeup_handler(0x1);
	kpd_tpd_wakeup_handler(0x0);
}
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
int charger_exist(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval online_usb, online_ac;

	psy = power_supply_get_by_name("usb");
	if (!psy)
		return -ENODEV;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &online_usb);
	if (ret)
		return -ENOENT;

	psy = power_supply_get_by_name("ac");
	if (!psy)
		return -ENODEV;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &online_ac);
	if (ret)
		return -ENOENT;

	if (online_usb.intval || online_ac.intval)
		return 1;
	else
		return 0;
}
#endif

/***********************************************************************
* Name: fts_report_value
* Brief: report the point information
* Input: event info
* Output: no
* Return: success is zero
***********************************************************************/
static int fts_report_value(struct ts_event *data)
{
	int ret = -1;
	int touchs = 0;
	u8 addr = 0x01;
	struct fts_packet_info buf;
	struct fts_touch_info *touch;
	int i, x, y;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buffer[64];
	int charger_flag = 0;
#endif

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	if (focal_suspend_flag && (gesture_wakeup_enabled == 1)) {
		tpd_wakeup_handler();
		focal_suspend_flag = false;
#ifdef CONFIG_AMAZON_METRICS_LOG
		charger_flag = charger_exist();
		if (charger_flag >= 0) {
			snprintf(buffer, sizeof(buffer),
				"%s:touchwakup:%s_charger_wakeup=1;CT;1:NR",
				__func__, (charger_flag ? "with" : "without"));
			log_to_metrics(ANDROID_LOG_INFO, "TouchWakeup", buffer);
		} else
			pr_err("[focal] %s charger_exist error: %d\n",
				__func__, charger_flag);
#endif
	}
#endif

#if   defined(CONFIG_FB)
	if (!focal_suspend_flag && (screen_on == 0)) {
		pr_err("[focal] %s touch resume ongoing, ignore.\n", __func__);
		return -1;
	}
#endif

	mutex_lock(&i2c_access);
	ret = ftxxxx_i2c_Read(i2c_client, &addr, 1, (char *)&buf,
			      sizeof(struct fts_packet_info));
	if (ret < 0) {
		pr_err("[focal] %s read touchdata failed.\n", __func__);
		mutex_unlock(&i2c_access);
		return ret;
	}
	mutex_unlock(&i2c_access);

	touch = &buf.touch[0];
	for (i = 0; i < (buf.fingers) && (i < CFG_MAX_TOUCH_POINTS);
	     i++, touch++) {

		x = (u16) (touch->x_msb << 8) | (u16) touch->x_lsb;
		y = (u16) (touch->y_msb << 8) | (u16) touch->y_lsb;
		input_mt_slot(tpd->dev, touch->id);

		if (touch->event == 0 || touch->event == 2) {
			input_mt_report_slot_state(tpd->dev,
						   MT_TOOL_FINGER, true);
			input_report_abs(tpd->dev, ABS_MT_PRESSURE,
					 touch->weight);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,
					 touch->area);
			input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
			touchs |= BIT(touch->id);
			current_touchs |= BIT(touch->id);

			/*
			   pr_debug
			   * ("[focal]%d: id=%d, x=%d, y=%d, p=0x%x, m=%d\n",
			   i, touch->id, x, y, touch->weight, touch->area);
			 */
		} else {
			input_mt_report_slot_state(tpd->dev,
						   MT_TOOL_FINGER, false);
			current_touchs &= ~BIT(touch->id);
		}
	}

	if (unlikely(current_touchs ^ touchs)) {
		for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
			if (BIT(i) & (current_touchs ^ touchs)) {
				input_mt_slot(tpd->dev, i);
				input_mt_report_slot_state(tpd->dev,
							   MT_TOOL_FINGER,
							   false);
			}
		}
	}
	current_touchs = touchs;
	if (touchs)
		input_report_key(tpd->dev, BTN_TOUCH, 1);
	else
		input_report_key(tpd->dev, BTN_TOUCH, 0);

	input_sync(tpd->dev);
	return 0;
}

/************************************************************************
* Name: touch_event_handler
* Brief: interrupt event from TP, and read/report data to Android system
* Input: no use
* Output: no
* Return: 0
***********************************************************************/
static int touch_event_handler(void *unused)
{
	struct ts_event pevent;
	struct sched_param param = {.sched_priority = RTPM_PRIO_TPD };

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		fts_report_value(&pevent);
	} while (!kthread_should_stop());

	return 0;
}

/************************************************************************
* Name: fts_reset_tp
* Brief: reset TP
* Input: pull low or high
* Output: no
* Return: 0
***********************************************************************/
void fts_reset_tp(int HighOrLow)
{
	if (HighOrLow)
		gpio_set_value(tpd_rst_gpio_number, 1);
	else
		gpio_set_value(tpd_rst_gpio_number, 0);
}

/************************************************************************
* Name: tpd_detect
* Brief: copy device name
* Input: i2c info, board info
* Output: no
* Return: 0
***********************************************************************/
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

/************************************************************************
* Name: tpd_eint_interrupt_handler
* Brief: deal with the interrupt event
* Input: no
* Output: no
* Return: no
***********************************************************************/
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	#ifdef GTP_ESD_PROTECT
		count_irq++;
	#endif
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

/************************************************************************
* Name: fts_init_gpio_hw
* Brief: initial gpio
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static int fts_init_gpio_hw(void)
{
	int ret = 0;

	ret = gpio_request(tpd_rst_gpio_number, "ft5726-rst");
	if (ret) {
		pr_err("[focal] reset-gpio:%d request fail.\n",
		       tpd_rst_gpio_number);
		return ret;
	}

	ret = gpio_request(tpd_int_gpio_number, "ft5726-int");
	if (ret) {
		pr_err("[focal] int-gpio:%d request fail.\n",
						tpd_int_gpio_number);
		return ret;
	}

	gpio_direction_output(tpd_rst_gpio_number, 0);
	/* pr_debug("[focal] rst-gpio = %d, int-gpio = %d\n",
	       gpio_get_value(tpd_rst_gpio_number),
	       gpio_get_value(tpd_int_gpio_number)); */

	return ret;
}

static void fts_reset_hw(void)
{
	gpio_set_value(tpd_rst_gpio_number, 0);

	msleep(20);

	gpio_set_value(tpd_rst_gpio_number, 1);

	msleep(200);

	/* pr_debug("[focal] rst-gpio = %d, int-gpio = %d\n",
	       gpio_get_value(tpd_rst_gpio_number),
	       gpio_get_value(tpd_int_gpio_number)); */
}
#ifdef FTS_POWER_DOWN_IN_SUSPEND
static void tpd_power_down(void)
{
	int ret = 0;

	gpio_set_value(tpd_rst_gpio_number, 0);

	msleep(1);

	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		pr_err("[focal]Failed to disable vgp4: %d\n", ret);

	if (enable_vdd) {
		ret = regulator_disable(tpd->reg);
		if (ret != 0)
			pr_err("[focal]Failed to disable vgp3: %d\n", ret);
	}
}
#endif

static void tpd_power_on(void)
{
	int ret = 0;

	/* pr_debug("[focal] %s-%d\n", __func__, __LINE__); */

	gpio_set_value(tpd_rst_gpio_number, 0);

	msleep(20);

	ret = regulator_enable(tpd->io_reg);
	if (ret != 0)
		pr_err("[focal] Failed to enable vgp4: %d\n", ret);

	if (enable_vdd) {
		ret = regulator_enable(tpd->reg);
		if (ret != 0)
			pr_err("[focal] Failed to enable vgp3: %d\n", ret);
	}

	msleep(100);

	fts_reset_hw();
}

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
static void any_touch_wakeup_enable(void)
{
	int retval = 0;

	if (gesture_wakeup_enabled == 1) {
		retval = irq_set_irq_wake(touch_irq, true);
		if (retval)
			pr_err("%s: irq_set_irq_wake err = %d\n", __func__, retval);
		enable_irq(touch_irq);
		mutex_lock(&i2c_access);
		fts_write_reg(i2c_client, 0xa5, 0x01);
		mutex_unlock(&i2c_access);
	} else {
#ifdef FTS_POWER_DOWN_IN_SUSPEND
		tpd_power_down();
#else
		/* write i2c command to make IC into deepsleep */
		mutex_lock(&i2c_access);
		fts_write_reg(i2c_client, 0xa5, 0x03);
		mutex_unlock(&i2c_access);
#endif
	}
}
#endif

static int of_get_focal_platform_data(struct device *dev)
{
	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(focal_dt_match), dev);
		if (!match) {
			pr_err("[focal] %s: No device match found\n", __func__);
			return -ENODEV;
		}
	}

	tpd_rst_gpio_number = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	if (tpd_rst_gpio_number < 0) {
		pr_err("[focal] Can't find rst-gpio\n");
		return tpd_rst_gpio_number;
	}

	tpd_int_gpio_number = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	if (tpd_int_gpio_number < 0) {
		pr_err("[focal] Can't find int-gpio\n");
		return tpd_int_gpio_number;
	}

	touch_irq = irq_of_parse_and_map(dev->of_node, 0);
	if (touch_irq < 0) {
		pr_err("[focal] get touch_irq failed!\n");
		return -ENOENT;
	}

	pr_debug("[focal] tpd_rst_gpio= %d, tpd_int_gpio= %d, irq = %d\n",
	       tpd_rst_gpio_number, tpd_int_gpio_number, touch_irq);
	return 0;
}
/*
static char get_hw_rev(struct device *dev)
{
	unsigned int hwid_gpio[3] = { 0 };
	struct pinctrl *hwid_gpio_pinctrl;
	struct pinctrl_state *set_state;
	char retval = 0;

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(focal_dt_match), dev);
		if (!match) {
			pr_err("[focal] No of-device match found\n");
			return 0;
		}
	}

	hwid_gpio_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(hwid_gpio_pinctrl)) {
		pr_err("[focal] get pinctrl failed, return hw_rev 0\n");
		return 0;
	}

	set_state =
	    pinctrl_lookup_state(hwid_gpio_pinctrl, "hwid_gpio_default");
	if (IS_ERR(set_state)) {
		pr_err
		 ("[focal]hwid_gpio_default not found, return hw_rev 0\n");
		return 0;
	}

	retval = pinctrl_select_state(hwid_gpio_pinctrl, set_state);
	if (retval < 0) {
		pr_err("[focal] set pinctrl state failed, return hw_rev 0\n");
		return 0;
	}

	retval = 0;
	hwid_gpio[0] = of_get_named_gpio(dev->of_node, "hwid0-gpio", 0);
	if (hwid_gpio[0] < 0) {
		pr_err("[focal] Can't find hwid0-gpio\n");
		return 0;
	}
	gpio_direction_input(hwid_gpio[0]);
	retval |= gpio_get_value(hwid_gpio[0]);

	hwid_gpio[1] = of_get_named_gpio(dev->of_node, "hwid1-gpio", 0);
	if (hwid_gpio[1] < 0) {
		pr_err("[focal] Can't find hwid1-gpio\n");
		return 0;
	}
	gpio_direction_input(hwid_gpio[1]);
	retval |= (gpio_get_value(hwid_gpio[1]) << 1);

	hwid_gpio[2] = of_get_named_gpio(dev->of_node, "hwid2-gpio", 0);
	if (hwid_gpio[2] < 0) {
		pr_err("[focal] Can't find hwid2-gpio\n");
		return 0;
	}
	gpio_direction_input(hwid_gpio[2]);
	retval |= (gpio_get_value(hwid_gpio[2]) << 2);

	return retval;
}
*/
/***********************************************************************
* Name: tpd_probe
* Brief: driver entrance function for initial/power on/create channel
* Input: i2c info, device id
* Output: no
* Return: 0
***********************************************************************/
static int tpd_focal_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int retval = 0, fs = 0, chdev = 0, proc = 0;
	unsigned char reg_value[2] = { 0 };
	unsigned char reg_addr;
	#ifdef CONFIG_IDME
	unsigned int board_type;
	unsigned int board_rev;
	#endif

	pr_debug("[focal] %s-%d probe start.\n", __func__, __LINE__);

	i2c_client = client;

	#ifdef CONFIG_IDME
	board_type = idme_get_board_type();
	board_rev = idme_get_board_rev();
	pr_info("[focal]board_type:0x%x, rev:0x%x\n",
					board_type, board_rev);
	if (board_type == BOARD_TYPE_SUEZ) {
		switch (board_rev) {
		case BOARD_REV_PROTO_0:
					enable_vdd = 0;
					break;
		default:
					enable_vdd = 1;
		};
	} else
		enable_vdd = 0;
	#else
	enable_vdd = 1;
	#endif

	if (enable_vdd) {
		/*get tpd regulator */
		tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
		if (IS_ERR(tpd->reg)) {
			pr_err("[focal] Can't request VGP3 power supply: %ld\n",
				   PTR_ERR(tpd->reg));
			return PTR_ERR(tpd->reg);
		}

		retval = regulator_set_voltage(tpd->reg, 3300000, 3300000);
		if (retval != 0) {
			pr_err
				("[focal]Failed to set vgp3 voltage:%d\n",
					retval);
			goto err_free_vgp3;
		}
	}

	tpd->io_reg = regulator_get(tpd->tpd_dev, "vtouchio");
	if (IS_ERR(tpd->io_reg)) {
		pr_err("[focal] Can't request VGP4 power supply: %ld\n",
		       PTR_ERR(tpd->reg));
		retval = PTR_ERR(tpd->io_reg);
		goto err_free_vgp3;
	}

	retval = regulator_set_voltage(tpd->io_reg, 1800000, 1800000);
	if (retval != 0) {
		pr_err("[focal] Failed to set vgp4 voltage: %d\n", retval);
		goto err_free_vgp4;
	}

	/* hw_rev = get_hw_rev(&i2c_client->dev);	*/	/*No need */
	/* pr_debug("[focal] HW rev:%d\n", hw_rev); */

	retval = of_get_focal_platform_data(&i2c_client->dev);
	if (retval) {
		pr_err("[focal] GPIO configuration failed\n");
		goto err_free_vgp4;
	}

	retval = fts_init_gpio_hw();
	if (retval) {
		pr_err("[focal] Init_gpio_hw failed\n");
		goto err_free_vgp4;
	}

	tpd_power_on();

	/* Check if TP is connectted, if not return -1 */
	retval = fts_read_reg(i2c_client, 0x00, &reg_value[0]);
	if (retval < 0 || reg_value[0] != 0) {
		/* reg0 data running state is 0; other state is not 0 */
		pr_err("[focal] Chip does not exits. I2C transfer error:%d, Reg0:%d\n",
			retval, reg_value[0]);
		goto err_free_hw;
	}

	reg_addr = FTS_REG_FW_VER;
	retval = ftxxxx_i2c_Read(i2c_client, &reg_addr, 1, reg_value, 1);
	if (retval < 0)
		pr_err("[focal] Read FW ver error!\n");
	else
		pr_debug("[focal] FW ver:0x%x\n", reg_value[0]);

	reg_addr = FTS_REG_VENDOR_ID;
	retval = ftxxxx_i2c_Read(i2c_client, &reg_addr, 1, reg_value, 1);
	if (retval < 0)
		pr_err("[focal] Read Vendor ID error!\n");
	else {
		ft_vendor_id = reg_value[0];
		pr_debug("[focal] Vendor ID:0x%x\n", reg_value[0]);
	}

	reg_addr = FTS_REG_ROUTING_TYPE;
	retval = ftxxxx_i2c_Read(i2c_client, &reg_addr, 1, reg_value, 1);
	if (retval < 0)
		pr_err("[focal]Read routing type error!\n");
	else {
		ft_routing_type = reg_value[0];
		pr_debug("[focal]Routing type:0x%x\n", reg_value[0]);
	}

	reg_addr = FTS_REG_CHIP_ID;
	retval = ftxxxx_i2c_Read(i2c_client, &reg_addr, 1, &reg_value[1], 1);
	if (retval < 0)
		pr_err("[focal] Read Chip ID error!\n");
	else
		pr_debug("[focal] Chip ID:0x%x\n", reg_value[1]);

	retval = request_irq(touch_irq, tpd_eint_interrupt_handler,
			     IRQF_TRIGGER_FALLING, TPD_DEVICE, NULL);
	if (retval < 0) {
		pr_err("[focal] request_irq faled.");
		goto err_free_hw;
	}

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	mutex_init(&fts_gesture_wakeup_mutex);
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		retval = PTR_ERR(thread);
		pr_err("[focal] failed to create kernel thread: %d\n", retval);
		goto err_free_irq;
	}

	pr_debug("[focal] %s-%d\n", __func__, __LINE__);

#ifdef SYSFS_DEBUG
	fs = fts_create_sysfs(i2c_client);
	if (fs != 0)
		pr_err("[focal] %s - ERROR: create sysfs failed.\n", __func__);
#endif

#ifdef FTS_CTL_IIC
	chdev = fts_rw_iic_drv_init(i2c_client);
	if (chdev < 0)
		pr_err("[focal] %s: create fts control iic driver failed\n",
		       __func__);
#endif

#ifdef FTS_APK_DEBUG
	proc = fts_create_apk_debug_channel(i2c_client);
	if (proc != 0)
		pr_err("[focal] Couldn't create apk debug channel!\n");
#endif

#ifdef TPD_AUTO_UPGRADE
	pr_debug("[focal] *****Enter Focal Touch Auto Upgrade*****\n");

	is_update = true;
	retval = fts_ctpm_auto_upgrade(i2c_client);
	if (retval != 0)
		pr_err("[focal] Auto upgrade failed\n");

	is_update = false;
#endif

#ifdef GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);

	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	if (gtp_esd_check_workqueue == NULL) {
		pr_err("[focal] %s: couldn't create ESD workqueue\n", __func__);
		retval = -ENOMEM;
		goto err_free;
	}

	queue_delayed_work(gtp_esd_check_workqueue,
			   &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

#ifdef FTS_CHARGER_DETECT
	INIT_DELAYED_WORK(&fts_charger_check_work, fts_charger_check_func);

	fts_charger_check_workqueue = create_workqueue("fts_charger_check");
	if (fts_charger_check_workqueue == NULL) {
		pr_err("[focal] Couldn't create charger check workqueue\n");
		retval = -ENOMEM;
		goto err_free;
	}

	queue_delayed_work(fts_charger_check_workqueue,
		&fts_charger_check_work, TPD_CHARGER_CHECK_CIRCLE);
#endif

	input_mt_init_slots(tpd->dev, CFG_MAX_TOUCH_POINTS, 0);
	input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	pr_debug("[focal] Touch driver probe %s\n",
	       (retval < 0) ? "FAIL" : "PASS");

	tpd_register_powermanger();
	focal_suspend_flag = false;
	tpd_load_status = 1;
	return 0;

#if defined(GTP_ESD_PROTECT) || defined(FTS_CHARGER_DETECT)
err_free:
	if (gtp_esd_check_workqueue != NULL)
		destroy_workqueue(gtp_esd_check_workqueue);
	kthread_stop(thread);
#endif
#ifdef SYSFS_DEBUG
	if (fs == 0)
		fts_release_sysfs(client);
#endif
#ifdef FTS_CTL_IIC
	if (chdev >= 0)
		fts_rw_iic_drv_exit();
#endif
#ifdef FTS_APK_DEBUG
	if (proc == 0)
		fts_release_apk_debug_channel();
#endif
err_free_irq:
	free_irq(touch_irq, NULL);
err_free_hw:
	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);

err_free_vgp4:
	retval = regulator_disable(tpd->io_reg);
	if (retval != 0)
		pr_err("[focal]Failed to disable vgp4: %d\n", retval);
	regulator_put(tpd->io_reg);

err_free_vgp3:
	if (enable_vdd) {
		retval = regulator_disable(tpd->reg);
		if (retval != 0)
			pr_err("[focal]Failed to disable vgp3: %d\n", retval);
		regulator_put(tpd->reg);
	}
	return retval;
}

static void tpd_focal_shutdown(struct i2c_client *client)
{
	int ret = 0;
	unsigned int isenable;

	tpd_halt = 1;
#ifdef FTS_CHARGER_DETECT
	cancel_delayed_work(&fts_charger_check_work);
	flush_workqueue(fts_charger_check_workqueue);
	destroy_workqueue(fts_charger_check_workqueue);
#endif

#ifdef GTP_ESD_PROTECT
	cancel_delayed_work(&gtp_esd_check_work);
	flush_workqueue(gtp_esd_check_workqueue);
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

#ifdef SYSFS_DEBUG
	fts_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	fts_rw_iic_drv_exit();
#endif

#ifdef FTS_APK_DEBUG
	fts_release_apk_debug_channel();
#endif

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	if (gesture_wakeup_enabled == 1)
		irq_set_irq_wake(touch_irq, false);
#endif
	free_irq(touch_irq, NULL);

	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);

	isenable = regulator_is_enabled(tpd->io_reg);
	if (isenable) {
		ret = regulator_disable(tpd->io_reg);
		if (ret != 0)
			pr_err("[focal]Failed to disable vgp4: %d\n", ret);
	}
	regulator_put(tpd->io_reg);

	if (enable_vdd) {
		isenable = regulator_is_enabled(tpd->reg);
		if (isenable) {
			ret = regulator_disable(tpd->reg);
			if (ret != 0)
				pr_err("[focal]Failed to disable vgp3: %d\n", ret);
		}
		regulator_put(tpd->reg);
	}
	pr_debug("[focal] TPD shutdown\n");
}

/***********************************************************************
* Name: tpd_focal_remove
* Brief: remove driver/channel
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
static int tpd_focal_remove(struct i2c_client *client)
{
	int ret = 0;
#ifdef FTS_CHARGER_DETECT
	destroy_workqueue(fts_charger_check_workqueue);
#endif

#ifdef GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
	kthread_stop(thread);
#endif

#ifdef SYSFS_DEBUG
	fts_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	fts_rw_iic_drv_exit();
#endif

#ifdef FTS_APK_DEBUG
	fts_release_apk_debug_channel();
#endif

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	if (gesture_wakeup_enabled == 1)
		irq_set_irq_wake(touch_irq, false);
#endif
	free_irq(touch_irq, NULL);

	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);

	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		pr_err("[focal]Failed to disable vgp4: %d\n", ret);
		regulator_put(tpd->io_reg);

	if (enable_vdd) {
		ret = regulator_disable(tpd->reg);
		if (ret != 0)
			pr_err("[focal]Failed to disable vgp3: %d\n", ret);

		regulator_put(tpd->reg);
	}

	pr_debug("[focal] TPD removed\n");
	return 0;
}

#if   defined(CONFIG_FB)
/* frame buffer notifier block */
static int tpd_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			pr_debug("Resume by fb notifier.\n");
			screen_on = 1;

		}
		else if (*blank == FB_BLANK_POWERDOWN) {
			pr_debug("Suspend by fb notifier.\n");
			screen_on = 0;
		}
	}

	return 0;
}

static struct notifier_block tpd_fb_notifier = {
        .notifier_call = tpd_fb_notifier_callback,
};
#endif

static int tpd_register_powermanger()
{
#if   defined(CONFIG_FB)
	fb_register_client(&tpd_fb_notifier);
#endif
	return 0;
}


#ifdef FTS_CHARGER_DETECT
static void fts_charger_check_func(struct work_struct *work)
{
	int cur_charger_state, ret = 0;

	if (tpd_halt || is_update) {
		pr_err("[focal] exit %s. tpd_halt=%d, is_update=%d\n",
			__func__, tpd_halt, is_update);
		return;
	}

	if (apk_debug_flag) {
		pr_err("[focal] exit %s. apk_debug_flag=%d\n",
			__func__, apk_debug_flag);

		queue_delayed_work(fts_charger_check_workqueue,
			&fts_charger_check_work, TPD_CHARGER_CHECK_CIRCLE);
		return;
	}

	cur_charger_state = upmu_get_pchr_chrdet();
	pr_debug("[focal] charger mode = %d\n", cur_charger_state);

	if (fts_charger_mode != cur_charger_state) {
		pr_err("[focal] charger detected, mode = %d\n",
			cur_charger_state);
		fts_charger_mode = cur_charger_state;

		if (fts_charger_mode == 1) {
			ret = fts_write_reg(i2c_client,
				FTS_REG_CHARGER_STATE, 1);
			if (ret < 0)
				pr_err("[focal] write 0x8B value 1 fail");
		} else {
			ret = fts_write_reg(i2c_client,
				FTS_REG_CHARGER_STATE, 0);
			if (ret < 0)
				pr_err("[focal] write 0x8B value 0 fail");
		}
	}

	if (!tpd_halt)
		queue_delayed_work(fts_charger_check_workqueue,
			&fts_charger_check_work, TPD_CHARGER_CHECK_CIRCLE);
}
#endif

#ifdef GTP_ESD_PROTECT
/***********************************************************************
* Name: force_reset_guitar
* Brief: reset
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static void force_reset_guitar(void)
{
	int ret;

	pr_debug("[focal] force_reset_guitar\n");
	if (enable_vdd) {
		ret = regulator_disable(tpd->reg);
		if (ret != 0)
			pr_err("[focal] Failed to disable vgp3: %d\n", ret);
	}

	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		pr_err("[focal] Failed to disable vgp4: %d\n", ret);

	msleep(200);

	ret = regulator_enable(tpd->io_reg);
	if (ret != 0)
		pr_err("[focal] Failed to enable vgp4: %d\n", ret);

	if (enable_vdd) {
		ret = regulator_enable(tpd->reg);
		if (ret != 0)
			pr_err("[focal] Failed to enable vgp3: %d\n", ret);
	}

	msleep(20);

	gpio_direction_output(tpd_rst_gpio_number, 0);

	msleep(20);

	pr_debug("[focal] ic reset\n");

	gpio_set_value(tpd_rst_gpio_number, 1);

	msleep(300);
}

/* 0 for no apk upgrade, 1 for apk upgrade */

#define A3_REG_VALUE			0x58
#define RESET_91_REGVALUE_SAMECOUNT	5
static u8 g_old_91_Reg_Value = 0x00;
static u8 g_first_read_91 = 0x01;
static u8 g_91value_same_count;

/***********************************************************************
* Name: gtp_esd_check_func
* Brief: esd check function
* Input: struct work_struct
* Output: no
* Return: 0
***********************************************************************/
static void gtp_esd_check_func(struct work_struct *work)
{
	int i = 0;
	int ret = -1;
	u8 data = 0;
	u8 flag_error = 0;
	int reset_flag = 0;

	if (tpd_halt || is_update) {
		pr_err("[focal] exit %s. tpd_halt=%d, is_update=%d\n",
			__func__, tpd_halt, is_update);
		return;
	}

	if (apk_debug_flag) {
		pr_err("[focal] exit %s. apk_debug_flag=%d\n",
			__func__, apk_debug_flag);

		queue_delayed_work(gtp_esd_check_workqueue,
				   &gtp_esd_check_work, esd_check_circle);
		return;
	}

	run_check_91_register = 0;

	for (i = 0; i < 3; i++) {
		ret = fts_read_reg(i2c_client, 0xA3, &data);
		if (ret < 0)
			pr_err("[focal] read chip ID fail (%d)\n", i);

		if (ret >= 1 && A3_REG_VALUE == data)
			break;
	}

	if (i >= 3) {
		force_reset_guitar();
		pr_err("[focal] reset for ESD, A3_Reg_Value = 0x%02x\n", data);
		reset_flag = 1;
		goto FOCAL_RESET_A3_REGISTER;
	}

	ret = fts_read_reg(i2c_client, 0x8F, &data);
	if (ret < 0)
		pr_err("[focal] read 0x8F value fail");

	pr_debug("[focal] 0x8F:%d, count_irq is %d\n", data, count_irq);
	flag_error = 0;

	if (((count_irq - data) > 10) && ((data + 200) > (count_irq + 10)))
		flag_error = 1;

	if ((data - count_irq) > 10)
		flag_error = 1;

	if (1 == flag_error) {
		pr_err("[focal] reset for ESD, 0x8F=%d, count_irq=%d\n",
		       data, count_irq);
		force_reset_guitar();
		reset_flag = 1;
		goto FOCAL_RESET_INT;
	}

	run_check_91_register = 1;

	ret = fts_read_reg(i2c_client, 0x91, &data);
	if (ret < 0)
		pr_err("[focal] read 0x91 fail");

	pr_debug("[focal] 91 register value = 0x%02x old value = 0x%02x\n",
	       data, g_old_91_Reg_Value);

	if (0x01 == g_first_read_91) {
		g_old_91_Reg_Value = data;
		g_first_read_91 = 0x00;
	} else {
		if (g_old_91_Reg_Value == data) {
			g_91value_same_count++;
			pr_debug("[focal] g_91value_same_count=%d\n",
			       g_91value_same_count);

			if (RESET_91_REGVALUE_SAMECOUNT ==
					g_91value_same_count) {
				force_reset_guitar();
				pr_err("[focal] reset for ESD, g_91value_same_count = 5\n");
				g_91value_same_count = 0;
				reset_flag = 1;
			}

			esd_check_circle = TPD_ESD_CHECK_CIRCLE / 2;
			g_old_91_Reg_Value = data;
		} else {
			g_old_91_Reg_Value = data;
			g_91value_same_count = 0;
			esd_check_circle = TPD_ESD_CHECK_CIRCLE;
		}
	}

FOCAL_RESET_INT:
FOCAL_RESET_A3_REGISTER:
	count_irq = 0;
	data = 0;

	ret = fts_write_reg(i2c_client, 0x8F, data);
	if (ret < 0)
		pr_err("[focal] write 0x8F value fail");

	if (0 == run_check_91_register)
		g_91value_same_count = 0;

	if (!tpd_halt)
		queue_delayed_work(gtp_esd_check_workqueue,
				   &gtp_esd_check_work, esd_check_circle);
}
#endif

/************************************************************************
* Name: tpd_local_init
* Brief: add driver info
* Input: no
* Output: no
* Return: fail <0
***********************************************************************/
static int tpd_local_init(void)
{
	/* pr_debug("[focal] fts I2C Driver init.\n"); */

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		pr_err("[foacl] fts unable to add i2c driver.\n");
		return -1;
	}

	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID,
			     0, (TINNO_TOUCH_TRACK_IDS - 1), 0, 0);

	tpd_type_cap = 1;

	pr_debug("[focal] fts add i2c driver ok.\n");
	return 0;
}

/************************************************************************
* Name: tpd_resume
* Brief: system wake up
* Input: no use
* Output: no
* Return: no
***********************************************************************/
static void tpd_resume(struct device *h)
{
	pr_debug("[focal] %s-%d TOUCH RESUME.\n", __func__, __LINE__);

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	mutex_lock(&fts_gesture_wakeup_mutex);
#endif

#ifdef FTS_POWER_DOWN_IN_SUSPEND
#ifndef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	tpd_power_on();
#else
	if (gesture_wakeup_enabled == 1)
		fts_reset_hw();
	else
		tpd_power_on();
#endif
#else
	fts_reset_hw();
#endif

#ifndef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	enable_irq(touch_irq);
#endif

	msleep(30);
	tpd_halt = 0;
#ifdef GTP_ESD_PROTECT
	count_irq = 0;
	queue_delayed_work(gtp_esd_check_workqueue,
			   &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif
#ifdef FTS_CHARGER_DETECT
	queue_delayed_work(fts_charger_check_workqueue,
			&fts_charger_check_work, TPD_CHARGER_CHECK_CIRCLE);
#endif

	focal_suspend_flag = false;

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	mutex_unlock(&fts_gesture_wakeup_mutex);
#endif

	pr_debug("[focal] %s-%d TOUCH RESUME DONE.\n", __func__, __LINE__);
}

/************************************************************************
* Name: tpd_suspend
* Brief: system sleep
* Input: no use
* Output: no
* Return: no
***********************************************************************/
static void tpd_suspend(struct device *h)
{
	int i = 0;

	if (focal_suspend_flag) {
		pr_debug("[focal] IC already suspend\n");
		return;
	}
	pr_debug("[focal] %s-%d TOUCH SUSPEND.\n", __func__, __LINE__);

#ifdef GTP_ESD_PROTECT
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif
#ifdef FTS_CHARGER_DETECT
	cancel_delayed_work_sync(&fts_charger_check_work);
#endif

	tpd_halt = 1;
	if (unlikely(current_touchs)) {
		for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
			if (BIT(i) & current_touchs) {
				input_mt_slot(tpd->dev, i);
				input_mt_report_slot_state(tpd->dev,
							   MT_TOOL_FINGER,
							   false);
			}
		}
		current_touchs = 0;
		input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_sync(tpd->dev);
	}

#ifndef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	disable_irq(touch_irq);
#endif

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	mutex_lock(&fts_gesture_wakeup_mutex);
#endif

#ifdef FTS_POWER_DOWN_IN_SUSPEND
#ifndef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	tpd_power_down();
#else
	any_touch_wakeup_enable();
#endif
#else
#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	any_touch_wakeup_enable();
#else
	/* write i2c command to make IC into deepsleep */
	mutex_lock(&i2c_access);
	fts_write_reg(i2c_client, 0xa5, 0x03);
	mutex_unlock(&i2c_access);
#endif
#endif

	focal_suspend_flag = true;

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	mutex_unlock(&fts_gesture_wakeup_mutex);
#endif

	pr_debug("[focal] %s-%d TOUCH SUSPEND DONE.\n", __func__, __LINE__);
}

static struct tpd_driver_t tpd_device_driver = {

	.tpd_device_name = "ft5726",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	.tpd_have_button = 0,
};

/************************************************************************
* Name: tpd_suspend
* Brief:  called when loaded into kernel
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static int __init tpd_driver_init(void)
{
	/* pr_debug("[focal] MediaTek fts touch panel driver init\n"); */

	if (tpd_driver_add(&tpd_device_driver) < 0)
		pr_err("[focal] MediaTek add fts driver failed\n");

	return 0;
}

/************************************************************************
* Name: tpd_driver_exit
* Brief:  should never be called
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static void __exit tpd_driver_exit(void)
{
	/* pr_debug("[focal] MediaTek fts touch panel driver exit\n"); */
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
