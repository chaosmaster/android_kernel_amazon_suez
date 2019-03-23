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

#include <xhci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/io.h>

#include <linux/irq.h>
#include <linux/switch.h>
#include <linux/module.h>
#include <linux/power_supply.h>

#ifdef CONFIG_SSUSB_PROJECT_PHY
#include <mu3phy/mtk-phy-asic.h>
#endif
#include <linux/gpio.h>
#ifdef CONFIG_MTK_SMART_BATTERY
#include "mt_battery_common.h"
#endif
#include "mu3d_hal_hw.h"
#include "xhci-mtk.h"


#define IDPIN_IN  0
#define IDPIN_OUT 1
#define USB2_PORT 2
#define USB3_PORT 3

#define OTG_IDDIG_DEBOUNCE 500
#define DOCK_IDDIG_DEBOUNCE 100

#define U3_UX_EXIT_LFPS_TIMING_PAR	0xa0
#define U3_REF_CK_PAR	0xb0
#define U3_RX_UX_EXIT_LFPS_REF_OFFSET	8
#define U3_RX_UX_EXIT_LFPS_REF	(3 << (U3_RX_UX_EXIT_LFPS_REF_OFFSET))
#define	U3_REF_CK_VAL	10

#define U3_TIMING_PULSE_CTRL	0xb4
#define MTK_CNT_1US_VALUE			63	/* 62.5MHz:63, 70MHz:70, 80MHz:80, 100MHz:100, 125MHz:125 */

#define USB20_TIMING_PARAMETER	0x40
#define MTK_TIME_VALUE_1US			63	/* 62.5MHz:63, 80MHz:80, 100MHz:100, 125MHz:125 */

#define LINK_PM_TIMER	0x8
#define MTK_PM_LC_TIMEOUT_VALUE	3

enum dock_state {
	DOCK_NOT_PRESENT = 0,
	DOCK_PRESENT = 1,
	DOCK_UNPOWERED = 2,
};

static struct xhci_hcd *mtk_xhci;
static struct switch_dev *g_otg_state;
static DEFINE_MUTEX(dock_detection_mutex);

#ifndef CONFIG_MTK_SMART_BATTERY
void bat_charger_boost_enable(bool arg)
{

}
#endif

/* avoid compile error if not support charger ic */
void __weak tbl_charger_otg_vbus(int mode)
{
	mu3d_dbg(K_ERR, "%s(): dummy func, maybe not what you need, check it!!\n", __func__);
}

static inline struct ssusb_mtk *otg_switch_to_ssusb(struct otg_switch_mtk *otg_sx)
{
	return container_of(otg_sx, struct ssusb_mtk, otg_switch);
}

static bool wait_for_value(void __iomem *base, int addr, int msk, int value, int ms_intvl,
			   int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if ((mu3d_readl(base, addr) & msk) == value)
			return true;
		mdelay(ms_intvl);
	}

	return false;
}

static void mtk_chk_usb_ip_ck_sts(struct ssusb_mtk *ssusb)
{
	int ret;
	int num_u3_port = ssusb->u3_ports;
	int num_u2_port = ssusb->u2_ports;
	void __iomem *sif_base = ssusb->sif_base;


	ret =
	    wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_SYS125_RST_B_STS,
			   SSUSB_SYS125_RST_B_STS, 1, 10);
	if (ret == false)
		mu3d_dbg(K_WARNIN, "sys125_ck is still active!!!\n");

	/* do not check when SSUSB_U2_PORT_PDN = 1, because U2 port stays in reset state */
	if (num_u2_port && !(mu3d_readl(sif_base, SSUSB_U2_CTRL(0)) & SSUSB_U2_PORT_PDN)) {
		ret =
		    wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS2, SSUSB_U2_MAC_SYS_RST_B_STS,
				   SSUSB_U2_MAC_SYS_RST_B_STS, 1, 10);
		if (ret == false)
			mu3d_dbg(K_WARNIN, "mac2_sys_ck is still active!!!\n");
	}

	/* do not check when SSUSB_U3_PORT_PDN = 1, because U3 port stays in reset state */
	if (num_u3_port && !(mu3d_readl(sif_base, SSUSB_U3_CTRL(0)) & SSUSB_U3_PORT_PDN)) {
		ret =
		    wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_U3_MAC_RST_B_STS,
				   SSUSB_U3_MAC_RST_B_STS, 1, 10);
		if (ret == false)
			mu3d_dbg(K_WARNIN, "mac3_mac_ck is still active!!!\n");
	}
}


/*
* if there is not iddig-pin but also use dual-mode, should set FORCE_IDDIG/RG_IDDIG,
* such as type A port,  to emulate iddig detection.
* in order to support both with-iddig-pin and without-iddig-pin cases,
* make use of it anyway.
* when OTG cable plug in, iddig is low level, otherwise is high level
*/
void ssusb_otg_iddig_en(struct ssusb_mtk *ssusb)
{
	mu3d_setmsk(ssusb->sif_base, U3D_U2PHYDTM1, FORCE_IDDIG);
	/*port0 is otg */
	mu3d_setmsk(ssusb->sif_base, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_OTG_SEL);
}

/* tell mac switch to host mode */
void ssusb_otg_plug_in(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->sif_base, U3D_U2PHYDTM1, RG_IDDIG);
}

/* tell mac switch to device mode */
void ssusb_otg_plug_out(struct ssusb_mtk *ssusb)
{
	mu3d_setmsk(ssusb->sif_base, U3D_U2PHYDTM1, RG_IDDIG);
}


