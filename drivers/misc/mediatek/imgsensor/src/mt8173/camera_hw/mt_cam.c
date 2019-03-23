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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
/*#include <linux/xlog.h>*/


#include "mt_cam.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"


PowerUp PowerOnList = {
	{
	 {SENSOR_DRVNAME_OV2724_MIPI_RAW,
	  {
	   {PDN, Vol_Low, 0},
	   {RST, Vol_Low, 0},
	   {LDO, Vol_1800, 1},
	   {AVDD, Vol_2800, 0},
	   {SensorMCLK, Vol_High, 1},
	   {PDN, Vol_High, 11},
	   {RST, Vol_High, 0}
	   },
	  },

	 {SENSOR_DRVNAME_HI704_YUV,/*SENSOR_DRVNAME_HI704_RAW,*/
	  {
	   {PDN, Vol_Low, 2},
	   {AVDD, Vol_2800, 1},
	   {PDN, Vol_High, 1},
	   {DOVDD, Vol_1800, 2},
	   {SensorMCLK, Vol_High, 4},
	   {PDN, Vol_Low, 11},
	   },
	  },
	 /* add new sensor before this line */
	 {NULL,},
	 }
};

PowerUp PowerOnList_cam = {
	{
	 {SENSOR_DRVNAME_GC2355_MIPI_RAW,
	  {
	   {PDN, Vol_High, 5},
	   {RST, Vol_Low, 5},
	   {DOVDD, Vol_1800, 5},
	   {AVDD, Vol_2800, 5},
	   {SensorMCLK, Vol_High, 5},
	   {PDN, Vol_Low, 10},
	   {RST, Vol_High, 10}
	   },
	  },

	 {SENSOR_DRVNAME_GC0312_YUV,//SENSOR_DRVNAME_GC0312_RAW,
	  {
	   {PDN, Vol_Low, 5},
	   {DOVDD, Vol_1800, 5},
	   {AVDD, Vol_2800, 5},
	   {SensorMCLK, Vol_High, 5},
	   {PDN, Vol_High, 5},
	   {PDN, Vol_Low, 5},
	   },
	  },

	 {SENSOR_DRVNAME_GC0312_RAW,
	  {
	   {PDN, Vol_Low, 5},
	   {DOVDD, Vol_1800, 5},
	   {AVDD, Vol_2800, 5},
	   {SensorMCLK, Vol_High, 5},
	   {PDN, Vol_High, 5},
	   {PDN, Vol_Low, 5},
	   },
	  },
	 // add new sensor before this line
	 {NULL,},
	 }
};

