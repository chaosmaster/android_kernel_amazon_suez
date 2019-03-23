/*
 * Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/************************************************************************
* File Name: focaltech_ex_fun.c
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: function for fw upgrade, adb command, create apk second entrance
*
************************************************************************/
#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "tpd_custom_fts.h"
#include "focaltech_ex_fun.h"
#include "test_lib.h"

int fts_5x22_ctpm_fw_upgrade(struct i2c_client *client,
			     u8 *pbt_buf, u32 dw_length);

struct i2c_client *G_Client;
static struct mutex g_device_mutex;
/* 0 for no apk upgrade, 1 for apk upgrade */
int apk_debug_flag = 0;
int gesture_wakeup_enabled = 0;
int g_test = 1;

struct mutex fts_gesture_wakeup_mutex;

static unsigned char CTPM_FW_LENS_2T2R[] = {
#include "R9662_5726_0x6D_V0x8E_20170302_app.h"
};

static unsigned char CTPM_FW_TopGroup_2T2R[] = {
#include "R9662_5726_0x3E_V0x8E_20170302_app.h"
};

static unsigned char CTPM_FW_LENS_1T2R[] = {
#include "R9662_5726_0x6D_1T2R_V0x91_20180118_app.h"
};

static unsigned char CTPM_FW_TopGroup_1T2R[] = {
#include "R9662_5726_0x3E_1T2R_V0x91_20180118_app.h"
};

/************************************************************************
* Name: fts_ctpm_auto_clb
* Brief:  auto calibration
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_clb(struct i2c_client *client)
{
	unsigned char uc_temp = 0x00;
	unsigned char i = 0;
	int i_ret = 0;

	/*start auto CLB */
	msleep(200);

	i_ret = fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
	if (i_ret  < 0)
		pr_err("[focal] failed to write FACTORYMODE_VALUE\n");

	/*make sure already enter factory mode */
	msleep(100);
	/*write command to start calibration */
	i_ret = fts_write_reg(client, 2, 0x4);
	if (i_ret  < 0)
		pr_err("[focal] failed to write 0x4\n");

	msleep(300);

	for (i = 0; i < 100; i++) {
		i_ret = fts_read_reg(client, 0, &uc_temp);
		if (i_ret  < 0)
			pr_err("[focal] failed to read mode info\n");

		/*return to normal mode, calibration finish */
		if (0x0 == ((uc_temp & 0x70) >> 4))
			break;

		msleep(20);
	}
	/*calibration OK */
	/*goto factory mode for store */
	i_ret = fts_write_reg(client, 0, 0x40);
	if (i_ret  < 0)
		pr_err("[focal] failed to goto factory mode for store\n");

	/*make sure already enter factory mode */
	msleep(200);
	/*store CLB result */
	i_ret = fts_write_reg(client, 2, 0x5);
	if (i_ret  < 0)
		pr_err("[focal] failed to store CLB result\n");

	msleep(300);
	/*return to normal mode */
	i_ret = fts_write_reg(client, 0, FTS_WORKMODE_VALUE);
	if (i_ret  < 0)
		pr_err("[focal] failed to return to normal mode\n");

	msleep(300);

	/*store CLB result OK */
	return 0;
}

/***********************************************************************
* Name: fts_ctpm_fw_upgrade_with_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int i;
	int fw_len = 0;

	if (ft_vendor_id == FT_LENS_ID || ft_vendor_id == FT_LENS_ID_PROTO) {
		if (ft_routing_type == FT_1T2R_ID) {
			fw_len = ARRAY_SIZE(CTPM_FW_LENS_1T2R);
			pbt_buf = CTPM_FW_LENS_1T2R;
		} else if (ft_routing_type == FT_2T2R_ID) {
			fw_len = ARRAY_SIZE(CTPM_FW_LENS_2T2R);
			pbt_buf = CTPM_FW_LENS_2T2R;
		} else {
			pr_err("[focal] %s unknown ft_routing_type:0x%x\n",
				__func__, ft_routing_type);
			goto err;
		}
	} else if (ft_vendor_id == FT_TopGroup_ID) {
		if (ft_routing_type == FT_1T2R_ID) {
			fw_len = ARRAY_SIZE(CTPM_FW_TopGroup_1T2R);
			pbt_buf = CTPM_FW_TopGroup_1T2R;
		} else if (ft_routing_type == FT_2T2R_ID) {
			fw_len = ARRAY_SIZE(CTPM_FW_TopGroup_2T2R);
			pbt_buf = CTPM_FW_TopGroup_2T2R;
		} else {
			pr_err("[focal] %s unknown ft_routing_type:0x%x\n",
				__func__, ft_routing_type);
			goto err;
		}
	} else {
		pr_err("[focal] %s unknown ft_vendor_id:0x%x\n",
			__func__, ft_vendor_id);
		goto err;
	}

	if (fw_len < 8 || fw_len > 54 * 1024) {
		dev_err(&client->dev, "[focal] %s:FW length error\n", __func__);
		return -EIO;
	}

	/*call the upgrade function */
	for (i = 0; i < UPGRADE_RETRY_LOOP; i++) {
		i_ret = fts_5x22_ctpm_fw_upgrade(client, pbt_buf, fw_len);
		if (i_ret != 0) {
			dev_err(&client->dev,
				"[focal] FW upgrade failed, err=%d.\n", i_ret);
		} else {
#ifdef AUTO_CLB
			i_ret = fts_ctpm_auto_clb(client);
			if (i_ret != 0)
				dev_err(&client->dev,
					"[focal] AUTO_CLB failed, err=%d.\n",
						i_ret);
#endif
			break;
		}
	}

	return i_ret;