static int check_port_param(struct ssusb_mtk *ssusb, int version, int index)
{
	int ret = -EINVAL;
	int num_u3_port = ssusb->u3_ports;
	int num_u2_port = ssusb->u2_ports;

	mu3d_dbg(K_INFO, "%s u2-ports:%d, u3-ports:%d\n", __func__, num_u2_port, num_u3_port);

	if (USB2_PORT == version) {
		if (num_u2_port && (index < num_u2_port))
			ret = 0;
	} else if (USB3_PORT == version) {
		if (num_u3_port && (index < num_u3_port))
			ret = 0;
	}
	if (ret)
		mu3d_dbg(K_WARNIN, "%s u2-ports:%d, u3-ports:%d (param: u%d-port-%d)\n",
			 __func__, num_u2_port, num_u3_port, version, index);

	return ret;
}

/**
* @version: only USB2_PORT(usb2's) & USB3_PORT(usb3's);
* @index: virtual port index of usb3/usb2's, valid values : 0, 1, max-ports - 1.
* set 1 to PORT_POWER of PORT_STATUS register of index-port (usb-version)
*/
#define	MTK_XHCI_PORT_RO	((1<<0) | (1<<3) | (0xf<<10) | (1<<30))
	/*
	 * These bits are RW; writing a 0 clears the bit, writing a 1 sets the bit:
	 * bits 5:8, 9, 14:15, 25:27
	 * link state, port power, port indicator state, "wake on" enable state
	 */
#define MTK_XHCI_PORT_RWS	((0xf<<5) | (1<<9) | (0x3<<14) | (0x7<<25))

static u32 mtk_xhci_port_state_to_neutral(u32 state)
{
	/* Save read-only status and port state */
	return (state & MTK_XHCI_PORT_RO) | (state & MTK_XHCI_PORT_RWS);
}

static int xhci_port_power_on(struct ssusb_mtk *ssusb, int version, int index)
{
	u32 temp;
	u32 __iomem *addr = NULL;
	struct xhci_hcd *xhci = mtk_xhci;

	mu3d_dbg(K_DEBUG, "%s in(xhci:%p)\n", __func__, xhci);

	if (check_port_param(ssusb, version, index))
		return -ENODEV;

	if (USB2_PORT == version)
		addr = xhci->usb2_ports[index];
	else
		addr = xhci->usb3_ports[index];

	temp = readl(addr);
	temp = mtk_xhci_port_state_to_neutral(temp);
	temp |= PORT_POWER;
	writel(temp, addr);
	while (!(readl(addr) & PORT_POWER))
		;
	mu3d_dbg(K_WARNIN, "%s (u%dport:%d), port status:0x%x\n", __func__,
		 version, index, readl(addr));
	return 0;
}

/**
* @version: only USB2_PORT(usb2's) & USB3_PORT(usb3's);
* @index: virtual port index of usb3/usb2's, valid values : 0, 1, max-ports - 1.
* set 0 to PORT_POWER of PORT_STATUS register of index-port (usb-version)
*/
static int xhci_port_power_off(struct ssusb_mtk *ssusb, int version, int index)
{
	u32 temp;
	u32 __iomem *addr = NULL;
	struct xhci_hcd *xhci = mtk_xhci;

	mu3d_dbg(K_DEBUG, "%s in(xhci:%p)\n", __func__, xhci);

	if (check_port_param(ssusb, version, index))
		return -ENODEV;

	if (USB2_PORT == version)
		addr = xhci->usb2_ports[index];
	else
		addr = xhci->usb3_ports[index];

	temp = readl(addr);
	temp = mtk_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	writel(temp, addr);
	while (readl(addr) & PORT_POWER)
		;
	mu3d_dbg(K_WARNIN, "%s (u%dport:%d), port status:0x%x\n", __func__,
		 version, index, readl(addr));
	return 0;
}


