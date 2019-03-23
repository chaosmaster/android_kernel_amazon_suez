/*
 * rt5514.h  --  ALC5514 ALSA SoC audio driver
 *
 * Copyright 2015 Realtek Microelectronics
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#ifndef __RT5514_H__
#define __RT5514_H__

#define RT5514_DEVICE_ID			0x10ec5514

#define RT5514_DSP_WDG_1			0x18001300
#define RT5514_DSP_WDG_2			0x18001304
#define RT5514_DSP_WDG_3			0x18001308
#define RT5514_DSP_CLK_2			0x18001104
#define RT5514_DSP_CLK_6			0x18001114
#define RT5514_DSP_CLK_7			0x18001118
#define RT5514_DRIVER_CTRL0			0x18001028
#define RT5514_RESET				0x18002000
#define RT5514_PWR_ANA1				0x18002004
#define RT5514_PWR_ANA2				0x18002008
#define RT5514_I2S_CTRL1			0x18002010
#define RT5514_I2S_CTRL2			0x18002014
#define RT5514_VAD_CTRL6			0x18002030
#define RT5514_EXT_VAD_CTRL			0x1800206c
#define RT5514_DIG_IO_CTRL			0x18002070
#define RT5514_PAD_CTRL1			0x18002080
#define RT5514_DMIC_DATA_CTRL			0x180020a0
#define RT5514_DIG_SOURCE_CTRL			0x180020a4
#define RT5514_SRC_CTRL				0x180020ac
#define RT5514_DOWNFILTER2_CTRL1		0x180020d0
#define RT5514_PLL_SOURCE_CTRL			0x18002100
#define RT5514_CLK_CTRL1			0x18002104
#define RT5514_CLK_CTRL2			0x18002108
#define RT5514_PLL3_CALIB_CTRL1			0x18002110
#define RT5514_PLL3_CALIB_CTRL5			0x18002124
#define RT5514_DELAY_BUF_CTRL1			0x18002140
#define RT5514_DELAY_BUF_CTRL3			0x18002148
#define RT5514_DOWNFILTER0_CTRL1		0x18002190
#define RT5514_DOWNFILTER0_CTRL2		0x18002194
#define RT5514_DOWNFILTER0_CTRL3		0x18002198
#define RT5514_DOWNFILTER1_CTRL1		0x180021a0
#define RT5514_DOWNFILTER1_CTRL2		0x180021a4
#define RT5514_DOWNFILTER1_CTRL3		0x180021a8
#define RT5514_ANA_CTRL_LDO10			0x18002200
#define RT5514_ANA_CTRL_LDO18_16		0x18002204
#define RT5514_ANA_CTRL_ADC12			0x18002210
#define RT5514_ANA_CTRL_ADC21			0x18002214
#define RT5514_ANA_CTRL_ADC22			0x18002218
#define RT5514_ANA_CTRL_ADC23			0x1800221c
#define RT5514_ANA_CTRL_MICBST			0x18002220
#define RT5514_ANA_CTRL_ADCFED			0x18002224
#define RT5514_ANA_CTRL_INBUF			0x18002228
#define RT5514_ANA_CTRL_VREF			0x1800222c
#define RT5514_ANA_CTRL_PLL3			0x18002240
#define RT5514_ANA_CTRL_PLL1_1			0x18002260
#define RT5514_ANA_CTRL_PLL1_2			0x18002264
#define RT5514_DMIC_LP_CTRL			0x18002e00
#define RT5514_MISC_CTRL_DSP			0x18002e04
#define RT5514_DSP_CTRL1			0x18002f00
#define RT5514_DSP_CTRL3			0x18002f08
#define RT5514_DSP_CTRL4			0x18002f10
#define RT5514_VENDOR_ID1			0x18002ff0
#define RT5514_VENDOR_ID2			0x18002ff4
#define RT5514_I2C_BYPASS			0xfafafafa
#define RT5514_DBG_RING1_ADDR			0x4ff60000
#define RT5514_DBG_RING2_ADDR			0x4ff60014
#define RT5514_DBG_RING3_ADDR			0x4ff60028
#define RT5514_DBG_RING4_ADDR			0x4ff6003c
#define RT5514_INT_VOICE_ENGY			0x4ff60050
#define RT5514_INT_NOISE_ENGY			0x4ff60058

/* RT5514_PWR_ANA1 (0x2004) */
#define RT5514_POW_LDO18_IN			(0x1 << 5)
#define RT5514_POW_LDO18_IN_BIT			5
#define RT5514_POW_LDO18_ADC			(0x1 << 4)
#define RT5514_POW_LDO18_ADC_BIT		4
#define RT5514_POW_LDO21			(0x1 << 3)
#define RT5514_POW_LDO21_BIT			3
#define RT5514_POW_BG_LDO18_IN			(0x1 << 2)
#define RT5514_POW_BG_LDO18_IN_BIT		2
#define RT5514_POW_BG_LDO21			(0x1 << 1)
#define RT5514_POW_BG_LDO21_BIT			1

