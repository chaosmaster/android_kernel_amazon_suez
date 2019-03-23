/*
 * asus_mt8173_accdet.c -- headset detection driver
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

#include "asus_mt8173_accdet.h"

/* Button values to be reported on the jack */
static const unsigned int hs_btns[] = {
	SND_JACK_BTN_0,
	SND_JACK_BTN_1,
	SND_JACK_BTN_2,
	SND_JACK_BTN_3,
};

static const char * const btn_type[] = {
	"KEY_MEDIA",
	"KEY_VOICECOMMAND",
	"KEY_VOLUMEUP",
	"KEY_VOLUMEDOWN",
};

static struct accdet_priv *hs_data;

static struct accdet_irq_setting irqs_setting[] = {
	/* the index must be aligned with dts table and enum value */
	{
		/* UART_DET */
		.name = "uart_remove_det",
		.callback = uart_remove_handler,
		.wake_src = true,
		.trigger_type = IRQF_TRIGGER_RISING|IRQF_ONESHOT,
		.pinctrl_name = "uart-default",
	},
	{
		/* JACK_DET */
		.name = "headset_jack_det",
		.callback = jack_handler,
		.wake_src = true,
		.trigger_type = IRQF_TRIGGER_RISING|
			    IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
		.pinctrl_name = "jack-default",
	},
};

/* pmic wrap read and write func */
static u32 pmic_pwrap_read(u32 addr)
{
	u32 val = 0;

	pwrap_read(addr, &val);
	return val;
}

static void pmic_pwrap_write(unsigned int addr, unsigned int wdata)
{
	pwrap_write(addr, wdata);
}

void accdet_auxadc_switch(int enable)
{
	if (enable) {
		if (!hs_data->vmode)
			pmic_pwrap_write(ACCDET_RSV,
					ACCDET_1V9_MODE_ON);
		else
			pmic_pwrap_write(0x732,
					ACCDET_2V8_MODE_ON);
	} else {
		if (!hs_data->vmode)
			pmic_pwrap_write(ACCDET_RSV,
					ACCDET_1V9_MODE_OFF);
		else
			pmic_pwrap_write(0x732, 0);
	}
}
EXPORT_SYMBOL(accdet_auxadc_switch);

/* this interrupt handler is called by pmic driver,
   and can be triggered only when enable_accdet is called.
   After being triggered we must clear ACCDET_IRQ_STS[IRQ_CLR_BIT]
   to re-enable interruption */
int accdet_irq_handler(void)
{
	int retry = 0;

	pr_debug("%s %s: entry\n", TAG, __func__);
	/* set ACCDET_IRQ_STS[IRQ_CLR_BIT] as 1 to clear
	   ACCDET_IRQ_STS[IRQ_STATUS_BIT] to re-enable interruption */
	clear_accdet_interrupt();

	queue_work(hs_data->accdet_workqueue, &hs_data->accdet_work);
	while (((pmic_pwrap_read(ACCDET_IRQ_STS) &
			IRQ_STATUS_BIT) && retry < 10)) {
		retry++;
		udelay(200);
	}
	return 1;
}
EXPORT_SYMBOL(accdet_irq_handler);

static void accdet_work_callback(struct work_struct *work)
{
	unsigned int uart_gpio = hs_data->gpio[UART_DET];
	unsigned int jack_gpio = hs_data->gpio[JACK_DET];

	wake_lock(&hs_data->accdet_work_wlock);

	pr_debug("%s %s: entry\n", TAG, __func__);

	if (gpio_is_valid(uart_gpio) && gpio_is_valid(jack_gpio)) {
		if (!gpio_get_value(uart_gpio) ||
			    gpio_get_value(jack_gpio)) {
			pr_info("%s %s: already plug out\n",
				TAG, __func__);
			headset_plug_out();
			wake_unlock(&hs_data->accdet_work_wlock);
			return;
		}
	}

	check_cable_type();

	mutex_lock(&hs_data->jack_sync_mutex);
	/* Only report plug-in event here, plug-out event
	   should be addressed in headset_plug_out function */
	if (hs_data->jack_sync_flag) {
		snd_soc_jack_report(&hs_data->jack,
				hs_data->jack_type|hs_data->button_held,
				ACCDET_JACK_MASK);
		pr_debug("%s %s: headset status(0x%x), button(0x%x)\n",
			    __func__, TAG, hs_data->jack_type,
			    hs_data->button_held);
	} else
		pr_info("%s %s: Headset has plugged out\n",
			TAG, __func__);
	mutex_unlock(&hs_data->jack_sync_mutex);

	wake_unlock(&hs_data->accdet_work_wlock);
}

