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
* File Name: focaltech_ctl.h
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: Function for APK tool
*
************************************************************************/
#ifndef __FOCALTECH_CTL_H__
#define __FOCALTECH_CTL_H__

#define FTS_RW_IIC_DRV		"ft_rw_iic_drv"
#define FTS_RW_IIC_DRV_MAJOR	210
#define FTS_I2C_RDWR_MAX_QUEUE	36
#define FTS_I2C_SLAVEADDR	11
#define FTS_I2C_RW		12

struct fts_rw_i2c {
	u8 *buf;
	u8 flag;		/*0-write 1-read */
	__u16 length;		/*the length of data */
};

struct fts_rw_i2c_queue {
	struct fts_rw_i2c __user *i2c_queue;
	int queuenum;
};

int fts_rw_iic_drv_init(struct i2c_client *client);
void fts_rw_iic_drv_exit(void);
#endif
