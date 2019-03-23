#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/string.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <misc/board_id.h>
#include <linux/kthread.h>
#else
#include <string.h>
#endif

#include "lcm_drv.h"
#include "nt51021_wuxga_dsi_vdo.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#endif

#ifdef BUILD_LK
#define LCM_PRINT printf
#if defined(UFBL_FEATURE_IDME)
extern char *board_version(void);
#endif
#else
#if defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif
#endif

#define LCM_DBG(fmt, arg...) \
	LCM_PRINT ("[LCM-NT51021-WUXGA-DSI-VDO] %s (line:%d) :" fmt "\r\n", __func__, __LINE__, ## arg)

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#if defined(MTK_ALPS_BOX_SUPPORT)
#define HDMI_SUB_PATH 1
#else
#define HDMI_SUB_PATH 0
#endif

#if HDMI_SUB_PATH
#define FRAME_WIDTH  (1920)
#define FRAME_HEIGHT (1080)
#else
#define FRAME_WIDTH  (1200)
#define FRAME_HEIGHT (1920)
#endif

#define GPIO_OUT_ONE 1
#define GPIO_OUT_ZERO 0

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static unsigned int vendor_id = 0xf;
static int cmd_sel = -1;
static unsigned int cabc_mode = 0xc0;

static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

#define SET_RESET_PIN(v) lcm_set_gpio_output(GPIO_LCD_RST, v)	/* (lcm_util.set_reset_pin((v))) */

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

unsigned int GPIO_LCD_RST_EN;
unsigned int LCM_ID1_GPIO;
unsigned int LCM_ID0_GPIO;

#define INX			0x0		/* Innolux */
#define BOE			0x1		/* BOE */

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)	 lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									 lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				 lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)						  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define LCM_DSI_CMD_MODE			0

/* --------------------------------------------------------------------------- */
/* IIC interface */
/* --------------------------------------------------------------------------- */

#define I2C_ID_NAME "nt51021"

static DEFINE_MUTEX(nt51021_i2c_access);

static int nt51021_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int nt51021_remove(struct i2c_client *client);

