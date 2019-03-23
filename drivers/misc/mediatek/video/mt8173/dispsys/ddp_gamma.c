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

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "cmdq_record.h"
#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_dither.h"
#include "ddp_gamma.h"
#include "ddp_log.h"

static DEFINE_MUTEX(g_gamma_global_lock);


/* ======================================================================== */
/*  GAMMA                                                                   */
/* ======================================================================== */

static DISP_GAMMA_LUT_T *g_disp_gamma_lut[DISP_GAMMA_TOTAL] = { NULL, NULL };

static ddp_module_notify g_gamma_ddp_notify;


static int disp_gamma_write_lut_reg(cmdqRecHandle cmdq, disp_gamma_id_t id, int lock);
static void disp_ccorr_init(disp_ccorr_id_t id, unsigned int width, unsigned int height,
			    void *cmdq);


void disp_gamma_init(disp_gamma_id_t id, unsigned int width, unsigned int height, void *cmdq)
{
	if (id == DISP_GAMMA1) {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_EN, 0x1, 0x1);
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_SIZE, ((width << 16) | height), ~0);
	}

	disp_gamma_write_lut_reg(cmdq, id, 1);

	{			/* Init CCORR */
		disp_ccorr_id_t ccorr_id = DISP_CCORR0;

		if (id == DISP_GAMMA1)
			ccorr_id = DISP_CCORR1;
		disp_ccorr_init(ccorr_id, width, height, cmdq);
	}
}


static int disp_gamma_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty) {
		disp_gamma_init(DISP_GAMMA1, pConfig->dst_w, pConfig->dst_h, cmdq);
		disp_dither_init(DISP_DITHER1, pConfig->lcm_bpp, cmdq);
	}

	return 0;
}


static void disp_gamma_trigger_refresh(disp_gamma_id_t id)
{
	if (g_gamma_ddp_notify != NULL) {
		if (id == DISP_GAMMA0)
			g_gamma_ddp_notify(DISP_MODULE_AAL, DISP_PATH_EVENT_TRIGGER);
		else
			g_gamma_ddp_notify(DISP_MODULE_GAMMA, DISP_PATH_EVENT_TRIGGER);
	}
}


static int disp_gamma_write_lut_reg(cmdqRecHandle cmdq, disp_gamma_id_t id, int lock)
{
	unsigned long lut_base = DISP_AAL_GAMMA_LUT;
	DISP_GAMMA_LUT_T *gamma_lut;
	int i;
	int ret = 0;

	if (id >= DISP_GAMMA_TOTAL) {
		DDPMSG("[GAMMA] disp_gamma_write_lut_reg: invalid ID = %d\n", id);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	gamma_lut = g_disp_gamma_lut[id];
	if (gamma_lut == NULL) {
		DDPMSG("[GAMMA] disp_gamma_write_lut_reg: gamma table [%d] not initialized\n", id);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (id == DISP_GAMMA0) {
		if (DISP_REG_GET(DISP_AAL_EN) == 0) {
			DDPMSG("[GAMMA][WARNING] DISP_AAL_EN not enabled!\n");
			DISP_REG_MASK(cmdq, DISP_AAL_EN, 0x1, 0x1);
		}

		DISP_REG_MASK(cmdq, DISP_AAL_CFG, 0x2, 0x2);
		lut_base = DISP_AAL_GAMMA_LUT;
	} else if (id == DISP_GAMMA1) {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_EN, 0x1, 0x1);
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG, 0x2, 0x2);
		lut_base = DISP_REG_GAMMA_LUT;
	} else {
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		DISP_REG_MASK(cmdq, (lut_base + i * 4), gamma_lut->lut[i], ~0);

		if ((i & 0x3f) == 0) {
			DDPDBG("[GAMMA] [0x%lx](%d) = 0x%x\n", (lut_base + i * 4), i,
			       gamma_lut->lut[i]);
		}
	}
	i--;
	DDPDBG("[GAMMA] [0x%lx](%d) = 0x%x\n", (lut_base + i * 4), i, gamma_lut->lut[i]);

gamma_write_lut_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}