err:
	return -EINVAL;
}

/************************************************************************
* Name: fts_ctpm_get_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: .i file version
***********************************************************************/
u8 fts_ctpm_get_i_file_ver(void)
{
	u8 *pbt_buf = NULL;
	u16 ui_sz;

	if (ft_vendor_id == FT_LENS_ID || ft_vendor_id == FT_LENS_ID_PROTO) {
		if (ft_routing_type == FT_1T2R_ID) {
			ui_sz = ARRAY_SIZE(CTPM_FW_LENS_1T2R);
			pbt_buf = CTPM_FW_LENS_1T2R;
		} else if (ft_routing_type == FT_2T2R_ID) {
			ui_sz = ARRAY_SIZE(CTPM_FW_LENS_2T2R);
			pbt_buf = CTPM_FW_LENS_2T2R;
		} else {
			pr_err("[focal] %s unknown ft_routing_type:0x%x\n",
				__func__, ft_routing_type);
			goto err;
		}
	} else if (ft_vendor_id == FT_TopGroup_ID) {
		if (ft_routing_type == FT_1T2R_ID) {
			ui_sz = ARRAY_SIZE(CTPM_FW_TopGroup_1T2R);
			pbt_buf = CTPM_FW_TopGroup_1T2R;
		} else if (ft_routing_type == FT_2T2R_ID) {
			ui_sz = ARRAY_SIZE(CTPM_FW_TopGroup_2T2R);
			pbt_buf = CTPM_FW_TopGroup_2T2R;
		} else {
			pr_err("[focal] %s unknown ft_routing_type:0x%x\n",
				__func__, ft_routing_type);
			goto err;
		}
	} else {
		pr_err("[focal] %s unknown ft_vendor_id:0x%x\n",
			__func__, ft_vendor_id);
		goto err;
	}

	if (ui_sz > 2)
		return pbt_buf[0x10a];

	return 0x00;		/*default value */

err:
	return -EINVAL;
}

