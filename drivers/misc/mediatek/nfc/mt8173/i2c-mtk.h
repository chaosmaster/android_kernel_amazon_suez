/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Ming-Chih Lung <Ming-Chih.Lung@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __I2C_MTK
#define __I2C_MTK
#ifdef __cplusplus
extern "C" {
#endif

extern int mtk_i2c_set_speed(struct i2c_adapter *adap, u32 speed);

#ifdef __cplusplus
}
#endif
#endif


