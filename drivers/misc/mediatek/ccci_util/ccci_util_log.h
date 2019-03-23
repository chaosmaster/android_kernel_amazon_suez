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

#ifndef __CCCI_UTIL_LOG_H__
#define __CCCI_UTIL_LOG_H__

/* No MD id message part */
#define CCCI_UTIL_DBG_MSG(fmt, args...)\
do {} while (0)/*	pr_debug("[ccci0/util]" fmt, ##args) */

#define CCCI_UTIL_INF_MSG(fmt, args...)\
do {} while (0)/*	pr_debug("[ccci0/util]" fmt, ##args) */

#define CCCI_UTIL_ERR_MSG(fmt, args...)\
	pr_err("[ccci0/util]" fmt, ##args)

/* With MD id message part */
#define CCCI_UTIL_DBG_MSG_WITH_ID(id, fmt, args...)\
do {} while (0)/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args) */

#define CCCI_UTIL_INF_MSG_WITH_ID(id, fmt, args...)\
do {} while (0)/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args) */

#define CCCI_UTIL_NOTICE_MSG_WITH_ID(id, fmt, args...) \
	pr_notice("[ccci%d/util]" fmt, (id+1), ##args)

#define CCCI_UTIL_ERR_MSG_WITH_ID(id, fmt, args...)\
	pr_err("[ccci%d/util]" fmt, (id+1), ##args)

#endif /*__CCCI_UTIL_LOG_H__ */