/************************************************************************
* Name: fts_ctpm_auto_upgrade
* Brief:  auto upgrade
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FTS_REG_FW_VER;
	u8 uc_tp_fm_ver;
	int i_ret;

	fts_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver = fts_ctpm_get_i_file_ver();
	if (uc_host_fm_ver < 0)
		return -EIO;

	pr_info("[focal] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
		uc_tp_fm_ver, uc_host_fm_ver);

	if ((uc_tp_fm_ver < uc_host_fm_ver) || (uc_tp_fm_ver >= 0xe0) ||
	    (uc_tp_fm_ver == 0x00) || (uc_tp_fm_ver == FTS_REG_FW_VER)) {
		msleep(100);

		i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
		if (i_ret == 0) {
			msleep(300);

			uc_host_fm_ver = fts_ctpm_get_i_file_ver();
			if (uc_host_fm_ver < 0)
				return -EIO;

			pr_info("[focal] Auto update to new version 0x%x\n",
					uc_host_fm_ver);
		} else {
			pr_err("[focal] Auto update failed ret=%d.\n", i_ret);
			return -EIO;
		}
	}
	return 0;
}

/************************************************************************
* Name: hid_to_i2c
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int hid_to_i2c(struct i2c_client *client)
{
#if HIDTOI2C_DISABLE
	return true;
#else
	u8 auc_i2c_write_buf[5] = { 0 };
	int bRet = 0, ret;

	auc_i2c_write_buf[0] = 0xeb;
	auc_i2c_write_buf[1] = 0xaa;
	auc_i2c_write_buf[2] = 0x09;

	ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 3);
	if (ret < 0) {
		dev_err(&client->dev, "[focal] %s: write iic error\n",
			__func__);
		return ret;
	}
	msleep(10);

	auc_i2c_write_buf[0] = 0;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = 0;
	ret =
	    ftxxxx_i2c_Read(client, auc_i2c_write_buf, 0, auc_i2c_write_buf, 3);
	if (ret < 0) {
		dev_err(&client->dev, "[focal] %s:  read iic error\n",
			__func__);
		return ret;
	}

	if ((0xeb == auc_i2c_write_buf[0]) &&
	    (0xaa == auc_i2c_write_buf[1]) && (0x08 == auc_i2c_write_buf[2])) {
		pr_debug("[focal] HidI2c_To_StdI2c successful\n");
		bRet = 1;
	} else {
		pr_err("[focal] HidI2c_To_StdI2c error.\n");
		bRet = 0;
	}

	return bRet;
#endif
}

/************************************************************************
* Name: fts_5x22_ctpm_fw_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_5x22_ctpm_fw_upgrade(struct i2c_client *client,
			     u8 *pbt_buf, u32 dw_length)
{
	u8 reg_val[4] = { 0 };
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	u8 bt_ecc_check;
	int i_ret;

	pr_info("[focal] Entering fts_5x22_ctpm_fw_upgrade ....\n");

	i_ret = hid_to_i2c(client);
	if (i_ret == 0)
		pr_err("[focal] hid change to i2c fail\n");

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*********Step 1:Reset CTPM *****/
		pr_info("[focal] Step 1: reset CTPM\n");
		/*write 0xaa to register 0xfc */
		i_ret = fts_write_reg(client, 0xfc, FTS_UPGRADE_AA);
		if (i_ret  < 0)
			pr_err("[focal] failed to write 0xaa to register 0xfc\n");

		msleep(IC_INFO_DELAY_AA);

		i_ret = fts_write_reg(client, 0xfc, FTS_UPGRADE_55);
		if (i_ret < 0)
			pr_err("[focal] failed to write 0x55 to register 0xfc\n");

		msleep(200);

		/*********Step 2:Enter upgrade mode *****/
		pr_info("[focal] Step 2: Enter upgrade mode\n");
		i_ret = hid_to_i2c(client);
		if (i_ret == 0) {
			pr_err("[focal] hid change to i2c fail\n");
			continue;
		}

		msleep(10);

		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			pr_err("[focal] failed writing 0x55 and 0xaa\n");
			continue;
		}

		/*********Step 3:check READ-ID***********************/
		pr_info("[focal] Step 3: Check read-ID\n");
		msleep(20);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = 0x00;
		auc_i2c_write_buf[2] = 0x00;
		auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;

		i_ret =	ftxxxx_i2c_Read(client, auc_i2c_write_buf,
			4, reg_val, 2);
		if (i_ret < 0)
			pr_err("[focal] fail on checking read-ID\n");

		if (reg_val[0] == IC_INFO_UPGRADE_ID1 &&
		    reg_val[1] == IC_INFO_UPGRADE_ID2) {
			/*read from bootloader FW */
			pr_info("[focal] Check OK, CTPM ID1=0x%x, ID2=0x%x\n",
				reg_val[0], reg_val[1]);
			break;
		}

		dev_info(&client->dev,
			"[focal] Step3: CTPM ID,ID1 = 0x%x, ID2 = 0x%x\n",
			reg_val[0], reg_val[1]);

		continue;
	}

	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	/*Step 4:erase app and panel paramenter area */
	pr_info("[focal] Step 4:erase app and panel paramenter area\n");
	auc_i2c_write_buf[0] = 0x61;
	/* erase app area */
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);
	if (i_ret < 0)
		pr_debug("[focal] failed to erase app area\n");

	msleep(1350);

	for (i = 0; i < 15; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		i_ret =
		    ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
		if (i_ret < 0)
			pr_err("[focal] failed to read iic\n");

		if (0xF0 == reg_val[0] && 0xAA == reg_val[1]) {
			pr_info("[focal] i=%d,reg_val[0]=0x%x,reg_val[1]=0x%x\n",
				i, reg_val[0], reg_val[1]);
			break;
		}
		msleep(50);
	}

	/*write bin file length to FW bootloader. */
	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = (u8) ((dw_length >> 16) & 0xFF);
	auc_i2c_write_buf[2] = (u8) ((dw_length >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_length & 0xFF);
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 4);
	if (i_ret < 0)
		pr_err("[focal] failed to write bin file length\n");

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	bt_ecc_check = 0;
	pr_info("[focal] Step 5:write firmware(FW) to ctpm flash\n");
	/*dw_length = dw_length - 8; */
	temp = 0;
	packet_number = (dw_length) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		length = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (length >> 8);
		packet_buf[5] = (u8) length;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc_check ^= pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}

		/* pr_info("[focal][%s] bt_ecc = %x\n", __func__, bt_ecc); */
		if (bt_ecc != bt_ecc_check)
			pr_info("[focal] Host csum error bt_ecc_check = %x\n",
				bt_ecc_check);

		i_ret =
		    ftxxxx_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
		if (i_ret < 0)
			pr_err("[focal] failed to write package\n");

		msleep(1);

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			i_ret = ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1,
						reg_val, 2);
			if (i_ret < 0)
				pr_err("[focal] failed to read from 0x6a\n");

			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
				break;

		pr_info
		("[focal]Retry. #packet=%d,i=%d,reg_val[0]=%x,reg_val[1]=%x\n",
			j, i, reg_val[0], reg_val[1]);

			msleep(20);
		}
	}

	if ((dw_length) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_length) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] =
			    pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc_check ^=
			    pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		i_ret = ftxxxx_i2c_Write(client, packet_buf, temp + 6);
		if (i_ret < 0)
			pr_err("[focal] failed to write i2c\n");

		/* pr_debug("[focal][%s] bt_ecc = %x\n", __func__, bt_ecc); */
		if (bt_ecc != bt_ecc_check)
			pr_err
			    ("[focal]Host checksum error bt_ecc_check = %x\n",
			     bt_ecc_check);

		msleep(1);

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			i_ret = ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1,
						reg_val, 2);
			if (i_ret < 0)
				pr_err("[focal] failed to read from 0x6a\n");

			if ((0x1000 + ((packet_number * FTS_PACKET_LENGTH) /
				((dw_length) % FTS_PACKET_LENGTH))) ==
				(((reg_val[0]) << 8) | reg_val[1]))
					break;

		pr_err
		("[focal] Retry:reg_val[0]=%x,reg_val[1]=%x,reg_val[2]=%x\n",
		reg_val[0], reg_val[1],
		(0x1000+((packet_number*FTS_PACKET_LENGTH)/
		((dw_length) % FTS_PACKET_LENGTH))));

			msleep(2);
		}
	}

	msleep(50);
	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	pr_debug("[focal] Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0x64;
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);
	if (i_ret < 0)
		pr_err("[focal] failed to read from 0x64\n");

	msleep(300);

	temp = 0;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8) (temp >> 16);
	auc_i2c_write_buf[2] = (u8) (temp >> 8);
	auc_i2c_write_buf[3] = (u8) (temp);
	temp = dw_length;
	auc_i2c_write_buf[4] = (u8) (temp >> 8);
	auc_i2c_write_buf[5] = (u8) (temp);
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 6);
	if (i_ret < 0)
		pr_err("[focal] failed to read from 0x65\n");

	msleep(dw_length / 256);

	for (i = 0; i < 100; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		i_ret =
		    ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
		if (i_ret < 0)
			pr_err("[focal] failed to read from 0x6a\n");

		if ((0xF0 == reg_val[0]) && (0x55 == reg_val[1])) {
			pr_info("[focal] --reg_val[0]=%02x reg_val[0]=%02x\n",
				reg_val[0], reg_val[1]);
			break;
		}

		msleep(10);
	}

	auc_i2c_write_buf[0] = 0x66;
	i_ret = ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (i_ret < 0)
		pr_err("[focal] failed to read from 0x66\n");

	if (reg_val[0] != bt_ecc) {
		dev_err(&client->dev,
			"[focal] ecc error! FW=%02x bt_ecc=%02x\n",
			reg_val[0], bt_ecc);
		return -EIO;
	}

	pr_info("[focal] checksum %X %X\n", reg_val[0], bt_ecc);
	/*********Step 7: reset the new FW***********************/
	pr_info("[focal] Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);
	if (i_ret < 0)
		pr_err("[focal] failed to reset the new FW\n");

	msleep(200);		/* make sure CTP startup normally */

	i_ret = hid_to_i2c(client);
	if (i_ret == 0)
		pr_err("[focal] HidI2c change to StdI2c fail !\n");

	return 0;
}