static const struct of_device_id lcm_of_match[] = {
		{.compatible = "novatek,nt51021_i2c"},
		{},
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

#define NT51021_ESD_DETECT

#ifdef NT51021_ESD_DETECT
static struct task_struct *nt51021_esd_check_task;
static int start_esd_check_thread = 1;
static atomic_t suspend_state = ATOMIC_INIT(0);	/* For ESD Check Task */
#endif

struct i2c_client *nt51021_i2c_client = NULL;

struct nt51021_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id nt51021_id[] = {
	{I2C_ID_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, nt51021_id);

static struct i2c_driver nt51021_iic_driver = {
	.id_table = nt51021_id,
	.probe = nt51021_probe,
	.remove = nt51021_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "nt51021",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(lcm_of_match),
#endif
		   },
};

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	LCM_DBG("GPIO = 0x%x, Output= 0x%x", GPIO, output);

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#else
#if HDMI_SUB_PATH
#else
	gpio_direction_output(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
	gpio_set_value(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#endif
#endif
}

static void get_lcm_id(void)
{
#ifdef BUILD_LK
	unsigned int ret = 0;

	ret = mt_set_gpio_mode(LCM_ID1_GPIO, GPIO_MODE_00);
	if (0 != ret)
		LCM_DBG("ID1 mt_set_gpio_mode fail");

	ret = mt_set_gpio_dir(LCM_ID1_GPIO, GPIO_DIR_IN);
	if (0 != ret)
		LCM_DBG("ID1 mt_set_gpio_dir fail");

	ret = mt_set_gpio_mode(LCM_ID0_GPIO, GPIO_MODE_00);
	if (0 != ret)
		LCM_DBG("ID0 mt_set_gpio_mode fail");

	ret = mt_set_gpio_dir(LCM_ID0_GPIO, GPIO_DIR_IN);
	if (0 != ret)
		LCM_DBG("ID0 mt_set_gpio_dir fail");

	/* get ID pin status */
	vendor_id = mt_get_gpio_in(LCM_ID1_GPIO);
	vendor_id |= (mt_get_gpio_in(LCM_ID0_GPIO) << 1);
#else
#if HDMI_SUB_PATH
#else
	gpio_direction_input(LCM_ID1_GPIO);
	vendor_id = gpio_get_value(LCM_ID1_GPIO);
	gpio_direction_input(LCM_ID0_GPIO);
	vendor_id |= (gpio_get_value(LCM_ID0_GPIO) << 1);
#endif
#endif
	LCM_DBG("vendor_id = 0x%x", vendor_id);
}

static int nt51021_write_byte(u8 reg, u8 data)
{
	struct i2c_client *client;
	int ret;

	if (nt51021_i2c_client == NULL)
		return -1;

	client = nt51021_i2c_client;

	mutex_lock(&nt51021_i2c_access);
	ret = i2c_smbus_write_byte_data(client, reg, data);
	mutex_unlock(&nt51021_i2c_access);
	if (ret < 0) {
		LCM_DBG("failed to write 0x%.2x", reg);
		return ret;
	}

	return 0;
}

static int nt51021_read_byte(u8 reg, u8 *data)
{
	struct i2c_client *client;
	int ret;

	if (nt51021_i2c_client == NULL)
		return -1;

	client = nt51021_i2c_client;

	mutex_lock(&nt51021_i2c_access);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&nt51021_i2c_access);
	if (ret < 0) {
		LCM_DBG("failed to read 0x%.2x", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

#ifdef NT51021_ESD_DETECT
static int nt51021_esd_check(void)
{
	u8 addr, val1, val2;
	int ret = -1;

	addr = 0x81;
	val1 = 0x00;

	ret = nt51021_read_byte(addr, &val1);

	addr = 0x82;
	val2 = 0x00;

	ret = nt51021_read_byte(addr, &val2);

	if (ret < 0)
		return ret;

	if ((val1 == 0x00) && (val2 == 0x00))
		ret = 0;
	else if ((val1 == 0x10) && (val2 == 0x80))
		ret = 0;
	else if ((val1 == 0x40) && (val2 == 0x00))
		ret = 0;
	else if ((val1 == 0x00) && (val2 == 0x02))
		ret = 0;
	else {
		ret = 1;
		LCM_DBG("MIPI status: 0x%02x, 0x%02x", val1, val2);
	}

	return ret;
}

static void restore_lcm_registers(void)
{
	u8 cmd, data;
	int ret = 0;

	get_lcm_id();

	switch (vendor_id) {
	case INX:
		LCM_DBG("Restore INX registers");
		cmd = 0x83; /* Page setting */
		data = 0x00;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x00;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x8c; /* GOP setting */
		data = 0x80;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xc7; /* STV & RST falling timing */
		data = 0x50;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xc5; /* GOP clock falling location */
		data = 0x50;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x85; /* Test mode1 */
		data = 0x04;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x86; /* Test mode2 */
		data = 0x08;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xa9; /* Enable TP_SYNC */
		data = 0x20;
		ret += nt51021_write_byte(cmd, data);

		cmd = 0x83; /* Page setting */
		data = 0xaa;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x11;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x9c; /* Test mode3 */
		data = 0x10;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xa9; /* IC MIPI Rx drivering setting 85% */
		data = 0x4b;
		ret += nt51021_write_byte(cmd, data);

		cmd = 0x83; /* Page setting */
		data = 0xbb;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x22;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x90; /* CABC setting */
		data = cabc_mode;
		ret += nt51021_write_byte(cmd, data);
		break;
	case BOE:
		LCM_DBG("Restore BOE registers");
		cmd = 0x83; /* Page setting */
		data = 0x00;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x00;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x8c; /* GOP setting */
		data = 0x80;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xcd; /* 3Dummy */
		data = 0x6c;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xc0; /* GCH */
		data = 0x8b;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xc8; /* GCH */
		data = 0xf0;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x97; /* Resistor setting 100ohm */
		data = 0x00;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x8b;
		data = 0x10;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xa9; /* Enable TP_SYNC */
		data = 0x20;
		ret += nt51021_write_byte(cmd, data);

		cmd = 0x83; /* Page setting */
		data = 0xaa;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x11;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0xa9; /* IC MIPI Rx drivering setting 85% */
		data = 0x4b;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x85; /* Test mode1 */
		data = 0x04;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x86; /* Test mode2 */
		data = 0x08;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x9c; /* Test mode3 */
		data = 0x10;
		ret += nt51021_write_byte(cmd, data);

		cmd = 0x83; /* Page setting */
		data = 0xbb;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x84; /* Page setting */
		data = 0x22;
		ret += nt51021_write_byte(cmd, data);
		cmd = 0x90; /* CABC setting */
		data = cabc_mode;
		ret += nt51021_write_byte(cmd, data);
		break;
	default:
		LCM_DBG("Not supported yet");
		ret = -1;
		break;
	}
	if (ret < 0)
		LCM_DBG("i2c write error");
	else
		LCM_DBG("i2c write success");
}

static int nt51021_esd_check_worker_kthread(void *unused)
{
	struct sched_param param = {.sched_priority = 87 }; /* RTPM_PRIO_FB_THREAD */
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (start_esd_check_thread) {

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(2000)); /* esd check every 2s */

		if (kthread_should_stop())
			break;

		ret = nt51021_esd_check();

		if (ret > 0) {
			lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
			MDELAY(5);
			lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
			LCM_DBG("Unexpected MIPI status - trigger RESET");
			MDELAY(20);
			restore_lcm_registers();
		}
	}
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------------------
 */

static ssize_t esd_recover_enable_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int cnt;

	if (start_esd_check_thread == 0)
		cnt = snprintf(buf, PAGE_SIZE, "off\n");
	else if (start_esd_check_thread == 1)
		cnt = snprintf(buf, PAGE_SIZE, "on\n");
	else
		cnt = 0;

	return cnt;
}

static ssize_t esd_recover_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int start_flag;

	if (atomic_read(&suspend_state)) {
		LCM_DBG("Skip esd_recover_enable in suspend state");
		return -EINVAL;
	}

	if (0 == strncmp(buf, "off", 3))
		start_flag = 0;
	else if (0 == strncmp(buf, "on", 2))
		start_flag = 1;
	else
		return -EINVAL;

	if ((!start_esd_check_thread) && (start_flag)) {
		if (nt51021_i2c_client != NULL && nt51021_esd_check_task == NULL) {
			nt51021_esd_check_task = kthread_run(nt51021_esd_check_worker_kthread, 0, "nt51021_esd_check");
			if (IS_ERR(nt51021_esd_check_task)) {
				nt51021_esd_check_task = NULL;
				LCM_DBG("Failed to create kernel thread: %ld", PTR_ERR(nt51021_esd_check_task));
			}
		}
	}

	if ((start_esd_check_thread) && (!start_flag)) {
		if (nt51021_esd_check_task)
			kthread_stop(nt51021_esd_check_task);
		nt51021_esd_check_task = NULL;
	}

	start_esd_check_thread = start_flag;

	return size;
}

static DEVICE_ATTR(esd_recover_enable, S_IRUGO | S_IWUSR, esd_recover_enable_show, esd_recover_enable_store);

static struct attribute *nt51021_attributes[] = {
	&dev_attr_esd_recover_enable.attr,
	NULL,
};

static const struct attribute_group nt51021_attr_group = {
	.attrs = nt51021_attributes,
};
#endif

static int nt51021_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	u8 val;

	LCM_DBG("name=%s, addr=0x%x", client->name, client->addr);
	nt51021_i2c_client = client;

	/* Check if panel is connectted, if not return -1 */
	ret = nt51021_read_byte(0x83, &val);
	if (ret < 0) {
		LCM_DBG("Chip does not exits. I2C transfer error");
		nt51021_i2c_client = NULL;
#ifdef NT51021_ESD_DETECT
		start_esd_check_thread = 0;
#endif
	}

#ifdef NT51021_ESD_DETECT
	if (nt51021_i2c_client != NULL) {
		ret = sysfs_create_group(&client->dev.kobj, &nt51021_attr_group);
		if (ret)
			LCM_DBG("Error creating sysfs entries");
	}
#endif
	return 0;
}

static int nt51021_remove(struct i2c_client *client)
{
	nt51021_i2c_client = NULL;

#ifdef NT51021_ESD_DETECT
	if (nt51021_esd_check_task) {
		kthread_stop(nt51021_esd_check_task);
		nt51021_esd_check_task = NULL;
	}

	sysfs_remove_group(&client->dev.kobj, &nt51021_attr_group);
#endif
	i2c_unregister_device(client);

	return 0;
}

static int __init nt51021_iic_init(void)
{
	LCM_DBG("nt51021_iic_init");
	i2c_add_driver(&nt51021_iic_driver);
	return 0;
}

static void __exit nt51021_iic_exit(void)
{
	LCM_DBG("nt51021_iic_exit");
	i2c_del_driver(&nt51021_iic_driver);
}


module_init(nt51021_iic_init);
module_exit(nt51021_iic_exit);

static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	get_lcm_id();

	switch (vendor_id) {
	case INX:
		LCM_DBG("Init INX registers");

		if (cmd_sel == 0) {
			LCM_DBG("Force sending by MIPI");
			data_array[0] = 0xA58F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
			MDELAY(1);
		}

		data_array[0] = 0x00011500;	/* Software reset */
		dsi_set_cmdq(data_array, 1, 1);
		MDELAY(20);

		if (cmd_sel == 0) {
			LCM_DBG("Force sending by MIPI");
			data_array[0] = 0xA58F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
			MDELAY(1);
		}

		data_array[0] = 0x00831500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x00841500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);

		/* BIST test */
		/*data_array[0] = 0x06911500;
		dsi_set_cmdq(data_array, 1, 1);*/

		data_array[0] = 0x808c1500; /* GOP setting */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x50c71500; /* STV & RST falling timing */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x50c51500; /* GOP clock falling location */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x04851500; /* Test mode1 */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x08861500; /* Test mode2 */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x20A91500; /* Enable TP_SYNC */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0xaa831500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x11841500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x109c1500; /* Test mode3 */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x4ba91500; /* IC MIPI Rx drivering setting 85% */
		dsi_set_cmdq(data_array, 1, 1);

		if (cmd_sel == 0) {
			LCM_DBG("Exit force sending");
			data_array[0] = 0x008F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
		}
		MDELAY(5);
		break;
	case BOE:
		LCM_DBG("Init BOE registers");

		if (cmd_sel == 0) {
			LCM_DBG("Force sending by MIPI");
			data_array[0] = 0xA58F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
			MDELAY(1);
		}

		data_array[0] = 0x00011500;	/* Software reset */
		dsi_set_cmdq(data_array, 1, 1);
		MDELAY(20);

		if (cmd_sel == 0) {
			LCM_DBG("Force sending by MIPI");
			data_array[0] = 0xA58F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
			MDELAY(1);
		}

		data_array[0] = 0x00831500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x00841500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);

		/* BIST test */
		/*data_array[0] = 0x06911500;
		dsi_set_cmdq(data_array, 1, 1);*/

		data_array[0] = 0x808c1500; /* GOP setting */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x6ccd1500; /* 3Dummy */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x8bc01500; /* GCH */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0xf0c81500; /* GCH */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x00971500; /* Resistor setting 100ohm */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x108b1500;
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x20A91500; /* Enable TP_SYNC */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0xaa831500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x11841500; /* Page setting */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x4ba91500; /* IC MIPI Rx drivering setting 85% */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x04851500; /* Test mode1 */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x08861500; /* Test mode2 */
		dsi_set_cmdq(data_array, 1, 1);
		data_array[0] = 0x109c1500; /* Test mode3 */
		dsi_set_cmdq(data_array, 1, 1);

		data_array[0] = 0x00110500; /* Sleep Out */
		dsi_set_cmdq(data_array, 1, 1);

		if (cmd_sel == 0) {
			LCM_DBG("Exit force sending");
			data_array[0] = 0x008F1500; /* Command interface selection */
			dsi_set_cmdq(data_array, 1, 1);
		}
		MDELAY(5);
		break;
	default:
		LCM_DBG("Not supported yet");
		break;
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	LCM_DBG();

	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_EVENT_VDO_MODE;
