/*
 * asus_mt8173_accdet.h -- header of headset detection driver
 *
 * Copyright 2015 ASUSTek COMPUTER INC.
 * Author: Karl Yu <Karl_Yu@asus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASUS_MT8173_ACCDET_H__
#define __ASUS_MT8173_ACCDET_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <upmu_common.h>
#include "reg_accdet.h"

#define MULTIKEY_ADC_CHANNEL 5

#define ACCDET_JACK_MASK (SND_JACK_HEADPHONE | \
		SND_JACK_HEADSET | \
		SND_JACK_BTN_0 | \
		SND_JACK_BTN_1 | \
		SND_JACK_BTN_2 | \
		SND_JACK_BTN_3)

#define REGISTER_VALUE(x)   (x - 1)
#define TAG "[Accdet]"
#define NUM_BUTTONS 4
#define NO_KEY  0x0

enum accdet_irq_index {
	UART_DET = 0,
	JACK_DET,
	IRQ_MAX,
};

enum accdet_state {
	HEADPHONE = 0,
	HEADSET = 1,
	PLUGOUT = 3,
};

enum accdet_power_control {
	OFF = 0,
	ON,
};

struct headset_mode_settings {
	int pwm_width;	    /* pwm frequence */
	int pwm_thresh;	    /* pwm duty */
	int fall_delay;	    /* falling stable time */
	int rise_delay;	    /* rising stable time */
	int debounce0;	    /* hook switch or double check debounce */
	int debounce1;	    /* mic bias debounce */
	int debounce3;	    /* plug out debounce */
};

struct accdet_irq_setting {
	const char *name;
	irqreturn_t (*callback)(int irq, void *data);
	bool wake_src:1;
	unsigned int trigger_type;
	const char *pinctrl_name;
};

struct accdet_priv {
	struct snd_soc_jack jack;
	unsigned int irq[IRQ_MAX];
	unsigned int gpio[IRQ_MAX];
	struct headset_mode_settings hs_debounce;
	struct workqueue_struct *jack_workqueue;
	struct workqueue_struct *accdet_workqueue;
	struct work_struct jack_work;
	struct work_struct accdet_work;
	spinlock_t uart_spinlock;
	struct wake_lock accdet_init_wlock;
	struct wake_lock accdet_work_wlock;
	struct mutex jack_sync_mutex;
	bool jack_sync_flag:1;
	bool uart_inserted:1;
	bool button_enabled:1;
	unsigned int button_held;
	unsigned int pre_key;
	unsigned int jack_type;
	unsigned int btn_vol_th[NUM_BUTTONS+1];
	unsigned int vmode;
};

static irqreturn_t jack_handler(int irq, void *pdata);
static irqreturn_t uart_remove_handler(int irq, void *pdata);
static inline void clear_accdet_interrupt(void);
static inline void check_cable_type(void);
static inline void accdet_init(void);
static inline void enable_accdet(u32 state_swctrl);
static inline void disable_accdet(void);
static void accdet_work_callback(struct work_struct *work);
static inline void headset_plug_out(void);
static void enable_vol_detect(int enable);
static unsigned int key_check(int vol);
static void multi_key_detection(void);
static inline void check_cable_type(void);
static void jack_work_callback(struct work_struct *work);
static int headset_pin_setup(struct platform_device *pdev);
static int mt8173_accdet_probe(struct platform_device *dev);
static int mt8173_accdet_remove(struct platform_device *pdev);

extern s32 pwrap_read(u32 adr, u32 *rdata);
extern s32 pwrap_write(u32 adr, u32 wdata);
#endif