/*
*note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
/************************************************************************
* Name: fts_GetFirmwareSize
* Brief:  get file size
* Input: file name
* Output: no
* Return: file size
***********************************************************************/
static int fts_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];

	memset(filepath, 0, ARRAY_SIZE(filepath));

	snprintf(filepath, ARRAY_SIZE(filepath), "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("[focal] error occurred while opening file %s.\n",
		       filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

/************************************************************************
* Name: fts_ReadFirmware
* Brief:  read firmware buf for .bin file.
* Input: file name, data buf
* Output: data buf
* Return: 0
***********************************************************************/
/*
note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_ReadFirmware(char *firmware_name, unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, ARRAY_SIZE(filepath));
	snprintf(filepath, ARRAY_SIZE(filepath), "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("[focal] error occurred while opening file %s.\n",
		       filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);
	return 0;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_file
* Brief:  upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
				      char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int fwsize = fts_GetFirmwareSize(firmware_name);

	if (fwsize <= 0) {
		dev_err(&client->dev,
			"[focal] %s ERROR:Get firmware size failed\n",
			__func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 54 * 1024) {
		dev_err(&client->dev, "[focal] %s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev,
			"[focal] %s() - ERROR: request_firmware failed\n",
			__func__);
		kfree(pbt_buf);
		return -EIO;
	}

	/*call the upgrade function */
	i_ret = fts_5x22_ctpm_fw_upgrade(client, pbt_buf, fwsize);
	if (i_ret != 0) {
		dev_err(&client->dev,
			"[focal] %s() - ERROR: upgrade failed..\n",
			__func__);
	}

	kfree(pbt_buf);

	return i_ret;
}