#endif

	params->dsi.LANE_NUM = LCM_FOUR_LANE;

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;
	params->dsi.intermediat_buffer_num = 0;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 1;
	params->dsi.vertical_backporch = 14;
	params->dsi.vertical_frontporch = 11;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 1;
	params->dsi.horizontal_backporch = 32;
	params->dsi.horizontal_frontporch = 110;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 490;
	params->dsi.clk_lp_per_line_enable = 1;
	/* params->dsi.ssc_disable = 1; */
    params->dsi.cont_clock = 1; /* Keep HS serial clock running */
    params->dsi.DA_HS_EXIT = 1;
	params->dsi.CLK_ZERO = 16;
	params->dsi.HS_ZERO = 19;
	params->dsi.HS_TRAIL = 1;
	params->dsi.CLK_TRAIL = 1;
	params->dsi.CLK_HS_POST = 8;
	params->dsi.CLK_HS_EXIT = 6;
	/* params->dsi.CLK_HS_PRPR = 1; */

	params->dsi.TA_GO = 8;
	params->dsi.TA_GET = 10;

	params->dsi.cust_clk_impendence = 0xf;

	/* The values are in mm.
	 * Add these values for reporting correct xdpi and ydpi values */
	params->physical_width = 136;
	params->physical_height = 221;
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	LCM_DBG();