static int disp_gamma_set_lut(const DISP_GAMMA_LUT_T __user *user_gamma_lut, void *cmdq)
{
	int ret = 0;
	disp_gamma_id_t id;
	DISP_GAMMA_LUT_T *gamma_lut, *old_lut;

	gamma_lut = kmalloc(sizeof(DISP_GAMMA_LUT_T), GFP_KERNEL);
	if (gamma_lut == NULL) {
		DDPERR("[GAMMA] disp_gamma_set_lut: no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(gamma_lut, user_gamma_lut, sizeof(DISP_GAMMA_LUT_T)) != 0) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		id = gamma_lut->hw_id;
		if (0 <= id && id < DISP_GAMMA_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_lut = g_disp_gamma_lut[id];
			g_disp_gamma_lut[id] = gamma_lut;

			ret = disp_gamma_write_lut_reg(cmdq, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_lut != NULL)
				kfree(old_lut);

			disp_gamma_trigger_refresh(id);
		} else {
			DDPERR("[GAMMA] disp_gamma_set_lut: invalid ID = %d\n", id);
			ret = -EFAULT;
		}
	}

	return ret;
}


/* ======================================================================== */
/*  COLOR CORRECTION                                                        */
/* ======================================================================== */

static DISP_CCORR_COEF_T *g_disp_ccorr_coef[DISP_CCORR_TOTAL] = { NULL, NULL };

static int disp_ccorr_write_coef_reg(cmdqRecHandle cmdq, disp_ccorr_id_t id, int lock);


static void disp_ccorr_init(disp_ccorr_id_t id, unsigned int width, unsigned int height, void *cmdq)
{
	disp_ccorr_write_coef_reg(cmdq, id, 1);
}


#define CCORR_REG(base, idx) (base + (idx) * 4)

static int disp_ccorr_write_coef_reg(cmdqRecHandle cmdq, disp_ccorr_id_t id, int lock)
{
	unsigned long ccorr_base = 0;
	int ret = 0;
	int is_identity = 0;
	DISP_CCORR_COEF_T *ccorr;

	if (id >= DISP_CCORR_TOTAL) {
		DDPERR("[GAMMA] disp_gamma_write_lut_reg: invalid ID = %d\n", id);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	ccorr = g_disp_ccorr_coef[id];
	if (ccorr == NULL) {
		DDPMSG("[GAMMA] disp_ccorr_write_coef_reg: [%d] not initialized\n", id);
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	if ((ccorr->coef[0][0] == 1024) && (ccorr->coef[0][1] == 0) && (ccorr->coef[0][2] == 0) &&
	    (ccorr->coef[1][0] == 0) && (ccorr->coef[1][1] == 1024) && (ccorr->coef[1][2] == 0) &&
	    (ccorr->coef[2][0] == 0) && (ccorr->coef[2][1] == 0) && (ccorr->coef[2][2] == 1024)) {
		is_identity = 1;
	}

	if (id == DISP_CCORR0) {
		ccorr_base = DISP_AAL_CCORR(0);
		DISP_REG_MASK(cmdq, DISP_AAL_CFG, (!is_identity) << 4, 0x1 << 4);
	} else if (id == DISP_CCORR1) {
		ccorr_base = DISP_GAMMA_CCORR_0;
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG, (!is_identity) << 4, 0x1 << 4);
	} else {
		DDPERR("[GAMMA] disp_gamma_write_ccorr_reg: invalid ID = %d\n", id);
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 0),
		     ((ccorr->coef[0][0] << 16) | (ccorr->coef[0][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 1),
		     ((ccorr->coef[0][2] << 16) | (ccorr->coef[1][0])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 2),
		     ((ccorr->coef[1][1] << 16) | (ccorr->coef[1][2])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 3),
		     ((ccorr->coef[2][0] << 16) | (ccorr->coef[2][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 4), (ccorr->coef[2][2] << 16));

ccorr_write_coef_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}


static int disp_ccorr_set_coef(const DISP_CCORR_COEF_T __user *user_color_corr, void *cmdq)
{
	int ret = 0;
	DISP_CCORR_COEF_T *ccorr, *old_ccorr;
	disp_ccorr_id_t id;

	ccorr = kmalloc(sizeof(DISP_CCORR_COEF_T), GFP_KERNEL);
	if (ccorr == NULL) {
		DDPERR("[GAMMA] disp_ccorr_set_coef: no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(ccorr, user_color_corr, sizeof(DISP_CCORR_COEF_T)) != 0) {
		ret = -EFAULT;
		kfree(ccorr);
	} else {
		id = ccorr->hw_id;
		if (0 <= id && id < DISP_CCORR_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_ccorr = g_disp_ccorr_coef[id];
			g_disp_ccorr_coef[id] = ccorr;

			ret = disp_ccorr_write_coef_reg(cmdq, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_ccorr != NULL)
				kfree(old_ccorr);

			disp_gamma_trigger_refresh(id);
		} else {
			DDPERR("[GAMMA] disp_ccorr_set_coef: invalid ID = %d\n", id);
			ret = -EFAULT;
		}
	}

	return ret;
}


static int disp_gamma_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	switch (msg) {
	case DISP_IOCTL_SET_GAMMALUT:
		if (disp_gamma_set_lut((DISP_GAMMA_LUT_T *) arg, cmdq) < 0) {
			DDPERR("DISP_IOCTL_SET_GAMMALUT: failed\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_SET_CCORR:
		if (disp_ccorr_set_coef((DISP_CCORR_COEF_T *) arg, cmdq) < 0) {
			DDPERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
		break;
	}

	return 0;
}


static int disp_gamma_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_gamma_ddp_notify = notify;
	return 0;
}


DDP_MODULE_DRIVER ddp_driver_gamma = {
	.config = disp_gamma_config,
	.set_listener = disp_gamma_set_listener,
	.cmd = disp_gamma_io
};

void enable_gamma(disp_gamma_id_t id, int enable)
{
	if (id == DISP_GAMMA1) {
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_EN, enable, 0x1);
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, (enable << 1) | (1 - enable) | ((1 - enable) << 5), 0x23);
	} else if (id == DISP_GAMMA0) {
		DISP_REG_MASK(NULL, DISP_AAL_EN, enable, 0x1);
		DISP_REG_MASK(NULL, DISP_AAL_CFG, (enable << 1) | (1 - enable) | ((1 - enable) << 5), 0x23);
	}
}

int query_gamma_status(disp_gamma_id_t id)
{
	int ret;

	if (id == DISP_GAMMA1) {
		ret = DISP_REG_GET(DISP_REG_GAMMA_EN);
		if (0 == (ret & 0x1))
			return 0;
		ret = DISP_REG_GET(DISP_REG_GAMMA_CFG);
		if (0x2 == (ret & 0x23))
			return 1;
		else
			return 0;
	} else if (id == DISP_GAMMA0) {
		ret = DISP_REG_GET(DISP_AAL_EN);
		if (0 == (ret & 0x1))
			return 0;
		ret = DISP_REG_GET(DISP_AAL_CFG);
		if (0x2 == (ret & 0x23))
			return 1;
		else
			return 0;
	}

	return -1;
}

void gamma_test(const char *cmd, char *debug_output)
{
	int ret;
	disp_gamma_id_t gamma_id;

	debug_output[0] = '\0';
	DDPMSG("[GAMMA_DEBUG] gamma_test:%s", cmd);

	if (strncmp(cmd, "GAMMA0:", 7) == 0) {
		gamma_id = DISP_GAMMA0;
	} else if (strncmp(cmd, "GAMMA1:", 7) == 0) {
		gamma_id = DISP_GAMMA1;
	} else {
		memcpy(debug_output, "Wrong GAMMA ID\n", 15);
		return;
	}
	cmd += 7;

	if (strncmp(cmd, "en:", 3) == 0) {
		int enable;

		cmd += 3;

		if (cmd[0] - '0' > 1 && cmd[0] - '0' < 0) {
			memcpy(debug_output, "cmd param error\n", 16);
			return;
		}
		enable = cmd[0] - '0';
		enable_gamma(gamma_id, enable);

		memcpy(debug_output, "Success\n", 8);
	} else if (strncmp(cmd, "query_status", 12) == 0) {
		ret = query_gamma_status(gamma_id);
		if (1 == ret)
			memcpy(debug_output, "Enabled\n", 8);
		else if (0 == ret)
			memcpy(debug_output, "Disabled\n", 9);
	}
}

void enable_ccorr(disp_ccorr_id_t id, int enable)
{
	if (id == DISP_CCORR1) {
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_EN, enable, 0x1);
#if 1
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, (enable << 4) | (1 - enable), 0x11);
#else /*CCORR with no GAMMA*/
		/*DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, (enable << 4) | (1 - enable) | (enable << 5), 0x31);*/
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, (1 - enable) | (enable << 5), 0x21);
#endif
	} else if (id == DISP_CCORR0) {
		DISP_REG_MASK(NULL, DISP_AAL_EN, enable, 0x1);
#if 1
		DISP_REG_MASK(NULL, DISP_AAL_CFG, (enable << 4) | (1 - enable), 0x11);
#else /*CCORR with no GAMMA*/
		/*DISP_REG_MASK(NULL, DISP_AAL_CFG, (enable << 4) | (1 - enable) | (enable << 5), 0x31);*/
		DISP_REG_MASK(NULL, DISP_AAL_CFG, (1 - enable) | (enable << 5), 0x21);
#endif
	}
}

int query_ccorr_status(disp_ccorr_id_t id)
{
	int ret;

	if (id == DISP_CCORR1) {
		ret = DISP_REG_GET(DISP_REG_GAMMA_EN);
		if (0 == (ret & 0x1))
			return 0;
		ret = DISP_REG_GET(DISP_REG_GAMMA_CFG);
		if (0x2 == (ret & 0x23))
			return 1;
		else
			return 0;
	} else if (id == DISP_CCORR0) {
		ret = DISP_REG_GET(DISP_AAL_EN);
		if (0 == (ret & 0x1))
			return 0;
		ret = DISP_REG_GET(DISP_AAL_CFG);
		if (0x10 == (ret & 0x11))
			return 1;
		else
			return 0;
	}

	return -1;
}

int	_set_ccorr_table(disp_ccorr_id_t id, unsigned int coef[3][3])
{
	int is_identity = 0;
	unsigned long ccorr_base = 0;

	if ((coef[0][0] == 1024) && (coef[0][1] == 0) && (coef[0][2] == 0) &&
	    (coef[1][0] == 0) && (coef[1][1] == 1024) && (coef[1][2] == 0) &&
	    (coef[2][0] == 0) && (coef[2][1] == 0) && (coef[2][2] == 1024)) {
		is_identity = 1;
	}

	if (id == DISP_CCORR0) {
		ccorr_base = DISP_AAL_CCORR(0);
		DISP_REG_MASK(NULL, DISP_AAL_CFG, (!is_identity) << 4, 0x1 << 4);
	} else if (id == DISP_CCORR1) {
		ccorr_base = DISP_GAMMA_CCORR_0;
		DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, (!is_identity) << 4, 0x1 << 4);
	} else {
		DDPERR("[CCORR] invalid ID = %d\n", id);
		return -1;
	}

	DISP_REG_SET(NULL, CCORR_REG(ccorr_base, 0),
		     ((coef[0][0] << 16) | (coef[0][1])));
	DISP_REG_SET(NULL, CCORR_REG(ccorr_base, 1),
		     ((coef[0][2] << 16) | (coef[1][0])));
	DISP_REG_SET(NULL, CCORR_REG(ccorr_base, 2),
		     ((coef[1][1] << 16) | (coef[1][2])));
	DISP_REG_SET(NULL, CCORR_REG(ccorr_base, 3),
		     ((coef[2][0] << 16) | (coef[2][1])));
	DISP_REG_SET(NULL, CCORR_REG(ccorr_base, 4), (coef[2][2] << 16));

	return 0;
}


int set_ccorr_table(const char *str, disp_ccorr_id_t ccorr_id)
{
	int ret = 0;
	unsigned int coef[3][3];
	const char *p_begin, *p_end;
	int i, j;
	char tmp[6];
	int length = 0;

	/* get ccorr table from str */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			p_begin = str;
			p_end = strchr(str, ',');
			if (p_end == NULL || p_end - p_begin > 5) {
				p_end = strchr(str, '\0');
				if (p_end == NULL)
					return -1;
			}
			length = p_end - p_begin;
			if (length > 5)
				return -2;

			memcpy(tmp, p_begin, length);
			tmp[length] = '\0';

			str += (length + 1);

			ret = kstrtouint(tmp, 0, (unsigned int *)&(coef[i][j]));
			if (ret != 0)
				return -3;
		}
	}

	if (i != 3 || j != 3)
		return -4;

	ret = _set_ccorr_table(ccorr_id, (unsigned int (*)[])coef);

	return ret;
}