/************************************************************************
* Name: fts_tpfwver_show
* Brief:  show tp fw vwersion
* Input: device, device attribute, char buf
* Output: no
* Return: char number
***********************************************************************/
static ssize_t fts_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	if (fts_read_reg(client, FTS_REG_FW_VER, &fwver) < 0) {
		num_read_chars = snprintf(buf, PAGE_SIZE,
					  "get TP FW version fail!\n");
	} else
		num_read_chars = scnprintf(buf, 100, "0x%X\n", fwver);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

/************************************************************************
* Name: fts_tprwreg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_tprwreg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	unsigned long int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = { 0 };

	memset(valbuf, 0, ARRAY_SIZE(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			pr_debug("[focal] please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		dev_err(&client->dev,
			"[focal] %s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n",
			__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/* read register */
		regaddr = wmreg;
		if (fts_read_reg(client, regaddr, &regvalue) < 0) {
			dev_err(&client->dev,
				"[focal] Could not read the register(0x%02x)\n",
				regaddr);
		} else {
			dev_info(&client->dev,
				"[focal] the register(0x%02x) is 0x%02x\n",
				regaddr, regvalue);
		}
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (fts_write_reg(client, regaddr, regvalue) < 0) {
			dev_err(&client->dev,
				"[focal] Could not write the register(0x%02x)\n",
				regaddr);
		} else {
			dev_info(&client->dev,
				"[focal] Write 0x%02x into register(0x%02x) successful\n",
				regvalue, regaddr);
		}
	}

error_return:
	mutex_unlock(&g_device_mutex);
	return count;
}