/* only operator ports will be used later */
static void host_ports_enable(struct ssusb_mtk *ssusb)
{
	int i;
	u32 temp;
	int num_u3_port = ssusb->u3_ports;
	int num_u2_port = ssusb->u2_ports;
	void __iomem *sif_base = ssusb->sif_base;

	mu3d_dbg(K_DEBUG, "%s in\n", __func__);

	mu3d_dbg(K_WARNIN, "%s u2p:%d, u3p:%d\n", __func__, num_u2_port, num_u3_port);

	mu3d_clrmsk(sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);

	/* disable ip host power down bit --> power on host ip */
	mu3d_clrmsk(sif_base, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* disable all u3 port power down and disable bits --> power on and enable all u3 ports */
	for (i = 0; i < num_u3_port; i++) {
		temp = mu3d_readl(sif_base, SSUSB_U3_CTRL(i));
		temp &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		temp |= SSUSB_U3_PORT_HOST_SEL;
		mu3d_writel(sif_base, SSUSB_U3_CTRL(i), temp);
	}

	/* disable all u2 port power down and disable bits --> power on and enable all u2 ports */
	for (i = 0; i < num_u2_port; i++) {
		temp = mu3d_readl(sif_base, SSUSB_U2_CTRL(i));
		temp &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		temp |= SSUSB_U2_PORT_HOST_SEL;
		mu3d_writel(sif_base, SSUSB_U2_CTRL(i), temp);
	}
	ssusb_otg_plug_in(ssusb);
	/* msleep(100); */
	mtk_chk_usb_ip_ck_sts(ssusb);
	mu3d_dbg(K_DEBUG, "%s out\n", __func__);
}


static void host_ports_disable(struct ssusb_mtk *ssusb)
{
	int i;
	u32 temp;
	int num_u3_port = ssusb->u3_ports;
	int num_u2_port = ssusb->u2_ports;
	void __iomem *sif_base = ssusb->sif_base;

	mu3d_dbg(K_DEBUG, "%s in\n", __func__);

	mu3d_dbg(K_WARNIN, "%s u2p:%d, u3p:%d\n", __func__, num_u2_port, num_u3_port);
	ssusb_otg_plug_out(ssusb);

	/* enable all u3 port power down and disable bits --> power down and disable all u3 ports */
	for (i = 0; i < num_u3_port; i++) {
		temp = mu3d_readl(sif_base, SSUSB_U3_CTRL(i));
		temp |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mu3d_writel(sif_base, SSUSB_U3_CTRL(i), temp);
	}

	/* enable all u2 port power down and disable bits --> power down and disable all u2 ports */
	for (i = 0; i < num_u2_port; i++) {
		temp = mu3d_readl(sif_base, SSUSB_U2_CTRL(i));
		temp |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
		mu3d_writel(sif_base, SSUSB_U2_CTRL(i), temp);
	}
	/* enable ip host power down bit --> power down host ip */
	mu3d_setmsk(sif_base, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	mu3d_dbg(K_DEBUG, "%s out\n", __func__);
}


static void mtk_host_wakelock_lock(struct wake_lock *wakelock)
{
	if (!wake_lock_active(wakelock))
		wake_lock(wakelock);
	mu3d_dbg(K_DEBUG, "done\n");
}

static void mtk_host_wakelock_unlock(struct wake_lock *wakelock)
{
	if (wake_lock_active(wakelock))
		wake_unlock(wakelock);
	mu3d_dbg(K_DEBUG, "done\n");
}


static int ssusb_set_vbus(struct vbus_ctrl_info *vbus, int is_on)
{

	if (SSUSB_VBUS_GPIO == vbus->vbus_mode) {
		mu3d_dbg(K_DEBUG, "%s() set vbus(gpio:%d).\n", __func__, vbus->vbus_gpio_num);
		gpio_set_value(vbus->vbus_gpio_num, !!is_on);

	} else if (SSUSB_VBUS_CHARGER == vbus->vbus_mode) {
		mu3d_dbg(K_DEBUG, "%s() call charger driver api.\n", __func__);
		bat_charger_boost_enable(is_on);

	} else if (SSUSB_VBUS_DEF_ON == vbus->vbus_mode) {
		/* nothing to do, turn on by default */
		mu3d_dbg(K_DEBUG, "%s() vbus turn on by default.\n", __func__);

	} else {
		/* Maybe LDO, TODO... */
		mu3d_dbg(K_ERR, "%s() error : no such case! check it...\n", __func__);
	}

	return 0;
}


static int ssusb_port_switch(struct ssusb_mtk *ssusb, bool tohost, int version, int index)
{
	u32 temp;
	void __iomem *sif_base = ssusb->sif_base;

	mu3d_dbg(K_WARNIN, "%s in(tohost: %d, u%dport-%d)\n", __func__, tohost, version, index);

	if (check_port_param(ssusb, version, index))
		return -ENODEV;

	if (tohost)
		ssusb_otg_plug_in(ssusb);
	else
		ssusb_otg_plug_out(ssusb);

	if (USB2_PORT == version)
		mu3d_setmsk(sif_base, SSUSB_U2_CTRL(index), SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
	else
		mu3d_setmsk(sif_base, SSUSB_U3_CTRL(index), SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);

	mtk_chk_usb_ip_ck_sts(ssusb);
	mu3d_dbg(K_DEBUG, "%s U3D_SSUSB_OTG_STS: %x\n", __func__,
		 mu3d_readl(sif_base, U3D_SSUSB_OTG_STS));

	/* disable u2 port power down and disable bits --> power on and enable all u2 ports */
	if (USB2_PORT == version) {
		temp = mu3d_readl(sif_base, SSUSB_U2_CTRL(index));
		temp = temp & (~SSUSB_U2_PORT_PDN);
		temp = temp & (~SSUSB_U2_PORT_DIS);
		temp = (tohost) ? temp | SSUSB_U2_PORT_HOST_SEL : temp & (~SSUSB_U2_PORT_HOST_SEL);
		mu3d_writel(sif_base, SSUSB_U2_CTRL(index), temp);
	} else {
		/* disable u3 port power down and disable bits --> power on and enable all u3 ports */

		temp = mu3d_readl(sif_base, SSUSB_U3_CTRL(index));
		temp = temp & (~SSUSB_U3_PORT_PDN);
		temp = temp & (~SSUSB_U3_PORT_DIS);
		temp = (tohost) ? temp | SSUSB_U3_PORT_HOST_SEL : temp & (~SSUSB_U3_PORT_HOST_SEL);
		mu3d_writel(sif_base, SSUSB_U3_CTRL(index), temp);
	}

	mtk_chk_usb_ip_ck_sts(ssusb);
	mu3d_dbg(K_DEBUG, "%s U3D_SSUSB_OTG_STS: %x\n", __func__,
		 mu3d_readl(sif_base, U3D_SSUSB_OTG_STS));

	mu3d_dbg(K_DEBUG, "%s out\n", __func__);
	return 0;
}



static void switch_port_to_host(struct ssusb_mtk *ssusb)
{
	mu3d_dbg(K_WARNIN, "%s\n", __func__);

	/* assert port power bit to drive drv_vbus */
	if (ssusb->otg_switch.port0_u2) {
		ssusb_port_switch(ssusb, true, USB2_PORT, 0);
		xhci_port_power_on(ssusb, USB2_PORT, 0);
	}
	if (ssusb->otg_switch.port0_u3) {
		ssusb_port_switch(ssusb, true, USB3_PORT, 0);
		xhci_port_power_on(ssusb, USB3_PORT, 0);
	}
}

static void switch_port_to_device(struct ssusb_mtk *ssusb, bool skip_u3dev)
{
	mu3d_dbg(K_WARNIN, "%s mtk_xhci:%p\n", __func__, mtk_xhci);

	/* deassert port power bit to drop off vbus */
	if (ssusb->otg_switch.port0_u2) {
		xhci_port_power_off(ssusb, USB2_PORT, 0);
		ssusb_port_switch(ssusb, false, USB2_PORT, 0);
	}

	if (ssusb->otg_switch.port0_u3) {
		xhci_port_power_off(ssusb, USB3_PORT, 0);
		ssusb_port_switch(ssusb, false, USB3_PORT, 0);
	}
}


static inline void set_iddig_out_detect(struct otg_switch_mtk *otg_switch)
{
	irq_set_irq_type(otg_switch->iddig_eint_num, IRQF_TRIGGER_HIGH);
}

static inline void set_iddig_in_detect(struct otg_switch_mtk *otg_switch)
{
	irq_set_irq_type(otg_switch->iddig_eint_num, IRQF_TRIGGER_LOW);
}

/*
* it is used by the platform which has more than one usb ports
* and port0 supports OTG, other ports works as host only.
* in the case, should ensure that port0's OTG switch can't affect
* other host ports.
*/
static void ssusb_mode_switch(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_switch =
	    container_of(dwork, struct otg_switch_mtk, switch_dwork);
	struct ssusb_mtk *ssusb = otg_switch_to_ssusb(otg_switch);

	bool cur_id_state = otg_switch->next_idpin_state;

	if (cur_id_state == IDPIN_IN) {
		mu3d_dbg(K_DEBUG, "%s to host\n", __func__);
		/* open port power and switch resource to host */
		switch_port_to_host(ssusb);
		switch_set_state(&otg_switch->otg_state, 1);
		ssusb_set_vbus(&otg_switch->p0_vbus, 1);
		mtk_host_wakelock_lock(&otg_switch->xhci_wakelock);

		/* expect next isr is for id-pin out action */
		otg_switch->next_idpin_state = IDPIN_OUT;
		/* make id pin to detect the plug-out */
		set_iddig_out_detect(otg_switch);

	} else {		/* IDPIN_OUT */
		mu3d_dbg(K_DEBUG, "%s to device\n", __func__);
		ssusb_set_vbus(&otg_switch->p0_vbus, 0);
		/* switch_port_to_device(false); */
		switch_port_to_device(ssusb, true);
		switch_set_state(&otg_switch->otg_state, 0);
		mtk_host_wakelock_unlock(&otg_switch->xhci_wakelock);

		/* expect next isr is for id-pin in action */
		/* mtk_id_nxt_state = IDPIN_IN; */
		otg_switch->next_idpin_state = IDPIN_IN;
		/* make id pin to detect the plug-in */
		set_iddig_in_detect(otg_switch);
	}
	enable_irq(otg_switch->iddig_eint_num);

	mu3d_dbg(K_WARNIN, "xhci switch resource to %s,  switch(%d)\n",
		 (cur_id_state == IDPIN_IN) ? "host" : "device",
		 switch_get_state(&otg_switch->otg_state));

}

/*
* the case is used by platform which only support one usb port,
* especially such as tablet/phone which demand to close all
* LDO, clocks and suspend phy when no cable plug in.
*/
void mtk_xhci_halt(struct ssusb_mtk *ssusb)
{

	struct usb_hcd	*hcd;
	struct xhci_hcd *xhci;
	hcd = platform_get_drvdata(ssusb->xhci);
	if (!hcd) {
		mu3d_dbg(K_ERR, "%s hcd is NULL\n", __func__);
		return;
	}
	xhci = hcd_to_xhci(hcd);
	if (xhci) {
		spin_lock_irq(&xhci->lock);
		xhci_halt(xhci);
		spin_unlock_irq(&xhci->lock);
	} else
		mu3d_dbg(K_ERR, "%s xhci is NULL\n", __func__);
}

static struct power_supply *ssusb_get_bat_psy(struct ssusb_mtk *ssusb)
{
	if (ssusb->batt_psy)
		return ssusb->batt_psy;

	ssusb->batt_psy = power_supply_get_by_name("battery");
	if (!ssusb->batt_psy) {
		mu3d_dbg(K_ERR, "%s: can't find battery psy\n", __func__);
		return NULL;
	}

	return ssusb->batt_psy;
}

static int ssusb_set_dock_present(struct ssusb_mtk *ssusb, bool en)
{
	union power_supply_propval val;
	int ret = 0;

	if (!ssusb_get_bat_psy(ssusb))
		return -1;

	val.intval = en ? 1 : 0;
	ret = ssusb->batt_psy->set_property(ssusb->batt_psy,
				POWER_SUPPLY_PROP_DOCK_PRESENT, &val);
	if (ret) {
		mu3d_dbg(K_ERR, "%s: set_property fail\n", __func__);
		return ret;
	}

	return ret;
}

#define VALID_VBUS_MV 4300
static int ssusb_dock_detection(void)
{
	int type = 0, vbus_mv = 0;

	/*
	 * This waiting is just for initialization phase.
	 * Because BC1.2 detection only can be invoked after usb moudle ready,
	 * otherwise result of detection is unstable.
	 */
	while (!bat_check_usb_ready()) {
		mu3d_dbg(K_WARNIN, "%s: wait usb ready\n", __func__);
		msleep(100);
	}

	type = bat_read_charger_type();
	mu3d_dbg(K_WARNIN, "%s: charger type: %d\n", __func__, type);
	if (type != STANDARD_CHARGER)
		return DOCK_NOT_PRESENT;

	vbus_mv = battery_meter_get_charger_voltage();
	mu3d_dbg(K_WARNIN, "%s: vbus_mv: %d\n", __func__, vbus_mv);
	if (vbus_mv < VALID_VBUS_MV)
		return DOCK_UNPOWERED;

	return DOCK_PRESENT;
}

void ssusb_rerun_dock_detection(void)
{
	struct otg_switch_mtk *otg_switch =
		container_of(g_otg_state, struct otg_switch_mtk, otg_state);
	struct ssusb_mtk *ssusb = otg_switch_to_ssusb(otg_switch);
	int vbus_mv = 0;
	bool is_present = false;

	if (!ssusb->pdata->is_amazon_dock_supported)
		return;

	mutex_lock(&dock_detection_mutex);
	vbus_mv = battery_meter_get_charger_voltage();
	is_present = bat_is_charger_exist();
	mu3d_dbg(K_WARNIN, "%s: state: %d vbus_mv: %d is_present: %d\n",
				__func__, otg_switch->dock_state,
				vbus_mv, is_present);
	switch (otg_switch->dock_state) {
	case DOCK_UNPOWERED:
		if (vbus_mv > VALID_VBUS_MV || is_present) {
			otg_switch->dock_state = DOCK_PRESENT;
			mu3d_dbg(K_WARNIN,
				"%s: Dock detected: Unpowered -> Powered\n",
				__func__);
			ssusb_set_dock_present(ssusb, true);
		}
		break;
	case DOCK_PRESENT:
		if (vbus_mv < VALID_VBUS_MV || !is_present) {
			otg_switch->dock_state = DOCK_UNPOWERED;
			mu3d_dbg(K_WARNIN,
				"%s: Dock removed: Powered -> Unpowered\n",
				__func__);
			ssusb_set_dock_present(ssusb, false);
		}
		break;
	default:
		break;
	}
	mutex_unlock(&dock_detection_mutex);
}

bool ssusb_dock_handler(struct ssusb_mtk *ssusb, int id_state)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;
	bool is_dock_mode = false;

	if (!ssusb->pdata->is_amazon_dock_supported)
		return false;

	mutex_lock(&dock_detection_mutex);
	if (id_state == IDPIN_IN) {
		otg_switch->dock_state = ssusb_dock_detection();
		switch (otg_switch->dock_state) {
		case DOCK_PRESENT:
			mu3d_dbg(K_WARNIN, "%s: Dock detected\n", __func__);
			ssusb_set_dock_present(ssusb, true);
			is_dock_mode = true;
			break;
		case DOCK_UNPOWERED:
			mu3d_dbg(K_WARNIN, "%s: Unpowered Dock detected\n",
					__func__);
			is_dock_mode = true;
			break;
		default:
			break;
		}

		if (is_dock_mode) {
			/* expect next isr is for id-pin out action */
			otg_switch->next_idpin_state = IDPIN_OUT;
			/* make id pin to detect the plug-out */
			set_iddig_out_detect(otg_switch);
		}
	} else {
		switch (otg_switch->dock_state) {
		case DOCK_PRESENT:
			mu3d_dbg(K_WARNIN, "%s: Dock removed\n", __func__);
			otg_switch->dock_state = DOCK_NOT_PRESENT;
			is_dock_mode = true;
			ssusb_set_dock_present(ssusb, false);
			break;
		case DOCK_UNPOWERED:
			is_dock_mode = true;
			otg_switch->dock_state = DOCK_NOT_PRESENT;
			mu3d_dbg(K_WARNIN, "%s: Unpowered Dock removed\n",
					__func__);
			break;
		default:
			break;
		}

		if (is_dock_mode) {
			/* expect next isr is for id-pin in action */
			otg_switch->next_idpin_state = IDPIN_IN;
			/* make id pin to detect the plug-in */
			set_iddig_in_detect(otg_switch);
		}
	}
	mutex_unlock(&dock_detection_mutex);

	return is_dock_mode;
}

static void ssusb_mode_switch_lowpw(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_switch =
	    container_of(dwork, struct otg_switch_mtk, switch_dwork);
	struct ssusb_mtk *ssusb = otg_switch_to_ssusb(otg_switch);

	bool cur_id_state = otg_switch->next_idpin_state;

	if (ssusb_dock_handler(ssusb, cur_id_state)) {
		enable_irq(otg_switch->iddig_eint_num);
		return;
	}

	if (cur_id_state == IDPIN_IN) {
		mu3d_dbg(K_DEBUG, "%s to host\n", __func__);
		mtk_host_wakelock_lock(&otg_switch->xhci_wakelock);
		ssusb_power_restore(ssusb);
		mtk_xhci_ip_init(ssusb);
		ssusb_host_init(ssusb);
		switch_set_state(&otg_switch->otg_state, 1);
		ssusb_set_vbus(&otg_switch->p0_vbus, 1);

		/* expect next isr is for id-pin out action */
		otg_switch->next_idpin_state = IDPIN_OUT;
		/* make id pin to detect the plug-out */
		set_iddig_out_detect(otg_switch);

	} else {		/* IDPIN_OUT */
		mu3d_dbg(K_DEBUG, "%s to device\n", __func__);
		ssusb_set_vbus(&otg_switch->p0_vbus, 0);
		mtk_xhci_halt(ssusb);
		mtk_xhci_ip_exit(ssusb);
		ssusb_host_exit(ssusb);
		ssusb_power_save(ssusb);
		switch_set_state(&otg_switch->otg_state, 0);
		mtk_host_wakelock_unlock(&otg_switch->xhci_wakelock);

		/* expect next isr is for id-pin in action */
		otg_switch->next_idpin_state = IDPIN_IN;
		/* make id pin to detect the plug-in */
		set_iddig_in_detect(otg_switch);
	}
	enable_irq(otg_switch->iddig_eint_num);

	mu3d_dbg(K_WARNIN, "xhci switch resource to %s,  switch(%d)\n",
		 (cur_id_state == IDPIN_IN) ? "host" : "device",
		 switch_get_state(&otg_switch->otg_state));

}

/*
* use eint of idpin instead of sysfs interface to switch device and host mode
*/
static void ssusb_mode_switch_idpin(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_switch =
	    container_of(dwork, struct otg_switch_mtk, switch_dwork);
	struct ssusb_mtk *ssusb = otg_switch_to_ssusb(otg_switch);
	struct musb *musb = ssusb->mu3di;
	bool cur_id_state = otg_switch->next_idpin_state;

	if (cur_id_state == IDPIN_IN) {
		musb_stop(musb);
		msleep(200);	/* if not, there is something wrong with xhci's port status */
		ssusb_mode_switch_manual(ssusb, 1);
		/* expect next isr is for id-pin out action */
		otg_switch->next_idpin_state = IDPIN_OUT;
		/* make id pin to detect the plug-out */
		set_iddig_out_detect(otg_switch);
	} else {
		ssusb_mode_switch_manual(ssusb, 0);
		msleep(100);
		musb_start(musb);
		/* expect next isr is for id-pin in action */
		otg_switch->next_idpin_state = IDPIN_IN;
		/* make id pin to detect the plug-in */
		set_iddig_in_detect(otg_switch);
	}
	enable_irq(otg_switch->iddig_eint_num);
}


static irqreturn_t iddig_eint_isr(int irq, void *otg_sx)
{
	struct otg_switch_mtk *otg_switch = (struct otg_switch_mtk *)otg_sx;
	struct ssusb_mtk *ssusb = otg_switch_to_ssusb(otg_switch);
	int interval_ms;

	mu3d_dbg(K_WARNIN, "%s : schedule to delayed work\n", __func__);
	disable_irq_nosync(otg_switch->iddig_eint_num);
	interval_ms = ssusb->pdata->is_amazon_dock_supported ?
				DOCK_IDDIG_DEBOUNCE : OTG_IDDIG_DEBOUNCE;
	schedule_delayed_work(&otg_switch->switch_dwork,
					interval_ms * HZ / 1000);
	return IRQ_HANDLED;
}

static int ssusb_iddig_eint_init(struct otg_switch_mtk *otg_sx)
{
	unsigned int eint_num = otg_sx->iddig_eint_num;
	int ret;

	ret = request_irq(eint_num, iddig_eint_isr, IRQF_ONESHOT, "usb_iddig", otg_sx);
	if (ret) {
		mu3d_dbg(K_ERR, "fail to register otg eint iddig isr\n");
		return -EINVAL;
	}
	otg_sx->is_iddig_registered = 1;
	mu3d_dbg(K_INFO, "otg iddig(irq-num:%d) register done.\n", eint_num);

	return 0;
}


static void ssusb_iddig_eint_exit(struct otg_switch_mtk *otg_sx)
{
	free_irq(otg_sx->iddig_eint_num, otg_sx);
	cancel_delayed_work(&otg_sx->switch_dwork);

	mu3d_dbg(K_INFO, "otg iddig unregister done.\n");
}

/*
* only for box platform, which port0 is type A, but also need to support dual-mode,
* and no iddig pin can be used;
* you can switch to host through:
* echo 1 > /sys/devices/bus.X/11270000.SSUSB/mode
* when 'echo 0' to switch to device mode;
*/
void ssusb_mode_switch_manual(struct ssusb_mtk *ssusb, int to_host)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;
	struct vbus_ctrl_info *vbus = &otg_switch->p0_vbus;

	if (to_host) {
		mu3d_dbg(K_DEBUG, "%s to host\n", __func__);
		/* open port power and switch resource to host */
		switch_port_to_host(ssusb);
		/*mtk_host_wakelock_lock(&otg_switch->xhci_wakelock); */
		switch_set_state(&otg_switch->otg_state, 1);
		ssusb_set_vbus(vbus, 1);
	} else {
		mu3d_dbg(K_DEBUG, "%s to device\n", __func__);
		ssusb_set_vbus(vbus, 0);
		switch_port_to_device(ssusb, true);
		switch_set_state(&otg_switch->otg_state, 0);
		/*mtk_host_wakelock_unlock(&otg_switch->xhci_wakelock); */
	}

	mu3d_dbg(K_WARNIN, "xhci switch resource to %s,  switch(%d)\n",
		 (to_host) ? "host" : "device", switch_get_state(&otg_switch->otg_state));

}

void ssusb_mode_switch_typec(int to_host)
{
	struct otg_switch_mtk *otg_switch = container_of(g_otg_state, struct otg_switch_mtk, otg_state);
	struct ssusb_mtk *ssusb = container_of(otg_switch, struct ssusb_mtk, otg_switch);

	if (to_host) {
		mu3d_dbg(K_DEBUG, "%s to host\n", __func__);
		ssusb_power_restore(ssusb);
		mtk_xhci_ip_init(ssusb);
		ssusb_host_init(ssusb);

		switch_set_state(&otg_switch->otg_state, 1);
		ssusb_set_vbus(&otg_switch->p0_vbus, 1);
		mtk_host_wakelock_lock(&otg_switch->xhci_wakelock);

	} else {		/* IDPIN_OUT */
		mu3d_dbg(K_DEBUG, "%s to device\n", __func__);
		ssusb_set_vbus(&otg_switch->p0_vbus, 0);
		mtk_xhci_ip_exit(ssusb);
		ssusb_host_exit(ssusb);
		ssusb_power_save(ssusb);
		switch_set_state(&otg_switch->otg_state, 0);
		mtk_host_wakelock_unlock(&otg_switch->xhci_wakelock);
	}

	mu3d_dbg(K_WARNIN, "xhci switch resource to %s,  switch(%d)\n",
		 to_host ? "host" : "device",
		 switch_get_state(&otg_switch->otg_state));

}

static void iddig_eint_register_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_switch =
	    container_of(dwork, struct otg_switch_mtk, iddig_reg_dwork);

	mu3d_dbg(K_INFO, "%s\n", __func__);
	ssusb_iddig_eint_init(otg_switch);
}