static inline void headset_plug_out(void)
{
	mutex_lock(&hs_data->jack_sync_mutex);
	hs_data->jack_type = 0;
	hs_data->button_enabled = false;
	hs_data->button_held = 0;
	hs_data->jack_sync_flag = false;
	snd_soc_jack_report(&hs_data->jack,
			hs_data->jack_type|hs_data->button_held,
			ACCDET_JACK_MASK);
	mutex_unlock(&hs_data->jack_sync_mutex);
	pr_info("%s %s: plug-out\n", TAG, __func__);
}

static void enable_vol_detect(int enable)
{
	if (enable) {
		accdet_auxadc_switch(enable);
		pmic_pwrap_write(ACCDET_PWM_WIDTH,
			    REGISTER_VALUE(hs_data->hs_debounce.pwm_width));
		pmic_pwrap_write(ACCDET_PWM_THRESH,
			    REGISTER_VALUE(hs_data->hs_debounce.pwm_width));
	} else {
		accdet_auxadc_switch(enable);
		pmic_pwrap_write(ACCDET_PWM_WIDTH,
			    REGISTER_VALUE(hs_data->hs_debounce.pwm_width));
		pmic_pwrap_write(ACCDET_PWM_THRESH,
			    REGISTER_VALUE(hs_data->hs_debounce.pwm_thresh));
	}
}

static unsigned int key_check(int vol)
{
	int i;

	pr_debug("%s %s: adc_data: %d mv",
			TAG, __func__, vol);
	for (i = NUM_BUTTONS; i > 0; i--) {
		if (vol < hs_data->btn_vol_th[i] &&
				vol >= hs_data->btn_vol_th[i-1]) {
			pr_debug("%s %s: detect %s\n",
					TAG, __func__, btn_type[i-1]);
			return hs_btns[i-1];
		}
	}
	pr_debug("%s %s: detect NO KEY\n", TAG, __func__);
	return NO_KEY;
}

static void multi_key_detection(void)
{
	unsigned int key_type = NO_KEY;
	int cali_voltage = 0;

	enable_vol_detect(ON);

	cali_voltage = PMIC_IMM_GetOneChannelValue(MULTIKEY_ADC_CHANNEL, 1, 1);
	key_type = key_check(cali_voltage);

	pr_debug("%s %s: key_type(0x%x),pre_key(0x%x)\n",
			TAG, __func__, key_type, hs_data->pre_key);

	if ((key_type == NO_KEY) && (hs_data->pre_key != NO_KEY)) {
		pr_debug("%s %s: key released\n", TAG, __func__);
		hs_data->button_held &= ~hs_data->pre_key;
	} else if ((hs_data->pre_key == NO_KEY) && (key_type != NO_KEY)) {
		pr_debug("%s %s: key pressed\n", TAG, __func__);
		hs_data->button_held |= key_type;
	}

	hs_data->pre_key = key_type;

	enable_vol_detect(OFF);
}

