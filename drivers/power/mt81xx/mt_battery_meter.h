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

#ifndef _BATTERY_METER_H
#define _BATTERY_METER_H

#ifdef CONFIG_IDME
#define BATTERY_ID_ATL 0x841
#define BATTERY_ID_DSY 0x831
#endif

#if defined(CONFIG_AMAZON_METRICS_LOG)
extern signed int gFG_BATT_CAPACITY_aging;
extern signed int gFG_BATT_CAPACITY;
#endif

extern s32 battery_meter_get_battery_voltage(void);
extern s32 battery_meter_get_charging_current(void);
extern s32 battery_meter_get_battery_current(void);
extern bool battery_meter_get_battery_current_sign(void);
extern s32 battery_meter_get_car(void);
extern s32 battery_meter_get_battery_temperature(void);
extern s32 battery_meter_get_charger_voltage(void);
extern s32 battery_meter_get_battery_percentage(void);
extern s32 battery_meter_initial(void);
extern s32 battery_meter_reset(bool bUI_SOC);
extern s32 battery_meter_sync(s32 bat_i_sense_offset);

extern s32 battery_meter_get_battery_zcv(void);
extern s32 battery_meter_get_battery_nPercent_zcv(void);
extern s32 battery_meter_get_battery_nPercent_UI_SOC(void);

extern s32 battery_meter_get_tempR(s32 dwVolt);
extern s32 battery_meter_get_batteryR(void);
extern s32 battery_meter_get_tempV(void);
extern s32 battery_meter_get_VSense(void);

extern int get_rtc_spare_fg_value(void);
extern int get_rtc_residual_fg_value(void);
extern int set_rtc_spare_fg_value(int);
extern int set_rtc_residual_fg_value(int);

extern s32 battery_meter_get_battery_voltage_cached(void);
extern s32 battery_meter_get_average_battery_voltage(void);
extern s32 battery_meter_get_battery_soc(void);
extern bool battery_meter_ocv2cv_trans_support(void);
extern s32 battery_meter_reset_aging(void);


#endif				/* #ifndef _BATTERY_METER_H */