int mtk_otg_switch_init(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;

	otg_switch->otg_state.name = "otg_state";
	otg_switch->otg_state.index = 0;
	otg_switch->otg_state.state = 0;

	g_otg_state = &otg_switch->otg_state;
	if (switch_dev_register(&otg_switch->otg_state)) {
		mu3d_dbg(K_ERR, "switch_dev register fail\n");
		return -1;
	}
	/* should not call switch_set_state, only after switch_dev register; */
	if ((SSUSB_MODE_HOST == ssusb->drv_mode) || otg_switch->is_init_as_host)
		switch_set_state(&otg_switch->otg_state, 1);
	else
		switch_set_state(&otg_switch->otg_state, 0);

	wake_lock_init(&otg_switch->xhci_wakelock, WAKE_LOCK_SUSPEND, "xhci.wakelock");

	if (is_eint_used(otg_switch->otg_mode)) {
		otg_switch->next_idpin_state = IDPIN_IN;
		INIT_DELAYED_WORK(&otg_switch->iddig_reg_dwork, iddig_eint_register_work);
		if (SSUSB_OTG_IDPIN == otg_switch->otg_mode)
			INIT_DELAYED_WORK(&otg_switch->switch_dwork, ssusb_mode_switch_idpin);
		else if (ssusb->is_power_saving_mode)
			INIT_DELAYED_WORK(&otg_switch->switch_dwork, ssusb_mode_switch_lowpw);
		else
			INIT_DELAYED_WORK(&otg_switch->switch_dwork, ssusb_mode_switch);

		/* It is enough to delay 2sec for waiting for charger ic initialization */
		schedule_delayed_work(&otg_switch->iddig_reg_dwork, HZ * 2);
	}

	return 0;
}

