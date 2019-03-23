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
* File Name: Test_FT5822.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-23
*
* Abstract: test item for FT5822\FT5626\FT5726\FT5826B
*
************************************************************************/
#ifndef _TEST_FT5822_H
#define _TEST_FT5822_H
#include "test_lib.h"

#define IC_TEST_VERSION \
"Test version: V1.0.0--2015-07-30, (sync version of FT_MultipleTest: V2.7.0.3--2015-07-13)"
#define SHOW_INFO
/* Reg */
#define DEVIDE_MODE_ADDR	0x00
#define REG_LINE_NUM	0x01
#define REG_TX_NUM	0x02
#define REG_RX_NUM	0x03
#define REG_PATTERN_5422        0x53
#define REG_MAPPING_SWITCH      0x54
#define REG_TX_NOMAPPING_NUM        0x55
#define REG_RX_NOMAPPING_NUM      0x56
#define REG_NORMALIZE_TYPE      0x16
#define REG_ScCbBuf0	0x4E
#define REG_ScWorkMode	0x44
#define REG_ScCbAddrR	0x45
#define REG_RawBuf0 0x36
#define REG_WATER_CHANNEL_SELECT 0x09
#define REG_FREQUENCY           0x0A
#define REG_FIR                 0xFB
#define MIN_HOLE_LEVEL   (-1)
#define MAX_HOLE_LEVEL   0x7F
/* define for buffer size */
#define STOREMSGAREA_SIZE (1024*8)
#define TMPBUFF_SIZE (1024*8)
#define INFOBUFF_SIZE (1024*8)

boolean FT5822_StartTest(void);
/* pTestData, External application for memory, buff size >= 1024*80 */
int FT5822_get_test_data(char *pTestData);
unsigned char FT5822_TestItem_EnterFactoryMode(void);
unsigned char FT5822_TestItem_RawDataTest(bool *bTestResult);
unsigned char FT5822_TestItem_SCapRawDataTest(bool *bTestResult);
unsigned char FT5822_TestItem_SCapCbTest(bool *bTestResult);
unsigned char FT5822_TestItem_UniformityTest(bool *bTestResult);
unsigned char FT5822_TestItem_PanelDifferTest(bool *bTestResult);
boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);
#endif