static inline void check_cable_type(void)
{
	int current_status = 0;
	int irq_temp = 0;
	int wait_clear_irq_times = 0;

	current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);
	/* AB=11:plug out, AB=01:headset, AB=00:headphone */
	pr_debug("%s %s: current AB = %d\n",
			TAG, __func__, current_status);

	mutex_lock(&hs_data->jack_sync_mutex);
	if (!hs_data->button_enabled) {
		switch (current_status) {
		case PLUGOUT:
			if (hs_data->jack_type || !hs_data->jack_sync_flag) {
				hs_data->jack_type = 0;
				hs_data->button_held = 0;
				hs_data->button_enabled = false;
				pr_info("%s %s: detect plug-out\n",
					TAG, __func__);
			} else
				pr_info("%s %s: already plug-out\n",
					TAG, __func__);
			break;
		case HEADPHONE:
			if (!hs_data->jack_type && hs_data->jack_sync_flag) {
				hs_data->jack_type |= SND_JACK_HEADPHONE;
				pr_info("%s %s: detect headphone\n",
					TAG, __func__);
			} else if (hs_data->jack_type)
				pr_info("%s %s: already plug-in (0x%x)\n",
					TAG, __func__, hs_data->jack_type);
			else
				pr_info("%s %s: already plug-out\n",
					TAG, __func__);
			break;
		case HEADSET:
			if (!hs_data->jack_type && hs_data->jack_sync_flag) {
				hs_data->jack_type |= SND_JACK_HEADSET;
				hs_data->button_enabled = true;
				hs_data->button_held = 0;
				pr_info("%s %s: detect headset\n",
					TAG, __func__);
			} else if (hs_data->jack_type)
				pr_info("%s %s: already plug-in (0x%x)\n",
					TAG, __func__, hs_data->jack_type);
			else
				pr_info("%s %s: already plug-out\n",
					TAG, __func__);
			break;
		default:
			pr_err("%s %s: unsupported AB status\n",
				TAG, __func__);
		}
	} else {
		multi_key_detection();
	}

	if (hs_data->jack_sync_flag) {
		while ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
				&& (wait_clear_irq_times < 3)) {
			pr_debug("%s %s: Clearing interrupt\n", TAG, __func__);
			wait_clear_irq_times++;
			msleep(20);
		}
	}
	irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
	irq_temp = irq_temp & (~IRQ_CLR_BIT);
	pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	mutex_unlock(&hs_data->jack_sync_mutex);
	pr_debug("%s %s: Clear interrupt:Done[0x%x]!\n",
			TAG, __func__, pmic_pwrap_read(ACCDET_IRQ_STS));
}

static irqreturn_t uart_remove_handler(int irq, void *pdata)
{
	unsigned int jack_irq = hs_data->irq[JACK_DET];
	unsigned long flags;

	pr_debug("%s %s: entry\n", TAG, __func__);
	spin_lock_irqsave(&hs_data->uart_spinlock, flags);
	if (hs_data->uart_inserted) {
		hs_data->uart_inserted = false;
		spin_unlock_irqrestore(&hs_data->uart_spinlock, flags);
		enable_irq(jack_irq);
		pr_info("%s %s: enable headset jack detection\n",
				TAG, __func__);
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&hs_data->uart_spinlock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t jack_handler(int irq, void *pdata)
{
	unsigned int uart_gpio = hs_data->gpio[UART_DET];
	unsigned int jack_irq = hs_data->irq[JACK_DET];
	unsigned long flags;

	pr_debug("%s %s: entry\n", TAG, __func__);
	if (gpio_is_valid(uart_gpio) ? !gpio_get_value(uart_gpio) : 0) {
		spin_lock_irqsave(&hs_data->uart_spinlock, flags);
		if (!hs_data->uart_inserted) {
			disable_irq_nosync(jack_irq);
			hs_data->uart_inserted = true;
			pr_info("%s %s: disable headset jack detection\n",
					TAG, __func__);
		}
		spin_unlock_irqrestore(&hs_data->uart_spinlock, flags);
		return IRQ_HANDLED;
	}

	disable_irq_nosync(jack_irq);

	queue_work(hs_data->jack_workqueue, &hs_data->jack_work);

	return IRQ_HANDLED;
}

static void jack_work_callback(struct work_struct *work)
{
	unsigned int jack_gpio = hs_data->gpio[JACK_DET];
	unsigned int jack_irq = hs_data->irq[JACK_DET];
	bool inserted = false; /*True: inserted, False: removed*/

	if (gpio_is_valid(jack_gpio))
		inserted = !gpio_get_value(jack_gpio);

	pr_info("%s %s: current jack state: %s\n",
			TAG, __func__, inserted ? "plug in" : "plug out");
	if (inserted) {
		mutex_lock(&hs_data->jack_sync_mutex);
		hs_data->jack_sync_flag = true;
		mutex_unlock(&hs_data->jack_sync_mutex);
		wake_lock(&hs_data->accdet_init_wlock);
		/* do set pwm_idle on in accdet_init */
		accdet_init();
		/* set PWM IDLE on */
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			(pmic_pwrap_read(ACCDET_STATE_SWCTRL) |
			    ACCDET_SWCTRL_IDLE_EN));
		/* enable ACCDET unit */
		enable_accdet(ACCDET_SWCTRL_EN);
		wake_unlock(&hs_data->accdet_init_wlock);
	} else {
		mutex_lock(&hs_data->jack_sync_mutex);
		hs_data->jack_sync_flag = false;
		mutex_unlock(&hs_data->jack_sync_mutex);
		disable_accdet();
		headset_plug_out();
	}
	enable_irq(jack_irq);
}

static inline void accdet_init(void)
{
	pr_debug("%s %s: entry\n", TAG, __func__);
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	/* reset the accdet unit */

	pmic_pwrap_write(TOP_RST_ACCDET_SET, ACCDET_RESET_SET);
	pmic_pwrap_write(TOP_RST_ACCDET_CLR, ACCDET_RESET_CLR);
	/* init pwm frequency and duty */
	pmic_pwrap_write(ACCDET_PWM_WIDTH,
			REGISTER_VALUE(hs_data->hs_debounce.pwm_width));
	pmic_pwrap_write(ACCDET_PWM_THRESH,
			REGISTER_VALUE(hs_data->hs_debounce.pwm_thresh));

	pwrap_write(ACCDET_STATE_SWCTRL, 0x07);

	pmic_pwrap_write(ACCDET_EN_DELAY_NUM,
			(hs_data->hs_debounce.fall_delay << 15 |
			 hs_data->hs_debounce.rise_delay));
	/* init the debounce time */
	pmic_pwrap_write(ACCDET_DEBOUNCE0, hs_data->hs_debounce.debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, hs_data->hs_debounce.debounce1);
	pmic_pwrap_write(ACCDET_DEBOUNCE3, hs_data->hs_debounce.debounce3);
	pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) &
				(~IRQ_CLR_BIT));
	pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_IRQ_SET);
	/* disable ACCDET unit */
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x0);
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
}