PowerUp PowerOffList_cam = {
	{
	 {SENSOR_DRVNAME_GC2355_MIPI_RAW,
	  {
	   {PDN, Vol_High, 5},
	   {RST, Vol_Low, 5},
	   {SensorMCLK, Vol_Low, 5},
	   {AVDD, Vol_2800, 5},
	   {DOVDD, Vol_1800, 5},
	   {PDN, Vol_Low, 5},
	   },
	  },

	 {SENSOR_DRVNAME_GC0312_YUV,//SENSOR_DRVNAME_GC0312_RAW,
	  {
	   {PDN, Vol_High, 5},
	   {SensorMCLK, Vol_Low, 5},
	   {AVDD, Vol_2800, 5},
	   {DOVDD, Vol_1800, 5},
	   {PDN, Vol_Low, 5},
	   },
	  },

	 {SENSOR_DRVNAME_GC0312_RAW,
	  {
	   {PDN, Vol_High, 5},
	   {SensorMCLK, Vol_Low, 5},
	   {AVDD, Vol_2800, 5},
	   {DOVDD, Vol_1800, 5},
	   {PDN, Vol_Low, 5},
	   },
	  },
	 // add new sensor before this line
	 {NULL,},
	 }
};
/* Camera Custom Configs */

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[mt_cam]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(PFX "[%s]" fmt, __func__, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         pr_err(PFX "[%s]" fmt, __func__, ##arg)
#else
#define PK_DBG(a, ...)
#define PK_ERR(a, ...)
#endif


u32 pinSetIdx = 0;		/* default main sensor */

BOOL hwpoweron(PowerInformation pwInfo, char *mode_name)
{
	switch (pwInfo.PowerType) {
	/* Power pins */
	case AVDD:
	case DVDD:
	case DOVDD:
	case AFVDD:
	{
		PK_DBG("Power Pin#:%d (%dV)\n", pwInfo.PowerType, pwInfo.Voltage);
		if (TRUE != CAMERA_Regulator_poweron(pinSetIdx, pwInfo.PowerType, pwInfo.Voltage)) {
			PK_DBG("[CAMERA SENSOR] Fail to enable cam(%d) power(%d) to %d\n",
					pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
				return FALSE;
		}
	}
	break;
	/* GPIO pins*/
	case RST:
	case PDN:
	case LDO:
	{
		PK_DBG("Set GPIO pin(%d): %d\n", pwInfo.PowerType, pwInfo.Voltage);
		mtkcam_gpio_set(pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
	}
	break;

	/* MCLK */
	case SensorMCLK:
	{
		if (pinSetIdx == 0) {
			PK_DBG("Sensor MCLK1 On");
			ISP_MCLK1_EN(TRUE);
		} else if (pinSetIdx == 1)	{
			PK_DBG("Sensor MCLK2 On");
			ISP_MCLK2_EN(TRUE);
		}
	}
	break;
	default:
		pr_err("Error: invalid Power type (%d)\n", pwInfo.PowerType);
	break;
	};

	if (pwInfo.Delay > 0)
		mdelay(pwInfo.Delay);

	return TRUE;
}



BOOL hwpowerdown(PowerInformation pwInfo, char *mode_name)
{
	switch (pwInfo.PowerType) {
	/* Power pins */
	case AVDD:
	case DVDD:
	case DOVDD:
	case AFVDD:
	{
		PK_DBG("Power Pin#:%d (%dV)\n", pwInfo.PowerType, pwInfo.Voltage);
		if (TRUE != CAMERA_Regulator_powerdown(pinSetIdx, pwInfo.PowerType)) {
			PK_DBG("[CAMERA SENSOR] Fail to disable cam(%d) power(%d) to %d\n",
					pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
				return FALSE;
		}
	}
	break;
	/* GPIO pins*/
	case RST:
	case PDN:
	case LDO:
	{
		PK_DBG("Set GPIO pin(%d): %d\n", pwInfo.PowerType, pwInfo.Voltage);
		mtkcam_gpio_set(pinSetIdx, pwInfo.PowerType, Vol_Low);
	}
	break;

	/* MCLK */
	case SensorMCLK:
	{
		if (pinSetIdx == 0) {
			PK_DBG("Sensor MCLK1 OFF");
			ISP_MCLK1_EN(FALSE);
		} else if (pinSetIdx == 1)	{
			PK_DBG("Sensor MCLK2 OFF");
			ISP_MCLK2_EN(FALSE);
		}
	}
	break;
	default:
		pr_err("Error: invalid Power type (%d)\n", pwInfo.PowerType);
	break;
	};

	return TRUE;
}

BOOL hwpowerdown_cam(PowerInformation pwInfo, char *mode_name)
{
	switch (pwInfo.PowerType) {
	/* Power pins */
	case AVDD:
	case DVDD:
	case DOVDD:
	case AFVDD:
	{
		PK_DBG("Power Pin#:%d (%dV)\n", pwInfo.PowerType, pwInfo.Voltage);
		if (TRUE != CAMERA_Regulator_powerdown(pinSetIdx, pwInfo.PowerType)) {
			PK_DBG("[CAMERA SENSOR] Fail to disable cam(%d) power(%d) to %d\n",
					pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
				return FALSE;
		}
	}
	break;
	/* GPIO pins*/
	case RST:
	case PDN:
	case LDO:
	{
		PK_DBG("Set GPIO pin(%d): %d\n", pwInfo.PowerType, pwInfo.Voltage);
		mtkcam_gpio_set(pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
	}
	break;

	/* MCLK */
	case SensorMCLK:
	{
		if (pinSetIdx == 0) {
			PK_DBG("Sensor MCLK1 OFF");
			ISP_MCLK1_EN(FALSE);
		} else if (pinSetIdx == 1)	{
			PK_DBG("Sensor MCLK2 OFF");
			ISP_MCLK2_EN(FALSE);
		}
	}
	break;
	default:
		pr_err("Error: invalid Power type (%d)\n", pwInfo.PowerType);
	break;
	};

	if (pwInfo.Delay > 0)
		mdelay(pwInfo.Delay);

	return TRUE;
}

BOOL gc2355mipiraw_poweron(char *mode_name)
{
	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **RST, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **SensorMCLK, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[5], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **PDN, Vol_Low, 10**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[0].PowerInfo[6], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc2355 : **RST, Vol_High, 10**}\n");
		return FALSE;
	}

	return TRUE;
}

BOOL gc2355mipiraw_poweroff(char *mode_name)
{
	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **RST, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **SensorMCLK, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[0].PowerInfo[5], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc2355 : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	return TRUE;
}

BOOL gc0312yuv_poweron(char *mode_name)
{
	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **SensorMCLK, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[1].PowerInfo[5], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312 : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	return TRUE;
}

BOOL gc0312yuv_poweroff(char *mode_name)
{
	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[1].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312 : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[1].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312 : **SensorMCLK, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[1].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312 : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[1].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312 : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[1].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312 : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	return TRUE;
}

BOOL gc0312mipiraw_poweron(char *mode_name)
{
	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **SensorMCLK, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpoweron(PowerOnList_cam.PowerSeq[2].PowerInfo[5], mode_name) == FALSE) {
		PK_ERR("Power ON Fail : gc0312raw : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	return TRUE;
}

BOOL gc0312mipiraw_poweroff(char *mode_name)
{
	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[2].PowerInfo[0], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312raw : **PDN, Vol_High, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[2].PowerInfo[1], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312raw : **SensorMCLK, Vol_Low, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[2].PowerInfo[2], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312raw : **AVDD, Vol_2800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[2].PowerInfo[3], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312raw : **DOVDD, Vol_1800, 5**}\n");
		return FALSE;
	}

	if (hwpowerdown_cam(PowerOffList_cam.PowerSeq[2].PowerInfo[4], mode_name) == FALSE) {
		PK_ERR("Power OFF Fail : gc0312raw : **PDN, Vol_Low, 5**}\n");
		return FALSE;
	}

	return TRUE;
}

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx,
						char *currSensorName, BOOL On, char *mode_name)
{
	int pwListIdx, pwIdx;

	if ((DUAL_CAMERA_MAIN_SENSOR == SensorIdx) && currSensorName
	    && (0 == strcmp(PowerOnList.PowerSeq[0].SensorName, currSensorName))) {
		pinSetIdx = 0;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(PowerOnList.PowerSeq[1].SensorName, currSensorName))) {
		pinSetIdx = 1;
	} else if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW, currSensorName))) {
		pinSetIdx = 0;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(SENSOR_DRVNAME_GC0312_YUV, currSensorName))) {
		pinSetIdx = 1;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(SENSOR_DRVNAME_GC0312_RAW, currSensorName))) {
		pinSetIdx = 1;
	} else {
		PK_DBG("Not Match ! Bypass:  SensorIdx = %d (1:Main , 2:Sub), SensorName=%s\n",
		       SensorIdx, currSensorName);
		return -ENODEV;
	}

	/* power ON */
	if (On) {
		PK_DBG("kdCISModulePowerOn -on:currSensorName=%s\n", currSensorName);
		PK_DBG("kdCISModulePowerOn -on:pinSetIdx=%d\n", pinSetIdx);

		if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -on : SENSOR_DRVNAME_GC2355_MIPI_RAW\n");
			if(TRUE != gc2355mipiraw_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_YUV,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -on : SENSOR_DRVNAME_GC0312_YUV\n");
			if(TRUE != gc0312yuv_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_RAW,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -on : SENSOR_DRVNAME_GC0312_RAW\n");
			if(TRUE != gc0312mipiraw_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else
		{
		    for (pwListIdx = 0; pwListIdx < 16; pwListIdx++) {
		        if (currSensorName && (PowerOnList.PowerSeq[pwListIdx].SensorName != NULL)
		            && (0 == strcmp(PowerOnList.PowerSeq[pwListIdx].SensorName, currSensorName))) {
		            PK_DBG("kdCISModulePowerOn get in---\n");
		            PK_DBG("sensorIdx:%d\n", SensorIdx);

		            for (pwIdx = 0; pwIdx < 10; pwIdx++) {
		                if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx].PowerType != VDD_None) {
		                    if (hwpoweron(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx], mode_name) == FALSE) {
		                        PK_DBG("Power ON Fail\n");
		                        goto _kdCISModulePowerOn_exit_;
		                    }
		                } else {
		                    PK_DBG("pwIdx=%d\n", pwIdx);
		                    break;
		                }
		            }
		            break;
		        } else if (PowerOnList.PowerSeq[pwListIdx].SensorName == NULL) {
		            break;
		        } else {
		        }
		    }
		}

	} else {		/* power OFF */
		if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -off : SENSOR_DRVNAME_GC2355_MIPI_RAW\n");
			if(TRUE != gc2355mipiraw_poweroff(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_YUV,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -off : SENSOR_DRVNAME_GC0312_YUV\n");
			if(TRUE != gc0312yuv_poweroff(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_RAW,currSensorName)))
		{
			PK_DBG("kdCISModulePowerOn -off : SENSOR_DRVNAME_GC0312_RAW\n");
			if(TRUE != gc0312mipiraw_poweroff(mode_name))
			goto _kdCISModulePowerOn_exit_;
		}
		else
		{
		    for (pwListIdx = 0; pwListIdx < 16; pwListIdx++) {
		        if (currSensorName && (PowerOnList.PowerSeq[pwListIdx].SensorName != NULL)
		            && (0 == strcmp(PowerOnList.PowerSeq[pwListIdx].SensorName, currSensorName))) {
		            PK_DBG("kdCISModulePowerOn get in---\n");
		            PK_DBG("sensorIdx:%d\n", SensorIdx);

		            for (pwIdx = 9; pwIdx >= 0; pwIdx--) {
		                if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx].PowerType != VDD_None) {
		                    if (hwpowerdown(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx], mode_name) == FALSE)
		                        goto _kdCISModulePowerOn_exit_;
		                    if (pwIdx > 0) {
		                        if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx - 1].Delay > 0)
		                            mdelay(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx - 1].Delay);
		                    }
		                } else {
		                    PK_DBG("pwIdx=%d\n", pwIdx);
		                }
		            }
		        } else if (PowerOnList.PowerSeq[pwListIdx].SensorName == NULL) {
		            break;
		        } else {
		        }
		    }
		}

	}

	return 0;

 _kdCISModulePowerOn_exit_:
	return -EIO;
}
EXPORT_SYMBOL(kdCISModulePowerOn);


/* !-- */
/*  */