/* RT5514_PWR_ANA2 (0x2008) */
#define RT5514_POW_PLL1				(0x1 << 18)
#define RT5514_POW_PLL1_BIT			18
#define RT5514_POW_PLL1_LDO			(0x1 << 16)
#define RT5514_POW_PLL1_LDO_BIT			16
#define RT5514_POW_BG_MBIAS			(0x1 << 15)
#define RT5514_POW_BG_MBIAS_BIT			15
#define RT5514_POW_MBIAS			(0x1 << 14)
#define RT5514_POW_MBIAS_BIT			14
#define RT5514_POW_VREF2			(0x1 << 13)
#define RT5514_POW_VREF2_BIT			13
#define RT5514_POW_VREF1			(0x1 << 12)
#define RT5514_POW_VREF1_BIT			12
#define RT5514_POWR_LDO16			(0x1 << 11)
#define RT5514_POWR_LDO16_BIT			11
#define RT5514_POWL_LDO16			(0x1 << 10)
#define RT5514_POWL_LDO16_BIT			10
#define RT5514_POW_ADC2				(0x1 << 9)
#define RT5514_POW_ADC2_BIT			9
#define RT5514_POW_INPUT_BUF			(0x1 << 8)
#define RT5514_POW_INPUT_BUF_BIT		8
#define RT5514_POW_ADC1_R			(0x1 << 7)
#define RT5514_POW_ADC1_R_BIT			7
#define RT5514_POW_ADC1_L			(0x1 << 6)
#define RT5514_POW_ADC1_L_BIT			6
#define RT5514_POW2_BSTR			(0x1 << 5)
#define RT5514_POW2_BSTR_BIT			5
#define RT5514_POW2_BSTL			(0x1 << 4)
#define RT5514_POW2_BSTL_BIT			4
#define RT5514_POW_BSTR				(0x1 << 3)
#define RT5514_POW_BSTR_BIT			3
#define RT5514_POW_BSTL				(0x1 << 2)
#define RT5514_POW_BSTL_BIT			2
#define RT5514_POW_ADCFEDR			(0x1 << 1)
#define RT5514_POW_ADCFEDR_BIT			1
#define RT5514_POW_ADCFEDL			(0x1 << 0)
#define RT5514_POW_ADCFEDL_BIT			0