#if defined(UFBL_FEATURE_IDME)
	/* get board_id */
	char *boardversion = board_version();

	LCM_DBG("board_id %s read", boardversion);

	if (strncmp("proto", boardversion, strlen(boardversion)) == 0) {
		cmd_sel = 1;
		gpio_rst = GPIO_LCD_RST_HVT;
	} else if (strncmp("hvt", boardversion, strlen(boardversion)) == 0) {
		cmd_sel = 1;
		gpio_rst = GPIO_LCD_RST_HVT;
	} else {
		cmd_sel = 0;
		gpio_rst = GPIO_LCD_RST_EVT;
	}
#endif

	/* VGP6_PMU       3V3_LCM_VCC33 for panel power */
	upmu_set_rg_vgp6_vosel(0x7);
	upmu_set_rg_vgp6_sw_en(0x1);

	lcm_set_gpio_output(gpio_rst, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(gpio_rst, GPIO_OUT_ONE);
	MDELAY(5);

	init_lcm_registers();
#else
#ifdef CONFIG_IDME
	unsigned int board_type;
	unsigned int board_rev;

	board_type = idme_get_board_type();
	board_rev = idme_get_board_rev();
	LCM_DBG("board_type:0x%x, rev:0x%x", board_type, board_rev);
	if (board_type == BOARD_TYPE_SUEZ) {
		switch (board_rev) {
		case BOARD_REV_PROTO_0:
			cmd_sel = 1;
			break;
		case BOARD_REV_HVT_0:
			cmd_sel = 1;
			break;
		default:
			cmd_sel = 0;
		};
	}
#endif
	LCM_DBG("Skip power_on & init lcm since it's done by lk");

#ifdef NT51021_ESD_DETECT
	if (nt51021_i2c_client != NULL) {
		if (start_esd_check_thread) {
			nt51021_esd_check_task = kthread_run(nt51021_esd_check_worker_kthread, 0, "nt51021_esd_check");
			if (IS_ERR(nt51021_esd_check_task)) {
				nt51021_esd_check_task = NULL;
				LCM_DBG("failed to create kernel thread: %ld", PTR_ERR(nt51021_esd_check_task));
			}
		}
	}
#endif

#endif
	LCM_DBG("cmd_sel = 0x%x", cmd_sel);
}