void ccorr_test(const char *cmd, char *debug_output)
{
	int ret;
	disp_ccorr_id_t ccorr_id;

	debug_output[0] = '\0';
	DDPMSG("[CCORR_DEBUG] ccorr_test:%s", cmd);

	if (strncmp(cmd, "CCORR0:", 7) == 0) {
		ccorr_id = DISP_CCORR0;
	} else if (strncmp(cmd, "CCORR1:", 7) == 0) {
		ccorr_id = DISP_CCORR1;
	} else {
		memcpy(debug_output, "Wrong CCORR ID\n", 15);
		return;
	}
	cmd += 7;

	if (strncmp(cmd, "en:", 3) == 0) {
		int enable;

		cmd += 3;

		if (cmd[0] - '0' > 1 && cmd[0] - '0' < 0) {
			memcpy(debug_output, "cmd param error\n", 16);
			return;
		}
		enable = cmd[0] - '0';
		enable_ccorr(ccorr_id, enable);

		memcpy(debug_output, "Success\n", 8);
	} else if (strncmp(cmd, "query_status", 12) == 0) {
		ret = query_ccorr_status(ccorr_id);
		if (1 == ret)
			memcpy(debug_output, "Enabled\n", 8);
		else if (0 == ret)
			memcpy(debug_output, "Disabled\n", 9);
	} else if (strncmp(cmd, "set_ccorr_table:", 16) == 0) {
		int ret = 0;

		cmd += 16;
		ret = set_ccorr_table(cmd, ccorr_id);
		if (ret == 0)
			memcpy(debug_output, "Success\n", 8);
		else {
			DDPMSG("[CCORR_DEBUG] set_ccorr_table return: %d\n", ret);
			memcpy(debug_output, "Fail\n", 5);
		}
	}

	return;

}