/* RT5514_I2S_CTRL1 (0x2010) */
#define RT5514_TDM_MODE				(0x1 << 28)
#define RT5514_TDM_MODE_SFT			28
#define RT5514_I2S_LR_MASK			(0x1 << 26)
#define RT5514_I2S_LR_SFT			26
#define RT5514_I2S_LR_NOR			(0x0 << 26)
#define RT5514_I2S_LR_INV			(0x1 << 26)
#define RT5514_I2S_BP_MASK			(0x1 << 25)
#define RT5514_I2S_BP_SFT			25
#define RT5514_I2S_BP_NOR			(0x0 << 25)
#define RT5514_I2S_BP_INV			(0x1 << 25)
#define RT5514_I2S_DF_MASK			(0x7 << 16)
#define RT5514_I2S_DF_SFT			16
#define RT5514_I2S_DF_I2S			(0x0 << 16)
#define RT5514_I2S_DF_LEFT			(0x1 << 16)
#define RT5514_I2S_DF_PCM_A			(0x2 << 16)
#define RT5514_I2S_DF_PCM_B			(0x3 << 16)
#define RT5514_TDMSLOT_SEL_RX_MASK		(0x3 << 10)
#define RT5514_TDMSLOT_SEL_RX_SFT		10
#define RT5514_TDMSLOT_SEL_RX_4CH		(0x1 << 10)
#define RT5514_CH_LEN_RX_MASK			(0x3 << 8)
#define RT5514_CH_LEN_RX_SFT			8
#define RT5514_CH_LEN_RX_16			(0x0 << 8)
#define RT5514_CH_LEN_RX_20			(0x1 << 8)
#define RT5514_CH_LEN_RX_24			(0x2 << 8)
#define RT5514_CH_LEN_RX_32			(0x3 << 8)
#define RT5514_TDMSLOT_SEL_TX_MASK		(0x3 << 6)
#define RT5514_TDMSLOT_SEL_TX_SFT		6
#define RT5514_TDMSLOT_SEL_TX_4CH		(0x1 << 6)
#define RT5514_CH_LEN_TX_MASK			(0x3 << 4)
#define RT5514_CH_LEN_TX_SFT			4
#define RT5514_CH_LEN_TX_16			(0x0 << 4)
#define RT5514_CH_LEN_TX_20			(0x1 << 4)
#define RT5514_CH_LEN_TX_24			(0x2 << 4)
#define RT5514_CH_LEN_TX_32			(0x3 << 4)
#define RT5514_I2S_DL_MASK			(0x3 << 0)
#define RT5514_I2S_DL_SFT			0
#define RT5514_I2S_DL_16			(0x0 << 0)
#define RT5514_I2S_DL_20			(0x1 << 0)
#define RT5514_I2S_DL_24			(0x2 << 0)
#define RT5514_I2S_DL_8				(0x3 << 0)

/* RT5514_DIG_SOURCE_CTRL (0x20a4) */
#define RT5514_AD1_DMIC_INPUT_SEL		(0x1 << 1)
#define RT5514_AD1_DMIC_INPUT_SEL_SFT		1
#define RT5514_AD0_DMIC_INPUT_SEL		(0x1 << 0)
#define RT5514_AD0_DMIC_INPUT_SEL_SFT		0

/* RT5514_PLL_SOURCE_CTRL (0x2100) */
#define RT5514_PLL_1_SEL_MASK			(0x7 << 12)
#define RT5514_PLL_1_SEL_SFT			12
#define RT5514_PLL_1_SEL_SCLK			(0x3 << 12)
#define RT5514_PLL_1_SEL_MCLK			(0x4 << 12)

/* RT5514_CLK_CTRL1 (0x2104) */
#define RT5514_CLK_AD_ANA1_EN			(0x1 << 31)
#define RT5514_CLK_AD_ANA1_EN_BIT		31
#define RT5514_CLK_AD1_EN			(0x1 << 24)
#define RT5514_CLK_AD1_EN_BIT			24
#define RT5514_CLK_AD0_EN			(0x1 << 23)
#define RT5514_CLK_AD0_EN_BIT			23
#define RT5514_CLK_DMIC_OUT_SEL_MASK		(0x7 << 8)
#define RT5514_CLK_DMIC_OUT_SEL_SFT		8
#define RT5514_CLK_AD_ANA1_SEL_MASK		(0xf << 0)
#define RT5514_CLK_AD_ANA1_SEL_SFT		0

/* RT5514_CLK_CTRL2 (0x2108) */
#define RT5514_CLK_SYS_DIV_OUT_MASK		(0x7 << 8)
#define RT5514_CLK_SYS_DIV_OUT_SFT		8
#define RT5514_SEL_ADC_OSR_MASK			(0x7 << 4)
#define RT5514_SEL_ADC_OSR_SFT			4
#define RT5514_CLK_SYS_PRE_SEL_MASK		(0x3 << 0)
#define RT5514_CLK_SYS_PRE_SEL_SFT		0
#define RT5514_CLK_SYS_PRE_SEL_MCLK		(0x2 << 0)
#define RT5514_CLK_SYS_PRE_SEL_PLL		(0x3 << 0)