/************************************************************************
* Name: fts_fwupdate_store
* Brief:  upgrade from *.i
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupdate_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	disable_irq(client->irq);
	apk_debug_flag = 1;

	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
	if (i_ret == 0) {
		msleep(300);

		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		if (uc_host_fm_ver < 0)
			return -EIO;

		pr_info("[focal] %s:update to new version 0x%x\n",
			__func__, uc_host_fm_ver);
	} else {
		dev_err(&client->dev,
			"[focal] %s ERROR:update failed.\n", __func__);
	}

	apk_debug_flag = 0;
	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

/************************************************************************
* Name: fts_fwupgradeapp_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupgradeapp_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(fwname, 0, ARRAY_SIZE(fwname));
	snprintf(fwname, ARRAY_SIZE(fwname), "%s", buf);
	fwname[count - 1] = '\0';

	mutex_lock(&g_device_mutex);
	disable_irq(client->irq);
	apk_debug_flag = 1;
	fts_ctpm_fw_upgrade_with_app_file(client, fwname);
	apk_debug_flag = 0;
	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

static int ftxxxx_SaveTestData(char *file_name, char *data_buf, int iLen)
{
	struct file *pfile = NULL;
	int err;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, ARRAY_SIZE(filepath));
	snprintf(filepath, ARRAY_SIZE(filepath), "%s%s",
		FTXXXX_DIAG_FILEPATH, file_name);
	if (NULL == pfile)
		pfile = filp_open(filepath,
			O_WRONLY|O_CREAT|O_TRUNC|O_SYNC,
				0664);

	if (IS_ERR(pfile)) {
		pr_debug("[focal] dfs path doesn't exist: %s\n", filepath);

		snprintf(filepath, ARRAY_SIZE(filepath), "%s%s",
			FTXXXX_DATA_FILEPATH, file_name);
		pfile = filp_open(filepath,
			O_WRONLY|O_CREAT|O_TRUNC|O_SYNC,
				0664);

		if (IS_ERR(pfile)) {
			pr_err("[focal] error occurred while opening file %s\n",
				filepath);

			return -EIO;
		}
	}

	pr_debug("[focal] occurred %s\n", filepath);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	pr_debug("[focal] ft_vendor_id = 0x%x, ft_routing_type=0x%x\n",
		ft_vendor_id, ft_routing_type);
	iLen += snprintf(data_buf + iLen, TESTDATA_SIZE,
		"TP ID=0x%X, Routing Type=0x%X\n",
		ft_vendor_id, ft_routing_type);

	if (1 == g_test)
		iLen += snprintf(data_buf + iLen, TESTDATA_SIZE,
			"Test Result PASS\n");
	else
		iLen += snprintf(data_buf + iLen, TESTDATA_SIZE,
			"Test Result FAIL\n");

	err = vfs_write(pfile, data_buf, iLen, &pos);
	if (err < 0)
		pr_err("[focal] write fail!\n");

	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

static int ftxxxx_ReadInIData(char *config_name, char *config_buf)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;

	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;

	memset(filepath, 0, ARRAY_SIZE(filepath));
	snprintf(filepath, ARRAY_SIZE(filepath), "%s", config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occurred while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, config_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

static int ftxxxx_GetInISize(char *config_name)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;

	off_t fsize = 0;
	char filepath[128];

	memset(filepath, 0 , ARRAY_SIZE(filepath));
	snprintf(filepath, ARRAY_SIZE(filepath), "%s", config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occurred while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	fsize = inode->i_size;
	filp_close(pfile, NULL);

	return fsize;
}

static int ftxxxx_get_testparam_from_ini(char *config_name)
{
	char *filedata = NULL;
	int inisize = ftxxxx_GetInISize(config_name);

	pr_debug("[focal] inisize = %d\n ", inisize);

	if (inisize <= 4096) {
		pr_err("%s ERROR:Get firmware size failed\n", __func__);
		return -EIO;
	}

	filedata = kmalloc(inisize + 1, GFP_ATOMIC);

	if (ftxxxx_ReadInIData(config_name, filedata)) {
		pr_err("[focal] %s() - ERROR: request_firmware failed\n",
			__func__);
		kfree(filedata);
		return -EIO;
	}
	pr_debug("[focal] ftxxxx_ReadInIData successful\n");

	set_param_data(filedata);
	return 0;
}

int FTS_I2c_Read(unsigned char *wBuf, int wLen, unsigned char *rBuf, int rLen)
{
	if (NULL == G_Client)
		return -1;

	return ftxxxx_i2c_Read(G_Client, wBuf, wLen, rBuf, rLen);
}

int FTS_I2c_Write(unsigned char *wBuf, int wLen)
{
	if (NULL == G_Client)
		return -1;

	return ftxxxx_i2c_Write(G_Client, wBuf, wLen);
}

static ssize_t ftxxxx_ftsscaptest_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	char cfgname[128] = {0};
	char *testdata = NULL;
	int iTestDataLen = 0;

	testdata = kmalloc(TESTDATA_SIZE, GFP_ATOMIC);
	if (!testdata)
		return -ENOMEM;

	memset(cfgname, 0, ARRAY_SIZE(cfgname));
	snprintf(cfgname, ARRAY_SIZE(cfgname), "%s", buf);
	cfgname[count-1] = '\0';
	pr_debug("[focal] get cfgname, %s\n", cfgname);

	mutex_lock(&g_device_mutex);
	apk_debug_flag = 1;
	init_i2c_write_func(FTS_I2c_Write);
	init_i2c_read_func(FTS_I2c_Read);
	if (ftxxxx_get_testparam_from_ini(cfgname) < 0)
		pr_err("[focal] get testparam from ini failure %s\n", cfgname);
	else {
		pr_err("[focal] TP test Start...\n");
		if (true == start_test_tp()) {
			g_test = 1;
			pr_err("[focal] TP test pass\n");
		} else {
			g_test = 0;
			pr_err("[focal] TP test failure\n");
		}

		iTestDataLen = get_test_data(testdata);
		pr_err("[focal] test data = %s\n", testdata);
		ftxxxx_SaveTestData("focal_log.csv", testdata, iTestDataLen);
		free_test_param_data();
	}

	apk_debug_flag = 0;
	mutex_unlock(&g_device_mutex);
	if (NULL != testdata)
		kfree(testdata);

	return count;
}

/************************************************************************
* Name: fts_esd_show
* Brief:  show ESD status
* Input: device, device attribute, char buf
* Output: print value of ESD protection mechanism
* Return: char number
***********************************************************************/
static ssize_t fts_esd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t status = 0;

	status = scnprintf(buf, 4, "%d\n", apk_debug_flag);

	return status;
}

