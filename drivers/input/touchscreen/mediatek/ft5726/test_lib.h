/*
 * Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
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
* File Name: Test_lib.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test entry for all IC
*
************************************************************************/
#ifndef _TEST_LIB_H
#define _TEST_LIB_H

#define boolean unsigned char
#define bool unsigned char
#define BYTE unsigned char
#define false 0
#define true  1
#define FTS_DRIVER_LIB_INFO  "Test_Lib_Version  V1.4.0 2015-12-03"
/* IIC communication */
typedef int (*FTS_I2C_READ_FUNCTION)	\
	(unsigned char *, int , unsigned char *, int);
typedef int (*FTS_I2C_WRITE_FUNCTION)(unsigned char *, int);

extern FTS_I2C_READ_FUNCTION fts_i2c_read_test;
extern FTS_I2C_WRITE_FUNCTION fts_i2c_write_test;

extern int init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read);
extern int init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write);

/* about test */
/* load config */
int set_param_data(char *TestParamData);
 /* test entry */
boolean start_test_tp(void);
/* test result data.
 * (pTestData, External application for memory, buff size >= 1024*80) */
int get_test_data(char *pTestData);

void free_test_param_data(void);
int show_lib_ver(char *pLibVer);

#endif