/* 0ms < T8 < 20ms, T8: latency between MIPI path power off and VDD off
 * Separate lcm VDD off from suspend procedure and would be called after
 * MIPI path power off */
static void lcm_suspend_power(void)
{
	LCM_DBG();

#ifdef BUILD_LK
#else
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(5);
	lcm_vgp_supply_disable();
#endif
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	LCM_DBG();

#ifdef BUILD_LK
#else
#ifdef NT51021_ESD_DETECT
	if (start_esd_check_thread && nt51021_esd_check_task) {
		kthread_stop(nt51021_esd_check_task);
		nt51021_esd_check_task = NULL;
	}

	atomic_set(&suspend_state, 1);
#endif

	if (cmd_sel == 0) {
		LCM_DBG("Force sending by MIPI");
		data_array[0] = 0xA58F1500; /* Command interface selection */
		dsi_set_cmdq(data_array, 1, 1);
		MDELAY(1);
	}

	data_array[0] = 0x00100500; /* Sleep In */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(50);
#endif
}

/* 1ms < T2 < 20ms, T2: latency between LCM reset and MIPI path power on
 * Separate lcm VDD on and reset from resume procedure and it would be
 * called before MIPI path power on */
static void lcm_resume_power(void)
{
	/* unsigned int data_array[16]; */

	LCM_DBG();

#ifdef BUILD_LK
#else
	lcm_vgp_supply_enable();
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(5);
#endif
}