/************************************************************************
* Name: fts_esd_store
* Brief:  write value to disable/enable ESD protection mechanism
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_esd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = kstrtoint(buf, 0, &input);
	if (ret != 0)
		return -EINVAL;

	if (input == 1)
		apk_debug_flag = 1;
	else if (input == 0)
		apk_debug_flag = 0;
	else
		return -EINVAL;

	return count;
}

/*sysfs
* disable/enable ESD protection mechanism
*example:echo 1 > ftsesd
*/
static DEVICE_ATTR(ftsesd, S_IWUSR | S_IWGRP | S_IRUGO,
			fts_esd_show, fts_esd_store);

/*sysfs
*get the fw version
*example:cat ftstpfwver
*/
static DEVICE_ATTR(ftstpfwver, S_IRUGO, fts_tpfwver_show, NULL);

/*upgrade from *.i
*example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(ftsfwupdate, S_IWUSR | S_IWGRP, NULL, fts_fwupdate_store);

/*read and write register
*read example: echo 88 > ftstprwreg ---read register 0x88
*write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IWUSR | S_IWGRP, NULL, fts_tprwreg_store);

/*upgrade from app.bin
*example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(ftsfwupgradeapp, S_IWUSR | S_IWGRP, NULL,
		   fts_fwupgradeapp_store);

/* RAW test and Differ test
 * example: echo limit.ini > ftsscaptest
 *  cat result.csv
*/
static DEVICE_ATTR(ftsscaptest, S_IWUSR | S_IWGRP, NULL,
	ftxxxx_ftsscaptest_store);

#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
/************************************************************************
* Name: fts_gesture_wakeup_show
* Brief:  show Gesture Wakeup status
* Input: device, device attribute, char buf
* Output: print value of Guesture Wakeup enable status
* Return: char number
***********************************************************************/
static ssize_t fts_gesture_wakeup_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t status = 0;

	status = scnprintf(buf, 4, "%d\n", gesture_wakeup_enabled);

	return status;
}

/************************************************************************
* Name: fts_gesture_wakeup_store
* Brief:  write value to disable/enable Gesture Wakeup
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_gesture_wakeup_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = kstrtoint(buf, 0, &input);
	if (ret != 0)
		return -EINVAL;

	mutex_lock(&fts_gesture_wakeup_mutex);
	if (input == 1) {
		gesture_wakeup_enabled = 1;
		mutex_unlock(&fts_gesture_wakeup_mutex);
	}
	else if (input == 0) {
		gesture_wakeup_enabled = 0;
		mutex_unlock(&fts_gesture_wakeup_mutex);
	}
	else {
		mutex_unlock(&fts_gesture_wakeup_mutex);
		return -EINVAL;
	}

	return count;
}

/*sysfs
* disable/enable Gesture Wakeup
*example:echo 1 > ftsgesturewakeup
*/
static DEVICE_ATTR(ftsgesturewakeup, S_IWUSR | S_IWGRP | S_IRUGO,
			fts_gesture_wakeup_show, fts_gesture_wakeup_store);
#endif

/*add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_ftsesd.attr,
	&dev_attr_ftstpfwver.attr,
	&dev_attr_ftsfwupdate.attr,
	&dev_attr_ftstprwreg.attr,
	&dev_attr_ftsfwupgradeapp.attr,
	&dev_attr_ftsscaptest.attr,
#ifdef CONFIG_TOUCHSCREEN_GESTURE_WAKEUP
	&dev_attr_ftsgesturewakeup.attr,
#endif
	NULL
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes
};

/************************************************************************
* Name: fts_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_sysfs(struct i2c_client *client)
{
	int err;

	err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
	if (err != 0) {
		pr_err("[focal] %s - ERROR: sysfs_create_group() failed.\n",
			__func__);
		sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
		return -EIO;
	}

	mutex_init(&g_device_mutex);
	pr_debug("[focal] %s - sysfs_create_group() succeeded.\n", __func__);

	err = hid_to_i2c(client);
	if (err == 0) {
		pr_err("[focal] %s - ERROR: hid_to_i2c failed.\n", __func__);
		return -EIO;
	}

	return 0;
}

/************************************************************************
* Name: fts_release_sysfs
* Brief:  release sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
void fts_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
	mutex_destroy(&g_device_mutex);
}

/*create apk debug channel*/
#define PROC_UPGRADE		0
#define PROC_READ_REGISTER	1
#define PROC_WRITE_REGISTER	2
#define PROC_AUTOCLB		4
#define PROC_UPGRADE_INFO	5
#define PROC_WRITE_DATA	6
#define PROC_READ_DATA		7
#define PROC_SET_TEST_FLAG	8
#define PROC_NAME		"ftxxxx-debug"

