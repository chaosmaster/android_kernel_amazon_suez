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


#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/trusty/trusty_ipc.h>

#include "kree_tipc.h"

#define REPLY_TIMEOUT                   5000
#define TXBUF_TIMEOUT                   15000

enum tipc_chan_state {
	TIPC_DISCONNECTED = 0,
	TIPC_CONNECTING,
	TIPC_CONNECTED,
	TIPC_STALE,
};

static void _free_shareable_mem(size_t sz, void *va, phys_addr_t pa)
{
	free_pages_exact(va, sz);
}

static void _free_msg_buf(struct tipc_msg_buf *mb)
{
	_free_shareable_mem(mb->buf_sz, mb->buf_va, mb->buf_pa);
	kfree(mb);
}

static void _free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb = NULL;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	while (mb) {
		list_del(&mb->node);
		_free_msg_buf(mb);
		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

struct tipc_dn_chan {
	int state;
	struct mutex lock; /* protects rx_msg_queue list and channel state */
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct completion reply_comp;
	struct list_head rx_msg_queue;
};

static int dn_wait_for_reply(struct tipc_dn_chan *dn, int timeout)
{
	int ret;

	ret = wait_for_completion_interruptible_timeout(&dn->reply_comp,
					msecs_to_jiffies(timeout));
	if (ret < 0)
		return ret;

	mutex_lock(&dn->lock);
	if (!ret) {
		/* no reply from remote */
		dn->state = TIPC_STALE;
		ret = -ETIMEDOUT;
	} else {
		/* got reply */
		if (dn->state == TIPC_CONNECTED)
			ret = 0;
		else if (dn->state == TIPC_DISCONNECTED)
			if (!list_empty(&dn->rx_msg_queue))
				ret = 0;
			else
				ret = -ENOTCONN;
		else
			ret = -EIO;
	}
	mutex_unlock(&dn->lock);

	return ret;
}

static struct tipc_msg_buf *_dn_handle_msg(void *data, struct tipc_msg_buf *rxbuf)
{
	struct tipc_dn_chan *dn = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(dn->chan);
		if (newbuf) {
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &dn->rx_msg_queue);
			wake_up_interruptible(&dn->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			pr_err("%s: discard incoming message\n", __func__);
		}
	}
	mutex_unlock(&dn->lock);

	return newbuf;
}

static void dn_connected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_CONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	mutex_unlock(&dn->lock);
}

static void dn_disconnected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_DISCONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_shutdown(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);

	/* set state to STALE */
	dn->state = TIPC_STALE;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_handle_event(void *data, int event)
{
	struct tipc_dn_chan *dn = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		dn_shutdown(dn);
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		dn_disconnected(dn);
		break;

	case TIPC_CHANNEL_CONNECTED:
		dn_connected(dn);
		break;

	default:
		pr_err("%s: unhandled event %d\n", __func__, event);
		break;
	}
}

static struct tipc_chan_ops _dn_ops = {
	.handle_msg = _dn_handle_msg,
	.handle_event = dn_handle_event,
};

static int tipc_open_channel(struct tipc_dn_chan **o_dn)
{
	int ret;
	struct tipc_dn_chan *dn;

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn) {
		ret = -ENOMEM;
		goto err_alloc_chan;
	}

	mutex_init(&dn->lock);
	init_waitqueue_head(&dn->readq);
	init_completion(&dn->reply_comp);
	INIT_LIST_HEAD(&dn->rx_msg_queue);

	dn->state = TIPC_DISCONNECTED;

	dn->chan = tipc_create_channel(NULL, &_dn_ops, dn);
	if (IS_ERR(dn->chan)) {
		ret = PTR_ERR(dn->chan);
		goto err_create_chan;
	}

	*o_dn = dn;
	return 0;

err_create_chan:
	kfree(dn);
err_alloc_chan:
	return ret;
}

int tipc_k_connect(tipc_k_handle *h, const char *port)
{
	int err;
	struct tipc_dn_chan *dn = NULL;

	err = tipc_open_channel(&dn);
	if (err)
		return err;

	*h = (tipc_k_handle)dn;

	/* send connect request */
	err = tipc_chan_connect(dn->chan, port);
	if (err)
		return err;

	/* and wait for reply */
	return dn_wait_for_reply(dn, REPLY_TIMEOUT);
}
EXPORT_SYMBOL(tipc_k_connect);

int tipc_k_disconnect(tipc_k_handle h)
{
	struct tipc_dn_chan *dn = (struct tipc_dn_chan *)h;

	dn_shutdown(dn);

	/* free all pending buffers */
	_free_msg_buf_list(&dn->rx_msg_queue);

	/* shutdown channel  */
	tipc_chan_shutdown(dn->chan);

	/* and destroy it */
	tipc_chan_destroy(dn->chan);

	kfree(dn);

	return 0;
}
EXPORT_SYMBOL(tipc_k_disconnect);

static inline bool _got_rx(struct tipc_dn_chan *dn)
{
	if (dn->state != TIPC_CONNECTED)
		return true;

	if (!list_empty(&dn->rx_msg_queue))
		return true;

	return false;
}

ssize_t tipc_k_read(tipc_k_handle h, void *buf, size_t buf_len, unsigned int flags)
{
	ssize_t ret;
	size_t  data_len;
	struct tipc_msg_buf *mb;
	struct tipc_dn_chan *dn = (struct tipc_dn_chan *)h;

	mutex_lock(&dn->lock);

	while (list_empty(&dn->rx_msg_queue)) {
		if (dn->state != TIPC_CONNECTED) {
			if (dn->state == TIPC_CONNECTING)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_DISCONNECTED)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_STALE)
				ret = -ESHUTDOWN;
			else
				ret = -EBADFD;
			goto out;
		}

		mutex_unlock(&dn->lock);

		if (flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(dn->readq, _got_rx(dn)))
			return -ERESTARTSYS;

		mutex_lock(&dn->lock);
	}

	mb = list_first_entry(&dn->rx_msg_queue, struct tipc_msg_buf, node);

	data_len = mb_avail_data(mb);
	if (data_len > buf_len) {
		ret = -EMSGSIZE;
		goto out;
	}

	memcpy(buf, mb_get_data(mb, data_len), data_len);

	ret = data_len;
	list_del(&mb->node);
	tipc_chan_put_rxbuf(dn->chan, mb);

out:
	mutex_unlock(&dn->lock);
	return ret;
}
EXPORT_SYMBOL(tipc_k_read);

ssize_t tipc_k_write(tipc_k_handle h, void *buf, size_t len, unsigned int flags)
{
	ssize_t ret;
	long timeout = TXBUF_TIMEOUT;
	struct tipc_msg_buf *txbuf = NULL;
	struct tipc_dn_chan *dn = (struct tipc_dn_chan *)h;

	if (flags & O_NONBLOCK)
		timeout = 0;

	txbuf = tipc_chan_get_txbuf_timeout(dn->chan, timeout);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* check available space */
	if (len > mb_avail_space(txbuf)) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	/* copy in message data */
	memcpy(mb_put_data(txbuf, len), buf, len);

	/* queue message */
	ret = tipc_chan_queue_msg(dn->chan, txbuf);
	if (ret)
		goto err_out;

	return len;

err_out:
	tipc_chan_put_txbuf(dn->chan, txbuf);
	return ret;
}
EXPORT_SYMBOL(tipc_k_write);