void mtk_otg_switch_exit(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;

	if (is_eint_used(otg_switch->otg_mode)) {
		cancel_delayed_work(&otg_switch->iddig_reg_dwork);
		if (otg_switch->is_iddig_registered)
			ssusb_iddig_eint_exit(otg_switch);
	}

	switch_dev_unregister(&otg_switch->otg_state);
	mu3d_dbg(K_INFO, "mtk-otg-switch exit done.\n");
}

#if 0
#define WIFI_HW_RESET 104

static void wifi_hw_reset(void)
{
	int retval;

	retval = gpio_request(WIFI_HW_RESET, "wifi_hw_rst");
	if (retval) {
		mu3d_dbg(K_ERR, "fail to requeset wifi_hw_rst :%d\n", WIFI_HW_RESET);
		return;
	}
	gpio_direction_output(WIFI_HW_RESET, 1);

	gpio_set_value(WIFI_HW_RESET, 0);
	mu3d_dbg(K_ERR, "%s %d gpio:%d (val: %d)\n", __func__, __LINE__, WIFI_HW_RESET,
		 gpio_get_value(WIFI_HW_RESET));
	mdelay(10);
	gpio_set_value(WIFI_HW_RESET, 1);
	mu3d_dbg(K_ERR, "%s %d gpio:%d (val: %d)\n", __func__, __LINE__, WIFI_HW_RESET,
		 gpio_get_value(WIFI_HW_RESET));
}
#endif

