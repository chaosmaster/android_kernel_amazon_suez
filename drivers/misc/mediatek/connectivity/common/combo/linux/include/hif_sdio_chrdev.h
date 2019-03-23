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

#ifndef _HIF_SDIO_CHRDEV_H_

#define _HIF_SDIO_CHRDEV_H_


#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>


#include "wmt_exp.h"
#include "wmt_plat.h"
#ifdef CFG_WMT_PS_SUPPORT
#undef CFG_WMT_PS_SUPPORT
#endif


extern INT32 hif_sdio_create_dev_node(VOID);
extern INT32 hif_sdio_remove_dev_node(VOID);
extern INT32 hifsdiod_start(VOID);
extern INT32 hifsdiod_stop(VOID);
INT32 hif_sdio_match_chipid_by_dev_id(const struct sdio_device_id *id);
INT32 hif_sdio_is_chipid_valid(INT32 chipId);



#endif /*_HIF_SDIO_CHRDEV_H_*/
