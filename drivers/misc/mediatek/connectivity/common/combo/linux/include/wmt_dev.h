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


#ifndef _WMT_DEV_H_
#define _WMT_DEV_H_


#include "osal.h"

extern VOID wmt_dev_rx_event_cb(VOID);
extern INT32 wmt_dev_rx_timeout(P_OSAL_EVENT pEvent);
extern INT32 wmt_dev_patch_get(PUINT8 pPatchName, osal_firmware **ppPatch, INT32 padSzBuf);
extern INT32 wmt_dev_patch_put(osal_firmware **ppPatch);
extern VOID wmt_dev_patch_info_free(VOID);
extern INT32 wmt_dev_tm_temp_query(VOID);

#endif /*_WMT_DEV_H_*/