void mtk_xhci_set(void *xhci)
{
	mu3d_dbg(K_WARNIN, "mtk_xhci = 0x%p\n", xhci);
	mtk_xhci = (struct xhci_hcd *)xhci;
	/*wifi_hw_reset();*/
}
EXPORT_SYMBOL_GPL(mtk_xhci_set);

void __iomem *get_xhci_base(void)
{
	if (mtk_xhci && mtk_xhci->main_hcd)
		return mtk_xhci->main_hcd->regs;

	return NULL;
}

bool mtk_is_host_mode(void)
{
	return switch_get_state(g_otg_state) ? true : false;
}

int is_init_host_for_manual_otg(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;

	return is_maual_otg(ssusb) && otg_switch->is_init_as_host;
}

/* only for box without charge or pmic to detect vbus */
int is_ssusb_connected_to_pc(struct ssusb_mtk *ssusb)
{
	u32 otg_status;
	int is_device;
	int vbus_valid = 0;

	is_device = !mtk_is_host_mode();
	if (is_device) {
		otg_status = mu3d_readl(ssusb->sif_base, U3D_SSUSB_OTG_STS);
		vbus_valid = otg_status & SSUSB_VBUS_VALID;
	}

	return is_device && vbus_valid;
}

static void mtk_xhci_ck_timer_init(struct ssusb_mtk *ssusb)
{
	int num_u3_port = ssusb->u3_ports;
	void __iomem *mac_base = ssusb->mac_base;

	mu3d_dbg(K_INFO, "num-u3-port:%d\n", num_u3_port);
	if (num_u3_port) {
		/* set MAC reference clock speed */
		mu3d_writelmsk(mac_base, U3D_UX_EXIT_LFPS_TIMING_PARAMETER,
			       RX_UX_EXIT_LFPS_REF, U3_RX_UX_EXIT_LFPS_REF);

		mu3d_writelmsk(mac_base, U3D_REF_CK_PARAMETER, REF_1000NS, U3_REF_CK_VAL);

		/* set SYS_CK */
		mu3d_writelmsk(mac_base, U3D_TIMING_PULSE_CTRL, CNT_1US_VALUE, MTK_CNT_1US_VALUE);
	}

	mu3d_writelmsk(mac_base, U3D_USB20_TIMING_PARAMETER, TIME_VALUE_1US, MTK_TIME_VALUE_1US);

	if (num_u3_port) {
		/* set LINK_PM_TIMER=3 */
		mu3d_writelmsk(mac_base, U3D_LINK_PM_TIMER, PM_LC_TIMEOUT_VALUE,
			       MTK_PM_LC_TIMEOUT_VALUE);
	}
}