/* Accdet only need this func */
static inline void enable_accdet(u32 state_swctrl)
{
	pr_debug("%s %s: entry\n", TAG, __func__);
	/* enable ACCDET unit and enable clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL,
		pmic_pwrap_read(ACCDET_STATE_SWCTRL) | state_swctrl);
	pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) |
		ACCDET_ENABLE);
}

static inline void disable_accdet(void)
{
	int irq_temp = 0;
	int retry = 0;

	/* sync with accdet_irq_handler() which set clear_accdet_irq bit
	   to avoid set clear_accdet_irq bit" after disable_accdet()
	   disable accdet irq */
	pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR);
	clear_accdet_interrupt();
	udelay(200);
	mutex_lock(&hs_data->jack_sync_mutex);
	while ((pmic_pwrap_read(ACCDET_IRQ_STS) &
			IRQ_STATUS_BIT) && retry < 3) {
		pr_debug("%s %s: Clearing interrupt\n", TAG, __func__);
		retry++;
		msleep(20);
	}
	irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
	irq_temp = irq_temp & (~IRQ_CLR_BIT);
	pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	mutex_unlock(&hs_data->jack_sync_mutex);
	/* disable ACCDET unit */
	pr_debug("%s %s: disable_accdet\n", TAG, __func__);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	/* disable clock and Analog control */
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
}

/* clear ACCDET IRQ in accdet register */
static inline void clear_accdet_interrupt(void)
{
	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	pmic_pwrap_write(ACCDET_IRQ_STS, ((pmic_pwrap_read(ACCDET_IRQ_STS)) &
				    0x8000) | (IRQ_CLR_BIT));
	pr_debug("%s %s: ACCDET_IRQ_STS = 0x%x\n",
			TAG, __func__, pmic_pwrap_read(ACCDET_IRQ_STS));
}