/*  RT5514_DOWNFILTER_CTRL (0x2190 0x2194 0x21a0 0x21a4) */
#define RT5514_AD_DMIC_MIX			(0x1 << 11)
#define RT5514_AD_DMIC_MIX_BIT			11
#define RT5514_AD_AD_MIX			(0x1 << 10)
#define RT5514_AD_AD_MIX_BIT			10
#define RT5514_AD_AD_MUTE			(0x1 << 7)
#define RT5514_AD_AD_MUTE_BIT			7
#define RT5514_AD_GAIN_MASK			(0x7f << 0)
#define RT5514_AD_GAIN_SFT			0

/*  RT5514_ANA_CTRL_MICBST (0x2220) */
#define RT5514_SEL_BSTL_MASK			(0xf << 4)
#define RT5514_SEL_BSTL_SFT			4
#define RT5514_SEL_BSTR_MASK			(0xf << 0)
#define RT5514_SEL_BSTR_SFT			0

/*  RT5514_ANA_CTRL_PLL1_1 (0x2260) */
#define RT5514_PLL_K_MAX			0x1f
#define RT5514_PLL_K_MASK			(RT5514_PLL_K_MAX << 16)
#define RT5514_PLL_K_SFT			16
#define RT5514_PLL_N_MAX			0x1ff
#define RT5514_PLL_N_MASK			(RT5514_PLL_N_MAX << 7)
#define RT5514_PLL_N_SFT			4
#define RT5514_PLL_M_MAX			0xf
#define RT5514_PLL_M_MASK			(RT5514_PLL_M_MAX << 0)
#define RT5514_PLL_M_SFT			0

/*  RT5514_ANA_CTRL_PLL1_2 (0x2264) */
#define RT5514_PLL_M_BP				(0x1 << 2)
#define RT5514_PLL_M_BP_SFT			2
#define RT5514_PLL_K_BP				(0x1 << 1)
#define RT5514_PLL_K_BP_SFT			1
#define RT5514_EN_LDO_PLL1			(0x1 << 0)
#define RT5514_EN_LDO_PLL1_BIT			0

#define RT5514_PLL_INP_MAX			40000000
#define RT5514_PLL_INP_MIN			256000

/* System Clock Source */
enum {
	RT5514_SCLK_S_MCLK,
	RT5514_SCLK_S_PLL1,
};

/* PLL1 Source */
enum {
	RT5514_PLL1_S_MCLK,
	RT5514_PLL1_S_BCLK,
};

struct rt5514_pll_code {
	bool m_bp;
	bool k_bp;
	int m_code;
	int n_code;
	int k_code;
};

struct rt5514_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct work_struct hotword_work;
	struct work_struct watchdog_work;
	struct work_struct handler_work;
	struct wake_lock vad_wake;
	struct mutex dspcontrol_lock;
	unsigned int sen_base, sen_limit, sen_rp, sen_wp;
	unsigned int dbg_base, dbg_limit, dbg_rp, dbg_wp;
	unsigned int sen2_base, sen2_limit, sen2_rp, sen2_wp;
	unsigned int sen3_base, sen3_limit, sen3_rp, sen3_wp;
	int sysclk;
	int sysclk_src;
	int lrck;
	int bclk;
	int pll_src;
	int pll_in;
	int pll_out;
	int dsp_enabled;
	int had_suspend;
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
	int dsp_fw_check;
	int sw_reset;
	int dsp_stop;
	int dsp_core_reset;
#endif
	int dsp_idle;
	uint64_t irq_speech_energy;
	uint64_t irq_ambient_energy;
};

typedef struct {
	unsigned int Offset;
	unsigned int Size;
	unsigned int Addr;
} SMicBinInfo;

typedef struct {
	unsigned int ID;
	unsigned int Addr;
} SMicTDInfo;

#define	SMICFW_SYNC			0x23795888

typedef struct {
	unsigned int Sync;
	unsigned int Version;
	unsigned int NumBin;
	unsigned int dspclock;
	unsigned int reserved[8];
	SMicBinInfo *BinArray;
} SMicFWHeader;

typedef struct {
	unsigned int NumTD;
	SMicTDInfo *TDArray;
} SMicFWSubHeader;

void rt5514_reset_duetoSPI(void);
#endif /* __RT5514_H__ */
