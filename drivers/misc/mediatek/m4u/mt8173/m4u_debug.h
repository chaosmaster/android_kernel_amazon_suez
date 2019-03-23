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

#ifndef __M4U_DEBUG_H__
#define __M4U_DEBUG_H__

extern __attribute__((weak)) int ddp_mem_test(void);
extern __attribute__((weak)) int __ddp_mem_test(unsigned int *pSrc, unsigned int pSrcPa,
			    unsigned int *pDst, unsigned int pDstPa,
			    int need_sync);
#endif