static int headset_pin_setup(struct platform_device *pdev)
{
	struct device_node *node;
	struct device *dev;
	int i, ret = 0;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;
	int debounce[7];
	unsigned int btns_threshold[NUM_BUTTONS+1];

	dev = &pdev->dev;
	node = dev->of_node;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("%s %s: Can't find accdet pinctrl\n",
			TAG, __func__);
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(irqs_setting); i++) {
		/* request gpio and set its direction as input */
		hs_data->gpio[i] = of_get_gpio(node, i);
		if (gpio_is_valid(hs_data->gpio[i])) {
			ret = devm_gpio_request_one(dev,
					hs_data->gpio[i],
					GPIOF_DIR_IN,
					irqs_setting[i].name);
			if (ret < 0) {
				pr_err("%s %s: Failed to request gpio %s\n",
					TAG, __func__, irqs_setting[i].name);
				goto err;
			}
		} else {
			pr_err("%s %s: gpio %s is invalid\n",
				TAG, __func__, irqs_setting[i].name);
			goto err;
		}

		hs_data->irq[i] = irq_of_parse_and_map(node, i);

		/* set default pinctrl */
		pinctrl_default =
			pinctrl_lookup_state(pinctrl,
					irqs_setting[i].pinctrl_name);
		if (IS_ERR(pinctrl_default)) {
			ret = PTR_ERR(pinctrl_default);
			pr_err("%s %s: Can't find pinctrl %s\n",
				TAG, __func__, irqs_setting[i].pinctrl_name);
			goto err;
		}
		pinctrl_select_state(pinctrl, pinctrl_default);
	}

	/* request interrupt handler here to avoid getting
	   wrong gpio and irq num which are not parsed yet */
	for (i = 0; i < ARRAY_SIZE(irqs_setting); i++) {
		/* request irq and set it as wakeup source if need */
		ret = devm_request_irq(dev, hs_data->irq[i],
				irqs_setting[i].callback,
				irqs_setting[i].trigger_type,
				irqs_setting[i].name, NULL);
		if (ret) {
			pr_err("%s %s: Failed to request irq %s\n",
				TAG, __func__, irqs_setting[i].name);
			goto err;
		}
		if (irqs_setting[i].wake_src)
			ret = irq_set_irq_wake(hs_data->irq[i], 1);

		/* disable interrupt first until snd_jack input
		   device is registered */
		disable_irq_nosync(hs_data->irq[i]);
	}

	/* get debounce value */
	ret = of_property_read_u32_array(node, "headset-mode-setting",
			debounce, ARRAY_SIZE(debounce));
	if (ret)
		pr_err("%s %s: Failed to parse debounce value\n",
			    TAG, __func__);
	else
		memcpy(&hs_data->hs_debounce, debounce, sizeof(debounce));

	/* get voltage threshold of buttons */
	ret = of_property_read_u32_array(node, "headset-key-threshold",
			btns_threshold, ARRAY_SIZE(btns_threshold));
	if (ret)
		pr_err("%s %s: Failed to parse buttons threshold\n",
			    TAG, __func__);
	else
		memcpy(&hs_data->btn_vol_th, btns_threshold,
					    sizeof(btns_threshold));

	/* get accdet voltage mode vmode:0->1.9V, 1->2.8V */
	ret = of_property_read_u32(node, "accdet_voltage_mode",
			&hs_data->vmode);
	if (ret)
		pr_err("%s %s: Failed to parse accdet voltage mode\n",
			    TAG, __func__);

	return 0;
err:
	return ret;
}

