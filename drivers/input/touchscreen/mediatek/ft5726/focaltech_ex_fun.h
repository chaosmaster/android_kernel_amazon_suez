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
* File Name: focaltech_ex_fun.h
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: function for fw upgrade, adb command, create apk second entrance
*
************************************************************************/
#ifndef __LINUX_fts_EX_FUN_H__
#define __LINUX_fts_EX_FUN_H__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/uaccess.h>

#define FTXXXX_DATA_FILEPATH "/data/"
#define FTXXXX_DIAG_FILEPATH "/dfs/output_data/"

/* #define AUTO_CLB */
#define HIDTOI2C_DISABLE		0

/* FT5726 register address */
#define FTS_UPGRADE_AA		0xAA
#define FTS_UPGRADE_55		0x55
#define FTS_PACKET_LENGTH	128
#define FTS_UPGRADE_LOOP	3
#define UPGRADE_RETRY_LOOP	3
#define FTS_FACTORYMODE_VALUE	0x40
#define FTS_WORKMODE_VALUE	0x00

#define FTS_REG_CHIP_ID	0xA3
#define FTS_REG_FW_VER		0xA6
#define FTS_REG_POINT_RATE	0x88
#define FTS_REG_CHARGER_STATE 0x8B
#define FTS_REG_VENDOR_ID	0xA8
#define FTS_REG_ROUTING_TYPE	0xAC
#define IC_INFO_DELAY_AA	20
#define IC_INFO_DELAY_55	20
#define IC_INFO_UPGRADE_ID1	0x58
#define IC_INFO_UPGRADE_ID2	0x2c
/* define for buffer size */
#define TESTDATA_SIZE (1024 * 120)

extern unsigned char hw_rev;
extern int apk_debug_flag;
extern struct tpd_device *tpd;
extern unsigned char ft_vendor_id;
extern unsigned char ft_routing_type;
#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
extern void kpd_tpd_wakeup_handler(unsigned long pressed);
extern int gesture_wakeup_enabled;
extern struct mutex fts_gesture_wakeup_mutex;
#endif
int hid_to_i2c(struct i2c_client *client);
int fts_ctpm_auto_upgrade(struct i2c_client *client);
int fts_create_sysfs(struct i2c_client *client);
int fts_create_apk_debug_channel(struct i2c_client *client);
void fts_release_apk_debug_channel(void);
void fts_release_sysfs(struct i2c_client *client);
#endif
