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


#ifndef __KREE_TIPC_H
#define __KREE_TIPC_H

#include <linux/types.h>

typedef void *tipc_k_handle;
int tipc_k_connect(tipc_k_handle *h, const char *port);
int tipc_k_disconnect(tipc_k_handle h);
ssize_t tipc_k_read(tipc_k_handle h, void *buf, size_t buf_len, unsigned int flags);
ssize_t tipc_k_write(tipc_k_handle h, void *buf, size_t len, unsigned int flags);

#endif