int mt8173_accdet_enable(struct snd_soc_codec *codec)
{
	int ret;
	unsigned int uart_irq;
	unsigned int jack_irq;
	unsigned int jack_gpio;
	unsigned int uart_gpio;

	if (hs_data == NULL) {
		pr_err("%s %s: private data does not exist\n",
				TAG, __func__);
		return -EINVAL;
	}

	uart_irq = hs_data->irq[UART_DET];
	jack_irq = hs_data->irq[JACK_DET];
	uart_gpio = hs_data->gpio[UART_DET];
	jack_gpio = hs_data->gpio[JACK_DET];


	ret = snd_soc_jack_new(codec, "Headset Detection",
			ACCDET_JACK_MASK, &hs_data->jack);

	if (ret) {
		pr_err("%s %s: Failed to add new jack devices\n",
				TAG, __func__);
		return ret;
	}

	snd_jack_set_key(hs_data->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(hs_data->jack.jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(hs_data->jack.jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(hs_data->jack.jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	/* If the voltage of det pin is low at boot time,
	   do the first detection to check jack type */
	if (gpio_is_valid(jack_gpio) && gpio_is_valid(uart_gpio)) {
		if (!gpio_get_value(jack_gpio) && gpio_get_value(uart_gpio)) {
			hs_data->jack_sync_flag = true;
			accdet_init();
			pmic_pwrap_write(ACCDET_STATE_SWCTRL,
					(pmic_pwrap_read(ACCDET_STATE_SWCTRL) |
					 ACCDET_SWCTRL_IDLE_EN));
			enable_accdet(ACCDET_SWCTRL_EN);
		}
	}

	/* enable detection interrupt */
	enable_irq(jack_irq);
	enable_irq(uart_irq);

	return 0;
}
EXPORT_SYMBOL(mt8173_accdet_enable);

static int mt8173_accdet_probe(struct platform_device *pdev)
{
	struct accdet_priv *accdet_priv = NULL;
	int ret = 0;
	unsigned int default_debounce[7] = {
		    0x100, 0x100, 1, 0x1F0, 0x200, 0x200, 0x20};

	accdet_priv = devm_kzalloc(&pdev->dev,
			sizeof(struct accdet_priv), GFP_KERNEL);
	if (accdet_priv == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	hs_data = accdet_priv;

	/* spin lock */
	spin_lock_init(&hs_data->uart_spinlock);

	/* wake lock */
	wake_lock_init(&hs_data->accdet_init_wlock,
			WAKE_LOCK_SUSPEND, "accdet init wakelock");
	wake_lock_init(&hs_data->accdet_work_wlock,
			WAKE_LOCK_SUSPEND, "accdet work wakelock");

	/* mutex */
	mutex_init(&hs_data->jack_sync_mutex);

	/* create workqueue and work */
	hs_data->jack_workqueue =
		create_singlethread_workqueue("jack detection");
	INIT_WORK(&hs_data->jack_work, jack_work_callback);
	hs_data->accdet_workqueue =
		create_singlethread_workqueue("accdet");
	INIT_WORK(&hs_data->accdet_work, accdet_work_callback);

	/* initialize detection value */
	hs_data->jack_type = 0;
	hs_data->button_held = 0;
	hs_data->button_enabled = false;
	hs_data->uart_inserted = false;
	hs_data->jack_sync_flag = false;
	hs_data->pre_key = NO_KEY;
	hs_data->vmode = 0;
	memcpy(&hs_data->hs_debounce, default_debounce,
				    sizeof(default_debounce));
	memset(&hs_data->btn_vol_th, 0, NUM_BUTTONS+1);

	/* setup pin */
	ret = headset_pin_setup(pdev);
	if (ret != 0) {
		pr_err("%s %s: Failed to setup headset pins\n",
				TAG, __func__);
		goto err;
	}

	return 0;
err:
	return ret;
}

static int mt8173_accdet_remove(struct platform_device *pdev)
{
	destroy_workqueue(hs_data->jack_workqueue);
	destroy_workqueue(hs_data->accdet_workqueue);
	return 0;
}

static struct of_device_id asus_accdet_of_match[] = {
	{ .compatible = "asus,mt8173-accdet", },
};

static struct platform_driver mt8173_accdet_driver = {
	.probe = mt8173_accdet_probe,
	.remove = mt8173_accdet_remove,
	.driver = {
		.name = "MT8173_Accdet_Driver",
		.of_match_table = asus_accdet_of_match,
	},
};

static int mt8173_accdet_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mt8173_accdet_driver);
	if (ret)
		pr_err("%s %s: platform_driver_register error:(%d)\n",
				TAG, __func__, ret);
	else
		pr_info("%s %s: platform_driver_register done!\n",
				TAG, __func__);

	return ret;
}

static void mt8173_accdet_exit(void)
{
	platform_driver_unregister(&mt8173_accdet_driver);
}

module_init(mt8173_accdet_init);
module_exit(mt8173_accdet_exit);

MODULE_DESCRIPTION("ASUS MT8173 ACCDET driver");
MODULE_AUTHOR("Karl Yu <Karl_Yu@asus.com>");
MODULE_LICENSE("GPL");