void mtk_xhci_ip_init(struct ssusb_mtk *ssusb)
{
	struct vbus_ctrl_info *vbus;

	mu3d_dbg(K_DEBUG, "%s in\n", __func__);

	/* port1 only as host, so for port1 vbus, if not power on by default, enable it */
	if (ssusb->p1_exist) {
		vbus = &ssusb->p1_vbus;
		ssusb_set_vbus(vbus, 1);
	}

	/* port0 maybe support OTG, so for its vbus,  turn it on only for special cases */
	if (SSUSB_MODE_DEVICE != ssusb->drv_mode) {
		vbus = &ssusb->otg_switch.p0_vbus;
		if ((SSUSB_MODE_HOST == ssusb->drv_mode) || ssusb->otg_switch.is_init_as_host) {
			/* power on by default */
			ssusb_set_vbus(vbus, 1);
		}
	}

	/*
	 * power on host and power on/enable all ports
	 * if support OTG, gadget driver will switch port0 to device mode
	 */
	host_ports_enable(ssusb);

	mtk_xhci_ck_timer_init(ssusb);

	mu3d_dbg(K_DEBUG, "%s out\n", __func__);
}

void mtk_xhci_ip_exit(struct ssusb_mtk *ssusb)
{
	struct vbus_ctrl_info *vbus;

	mu3d_dbg(K_DEBUG, "%s in\n", __func__);

	if (ssusb->p1_exist) {
		vbus = &ssusb->p1_vbus;
		ssusb_set_vbus(vbus, 0);
	}

	if ((SSUSB_MODE_HOST == ssusb->drv_mode) || ssusb->otg_switch.is_init_as_host) {
		vbus = &ssusb->otg_switch.p0_vbus;
		ssusb_set_vbus(vbus, 0);
	}

	host_ports_disable(ssusb);

	mu3d_dbg(K_DEBUG, "%s out\n", __func__);
}