static void lcm_resume(void)
{
	LCM_DBG();

#ifdef BUILD_LK
#else
	MDELAY(20);
	init_lcm_registers();

#ifdef NT51021_ESD_DETECT
	atomic_set(&suspend_state, 0);
	if (nt51021_i2c_client != NULL) {
		if (start_esd_check_thread) {
			nt51021_esd_check_task = kthread_run(nt51021_esd_check_worker_kthread, 0, "nt51021_esd_check");
			if (IS_ERR(nt51021_esd_check_task)) {
				nt51021_esd_check_task = NULL;
				LCM_DBG("Failed to create kernel thread: %ld", PTR_ERR(nt51021_esd_check_task));
			}
		}
	}
#endif
#endif
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00290508;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
#endif

static void lcm_setbacklight_mode(unsigned int mode)
{
	u8 cmd, data;
	int ret = 0;
	LCM_DBG("Send CABC mode 0x%x via i2c", mode);

	cabc_mode = mode;

	cmd = 0x83;
	data = 0xbb;

	ret += nt51021_write_byte(cmd, data);

	cmd = 0x84;
	data = 0x22;

	ret += nt51021_write_byte(cmd, data);

	cmd = 0x90;
	data = mode;

	ret += nt51021_write_byte(cmd, data);

	if (ret < 0)
		LCM_DBG("i2c write error");
	else
		LCM_DBG("i2c write success");
}

LCM_DRIVER nt51021_wuxga_dsi_vdo_lcm_drv = {
	.name = "nt51021_wuxga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend_power = lcm_suspend_power,
	.suspend = lcm_suspend,
	.resume_power = lcm_resume_power,
	.resume = lcm_resume,
	.set_backlight_mode = lcm_setbacklight_mode,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};
