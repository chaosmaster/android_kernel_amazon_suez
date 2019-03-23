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
#ifndef __CMDQ_FENCE_H__
#define __CMDQ_FENCE_H__

/*
**	public functions
*/
#define MTK_FB_INVALID_FENCE_FD (-1)
/*
**	return 0 if success;
*/
int32_t cmdqFenceGetFence(struct cmdqFenceStruct *pFence);

/*
**	return release status
**	0 : success
**	-1 : release fail
*/
int32_t cmdqFenceReleaseFence(struct cmdqFenceStruct fence);

/*
**	create timeline for fence
**	return 0 for success, -1 for fail
*/
int32_t cmdqFenceCreateTimeLine(void);

#endif