int ssusb_host_init(struct ssusb_mtk *ssusb)
{
	struct platform_device *xhci;
	struct ssusb_xhci_pdata pdata;
	int ret;

	xhci = platform_device_alloc("xhci-hcd", PLATFORM_DEVID_AUTO);
	if (!xhci) {
		dev_err(ssusb->dev, "couldn't allocate xHCI device\n");
		ret = -ENOMEM;
		goto err0;
	}

	xhci->dev.parent = ssusb->dev;
	xhci->dev.dma_mask = ssusb->dev->dma_mask;
	dma_set_coherent_mask(&xhci->dev, ssusb->dev->coherent_dma_mask);
	ssusb->xhci = xhci;

	ret = platform_device_add_resources(xhci, ssusb->xhci_rscs, SSUSB_XHCI_RSCS_NUM);
	if (ret) {
		dev_err(ssusb->dev, "couldn't add resources to xHCI device\n");
		goto err1;
	}
	memset(&pdata, 0, sizeof(pdata));
	pdata.need_str = !!(ssusb->str_mode == SSUSB_STR_DEEP);

	ret = platform_device_add_data(xhci, &pdata, sizeof(pdata));
	if (ret) {
		dev_err(ssusb->dev, "couldn't add platform data to xHCI device\n");
		goto err1;
	}

	ret = platform_device_add(xhci);
	if (ret) {
		dev_err(ssusb->dev, "failed to register xHCI device\n");
		goto err1;
	}
	dev_notice(ssusb->dev, "XHCI device register success...\n");

	return 0;

err1:
	platform_device_put(xhci);

err0:
	return ret;
}

void ssusb_host_exit(struct ssusb_mtk *ssusb)
{
	platform_device_unregister(ssusb->xhci);
}
