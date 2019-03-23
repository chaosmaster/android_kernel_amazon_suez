/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LCM_UTIL_H__
#define __LCM_UTIL_H__

#include "lcm_drv.h"
#include "lcm_common.h"


LCM_STATUS lcm_util_set_data(const LCM_UTIL_FUNCS *lcm_util, char type, LCM_DATA_T1 *t1);
LCM_STATUS lcm_util_set_write_cmd_v1(const LCM_UTIL_FUNCS *lcm_util, LCM_DATA_T5 *t5, unsigned char force_update);
LCM_STATUS lcm_util_set_write_cmd_v2(const LCM_UTIL_FUNCS *lcm_util, LCM_DATA_T3 *t3, unsigned char force_update);
LCM_STATUS lcm_util_set_read_cmd_v2(const LCM_UTIL_FUNCS *lcm_util, LCM_DATA_T4 *t4, unsigned int *compare);


#endif