static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;

static ssize_t fts_debug_read(struct file *file, char __user *buf,
			      size_t count, loff_t *ppos);
static ssize_t fts_debug_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos);

static const struct file_operations fts_proc_fops = {
	.owner = THIS_MODULE,
	.read = fts_debug_read,
	.write = fts_debug_write,
};

/************************************************************************
* interface of write proc
* Name: fts_debug_write
* Brief:  interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static ssize_t fts_debug_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct i2c_client *client = G_Client;
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = count;
	int writelen = 0;
	int ret = 0;

	if (copy_from_user(&writebuf, buf, buflen)) {
		dev_err(&client->dev, "[focal] %s:copy from user error\n",
			__func__);
		return -EFAULT;
	}

	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];

			memset(upgrade_file_path, 0,
				ARRAY_SIZE(upgrade_file_path));
			snprintf(upgrade_file_path,
				ARRAY_SIZE(upgrade_file_path),
				"%s", writebuf + 1);
			upgrade_file_path[buflen - 1] = '\0';
			pr_debug("[focal] %s\n", upgrade_file_path);
			disable_irq(client->irq);
			apk_debug_flag = 1;
			ret = fts_ctpm_fw_upgrade_with_app_file(
				client, upgrade_file_path);
			apk_debug_flag = 0;
			enable_irq(client->irq);
			if (ret < 0) {
				dev_err(&client->dev,
					"[focal] %s:upgrade failed.\n",
					__func__);
				return ret;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "[focal] %s:write iic error\n",
				__func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "[focal] %s:write iic error\n",
				__func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		pr_debug("[focal] %s: autoclb\n", __func__);
		fts_ctpm_auto_clb(client);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "[focal] %s:write iic error\n",
				__func__);
			return ret;
		}
		break;
	default:
		break;
	}

	return count;
}

/************************************************************************
* interface of read proc
* Name: fts_debug_read
* Brief:  interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use
* Output: page point to data
* Return: read char number
***********************************************************************/
static ssize_t fts_debug_read(struct file *file, char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct i2c_client *client = G_Client;
	int ret = 0;
	unsigned char *buffer = NULL;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;

	buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling fts_debug_write to upgrade */
		regaddr = 0xA6;
		ret = fts_read_reg(client, regaddr, &regvalue);
		if (ret < 0) {
			num_read_chars = snprintf(buffer, PAGE_SIZE,
						 "%s",
						 "get fw version failed.\n");
		} else {
			num_read_chars = snprintf(buffer, PAGE_SIZE,
						 "current fw version:0x%02x\n",
						 regvalue);
		}
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = ftxxxx_i2c_Read(client, NULL, 0, buffer, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "[focal] %s:read iic error\n",
				__func__);
			return ret;
		}
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = ftxxxx_i2c_Read(client, NULL, 0, buffer, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "[focal] %s:read iic error\n",
				__func__);
			return ret;
		}
		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}

	memcpy(buf, buffer, num_read_chars);
	kfree(buffer);

	return num_read_chars;
}

/************************************************************************
* Name: fts_create_apk_debug_channel
* Brief:  create apk debug channel
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_apk_debug_channel(struct i2c_client *client)
{
	fts_proc_entry = proc_create(PROC_NAME, 0440, NULL, &fts_proc_fops);
	G_Client = client;

	if (NULL == fts_proc_entry) {
		dev_err(&client->dev, "[focal] Couldn't create proc entry!\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "[focal] Create proc entry success!\n");

	return 0;
}

/************************************************************************
* Name: fts_release_apk_debug_channel
* Brief:  release apk debug channel
* Input: no
* Output: no
* Return: no
***********************************************************************/
void fts_release_apk_debug_channel(void)
{
	if (fts_proc_entry)
		proc_remove(fts_proc_entry);
}
