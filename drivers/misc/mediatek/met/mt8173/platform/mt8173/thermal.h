#ifndef _THERMAL_H_

#define _THERMAL_H_

struct th_zone {
	char path[256];
	char name[32];
	int tm;
};

typedef enum {
	MTK_THERMAL_SENSOR_CPU = 0,
	MTK_THERMAL_SENSOR_ABB,
	MTK_THERMAL_SENSOR_PMIC,
	MTK_THERMAL_SENSOR_BATTERY,
	MTK_THERMAL_SENSOR_MD1,
	MTK_THERMAL_SENSOR_MD2,
	MTK_THERMAL_SENSOR_WIFI,
	MTK_THERMAL_SENSOR_BATTERY2,
	MTK_THERMAL_SENSOR_BUCK,
	MTK_THERMAL_SENSOR_AP,
	MTK_THERMAL_SENSOR_PCB1,
	MTK_THERMAL_SENSOR_PCB2,
	MTK_THERMAL_SENSOR_SKIN,
	MTK_THERMAL_SENSOR_XTAL,

	MTK_THERMAL_SENSOR_COUNT
} MTK_THERMAL_SENSOR_ID;

extern struct metdevice met_thermal;

#ifndef NO_MTK_THERMAL_GET_TEMP
#define NO_MTK_THERMAL_GET_TEMP 0
#endif
#if NO_MTK_THERMAL_GET_TEMP == 0
extern int mtk_thermal_get_temp(MTK_THERMAL_SENSOR_ID id);
#endif

#ifdef MET_USE_THERMALDRIVER_TIMER
typedef void (*met_thermalsampler_func) (struct work_struct *);
extern void mt_thermalsampler_registerCB(met_thermalsampler_func pCB);
#endif	/* MET_USE_THERMALDRIVER_TIMER */

#endif				/* _THERMAL_H_ */
