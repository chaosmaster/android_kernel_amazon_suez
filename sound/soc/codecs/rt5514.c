/*
 * rt5514.c  --  ALC5514 ALSA SoC audio codec driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include  <linux/metricslog.h>

#include "rt5514.h"
#include "rt5514-spi.h"

#define RT5514_RW_BY_SPI
#define RT5514_USE_AMIC


#define VERSION "0.0.9"
#define VERSION_LEN 16
static char fw_ver[VERSION_LEN];
static int rt5514_dsp_set_idle_mode(struct snd_soc_codec *codec, int IdleMode);
static int rt5514_set_dsp_mode(struct snd_soc_codec *codec, int DSPMode);
static struct rt5514_priv *rt5514_pointer;

static struct reg_default rt5514_init_list[] = {
	{RT5514_DIG_IO_CTRL,		0x00000040},
	{RT5514_CLK_CTRL1,		0x38020041},
	{RT5514_SRC_CTRL,		0x44000eee},
	{RT5514_ANA_CTRL_LDO10,		0x00028604},
	{RT5514_ANA_CTRL_ADCFED,	0x00000800},
	{RT5514_DOWNFILTER0_CTRL3,	0x10000362},
	{RT5514_DOWNFILTER1_CTRL3,	0x10000362},
#ifdef RT5514_USE_AMIC
	{RT5514_PWR_ANA1,		0x00800880},
#endif
};

static int rt5514_reg_init(struct snd_soc_codec *codec)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	int i;

	for (i = 0; i < ARRAY_SIZE(rt5514_init_list); i++)
		regmap_write(rt5514->regmap, rt5514_init_list[i].reg,
			rt5514_init_list[i].def);

	return 0;
}

static const struct reg_default rt5514_reg[] = {
	{RT5514_BUFFER_VOICE_WP,	0x00000000},
	{RT5514_DSP_WDG_1,		0x00000000},
	{RT5514_DSP_WDG_2,		0x00000000},
	{RT5514_DSP_WDG_3,		0x00000000},
	{RT5514_DSP_CLK_2,		0x00000007},
	{RT5514_DSP_CLK_6,		0x00000000},
	{RT5514_DSP_CLK_7,		0x00000000},
	{RT5514_RESET,			0x00000000},
	{RT5514_PWR_ANA1,		0x00808880},
	{RT5514_PWR_ANA2,		0x00220000},
	{RT5514_I2S_CTRL1,		0x00000330},
	{RT5514_I2S_CTRL2,		0x20000000},
	{RT5514_VAD_CTRL6,		0xc00007d2},
	{RT5514_EXT_VAD_CTRL,		0x80000080},
	{RT5514_DIG_IO_CTRL,		0x00f00288},
	{RT5514_PAD_CTRL1,		0x00804000},
	{RT5514_DMIC_DATA_CTRL,		0x00000005},
	{RT5514_DIG_SOURCE_CTRL,	0x00000002},
	{RT5514_SRC_CTRL,		0x00000eee},
	{RT5514_DOWNFILTER2_CTRL1,	0x0000882f},
	{RT5514_PLL_SOURCE_CTRL,	0x00000004},
	{RT5514_CLK_CTRL1,		0x38022841},
	{RT5514_CLK_CTRL2,		0x00000000},
	{RT5514_PLL3_CALIB_CTRL1,	0x00400200},
	{RT5514_PLL3_CALIB_CTRL5,	0x40220012},
	{RT5514_DELAY_BUF_CTRL1,	0x7fff006a},
	{RT5514_DELAY_BUF_CTRL3,	0x00000000},
	{RT5514_DOWNFILTER0_CTRL1,	0x00020c2f},
	{RT5514_DOWNFILTER0_CTRL2,	0x00020c2f},
	{RT5514_DOWNFILTER0_CTRL3,	0x00000362},
	{RT5514_DOWNFILTER1_CTRL1,	0x00020c2f},
	{RT5514_DOWNFILTER1_CTRL2,	0x00020c2f},
	{RT5514_DOWNFILTER1_CTRL3,	0x00000362},
	{RT5514_ANA_CTRL_LDO10,		0x00038604},
	{RT5514_ANA_CTRL_LDO18_16,	0x02000345},
	{RT5514_ANA_CTRL_ADC12,		0x0000a2a8},
	{RT5514_ANA_CTRL_ADC21,		0x00001180},
	{RT5514_ANA_CTRL_ADC22,		0x0000aaa8},
	{RT5514_ANA_CTRL_ADC23,		0x00151427},
	{RT5514_ANA_CTRL_MICBST,	0x00002000},
	{RT5514_ANA_CTRL_ADCFED,	0x00000b88},
	{RT5514_ANA_CTRL_INBUF,		0x00000143},
	{RT5514_ANA_CTRL_VREF,		0x00008d50},
	{RT5514_ANA_CTRL_PLL3,		0x0000000e},
	{RT5514_ANA_CTRL_PLL1_1,	0x00000000},
	{RT5514_ANA_CTRL_PLL1_2,	0x00030220},
	{RT5514_DMIC_LP_CTRL,		0x00000000},
	{RT5514_MISC_CTRL_DSP,		0x00000000},
	{RT5514_DSP_CTRL1,		0x00055149},
	{RT5514_DSP_CTRL3,		0x00000006},
	{RT5514_DSP_CTRL4,		0x00000001},
	{RT5514_VENDOR_ID1,		0x00000001},
	{RT5514_VENDOR_ID2,		RT5514_DEVICE_ID},
};

static void rt5514_enable_dsp_clock(struct rt5514_priv *rt5514)
{
	regmap_write(rt5514->regmap, 0x18002000, 0x000010ec);  //reset
	regmap_write(rt5514->regmap, 0x18002200, 0x00028604);  //LDO_I_limit
	regmap_write(rt5514->regmap, 0x18002f00, 0x0005514b);  //(for reset DSP) mini-core reset
	regmap_write(rt5514->regmap, 0x18002f00, 0x00055149);  //(for reset DSP) mini-core reset
	regmap_write(rt5514->regmap, 0x180020a0, 0x00000000);  //DMIC_OUT1/2=HiZ
#ifdef RT5514_USE_AMIC
	regmap_write(rt5514->regmap, 0x18002070, 0x00000150);  //PIN config
#else
	regmap_write(rt5514->regmap, 0x18002070, 0x00000040);  //PIN config
#endif
	/* 33.8MHz start */
	regmap_write(rt5514->regmap, 0x18002240, 0x00000018);  //pll_qn=19+2 (1.3*21=27.3)
	/* 33.8MHz end */
	regmap_write(rt5514->regmap, 0x18002100, 0x0000000b);  //pll3 source=RCOSC, fsi=rt_clk
	regmap_write(rt5514->regmap, 0x18002004, 0x00808b81);  //PU RCOSC, pll3
	regmap_write(rt5514->regmap, 0x18002008, 0x00220000);  //PD ADC1/2
	regmap_write(rt5514->regmap, 0x18002f08, 0x00000005);  //DSP clk source=pll3, ENABLE DSP clk
	regmap_write(rt5514->regmap, 0x18002104, 0x10023541);  //256fs=/4,DMIC_CLK_OUT=/16(disable ad2)
	regmap_write(rt5514->regmap, 0x18002108, 0x00000000);  //clk_sys source=mux_out
	regmap_write(rt5514->regmap, 0x18001100, 0x00000217);  //DSP clk= 32M*(23+1)/32 =24M
	regmap_write(rt5514->regmap, 0x18002148, 0x80000000);  //DB, pointer
	regmap_write(rt5514->regmap, 0x18002140, 0x3fff00fa);  //DB, pop=4x
	regmap_write(rt5514->regmap, 0x18002148, 0x00000000);  //DB, pointer
	regmap_write(rt5514->regmap, 0x18002124, 0x00220012);  //DFLL reset
	/* 33.8MHz start */
	regmap_write(rt5514->regmap, 0x18002110, 0x000103e8);  //dfll_m=750,dfll_n=1
	/* 33.8MHz end */
	regmap_write(rt5514->regmap, 0x18002124, 0x80220012);  //DFLL,reset DFLL
	regmap_write(rt5514->regmap, 0x18002124, 0xc0220012);  //DFLL
	regmap_write(rt5514->regmap, 0x18002010, 0x10000772);  //(I2S) i2s format, TDM 4ch
	regmap_write(rt5514->regmap, 0x180020ac, 0x44000eee);  //(I2S) source sel; tdm_0=ad0, tdm_1=ad1
#ifdef RT5514_USE_AMIC
	regmap_write(rt5514->regmap, 0x18002190, 0x0002082f);  //(ad0)source of AMIC
	regmap_write(rt5514->regmap, 0x18002194, 0x0002082f);  //(ad0)source of AMIC
	regmap_write(rt5514->regmap, 0x18002198, 0x10003162);  //(ad0)ad0 compensation gain = 3dB
	regmap_write(rt5514->regmap, 0x180020d0, 0x00008937);  //(ad2) gain=12+3dB for WOV
	regmap_write(rt5514->regmap, 0x18002220, 0x00002000);  //(ADC1)micbst gain
	regmap_write(rt5514->regmap, 0x18002224, 0x00000800);  //(ADC1)adcfed unmute and 0dB
#else
	regmap_write(rt5514->regmap, 0x18002190, 0x0002042f);  //(ad0) source of DMIC
	regmap_write(rt5514->regmap, 0x18002194, 0x0002042f);  //(ad0) source of DMIC
	regmap_write(rt5514->regmap, 0x18002198, 0x10000362);  //(ad0) DMIC-IN1 L/R select
	regmap_write(rt5514->regmap, 0x180021a0, 0x0002042f);  //(ad1) source of DMIC
	regmap_write(rt5514->regmap, 0x180021a4, 0x0002042f);  //(ad1) source of DMIC
	regmap_write(rt5514->regmap, 0x180021a8, 0x10000362);  //(ad1) DMIC-IN2 L/R select
	regmap_write(rt5514->regmap, 0x180020d0, 0x00008a2f);  //(ad2) gain=24dB for WOV
#endif
	regmap_write(rt5514->regmap, 0x18001114, 0x00000001);  //dsp clk auto switch /* Fix SPI recording */
	regmap_write(rt5514->regmap, 0x18001118, 0x00000000);  //disable clk_bus_autogat_func
	regmap_write(rt5514->regmap, 0x18001104, 0x00000003);  //UART clk=off
	regmap_write(rt5514->regmap, 0x1800201c, 0x69f32067);  //(pitch VAD)fix1
	regmap_write(rt5514->regmap, 0x18002020, 0x50d500a5);  //(pitch VAD)fix2
	regmap_write(rt5514->regmap, 0x18002024, 0x000a0206);  //(pitch VAD)fix3
	regmap_write(rt5514->regmap, 0x18002028, 0x01800114);  //(pitch VAD)fix4
	regmap_write(rt5514->regmap, 0x18002038, 0x00100010);  //(hello VAD)fix1
	regmap_write(rt5514->regmap, 0x1800204c, 0x000503c8);  //(ok VAD)fix1
	regmap_write(rt5514->regmap, 0x18002050, 0x001a0308);  //(ok VAD)fix2
	regmap_write(rt5514->regmap, 0x18002054, 0x50020502);  //(ok VAD)fix3
	regmap_write(rt5514->regmap, 0x18002058, 0x50000d18);  //(ok VAD)fix4
	regmap_write(rt5514->regmap, 0x1800205c, 0x640c0b14);  //(ok VAD)fix5
	regmap_write(rt5514->regmap, 0x18002060, 0x00100001);  //(ok VAD)fix6
	regmap_write(rt5514->regmap, 0x18002fa4, 0x00000000);  //(FW) SENSORY_SVSCORE(for UDT+SID)
	regmap_write(rt5514->regmap, 0x18002fa8, 0x00000000);  //(FW) SENSORY_THRS(for UDT+SID)
	regmap_write(rt5514->regmap, 0x18002fbc, 0x00000000);  //(FW) DLY_BUF_LTC_OFFSET (for ok/hello VAD)
}

static void rt5514_enable_dsp(struct rt5514_priv *rt5514)
{
	// the gain setting is 15dB(HW analog) + 15dB(HW digital) + 6dB(SW)
	regmap_write(rt5514->regmap, 0x18001028, 0x0001000a);  //(FW) DRIVER_CTRL0: (1)VAD timeout[7:0], /0.1s; (2) buffer mode[15:14]:0=key phrase+voice command, 1: voice command, 2: key phrase
#ifdef RT5514_USE_AMIC
	regmap_write(rt5514->regmap, 0x18002004, 0x00800bbf);  //PU PLL3 and RCOSC, LDO21
	regmap_write(rt5514->regmap, 0x18002008, 0x00223700);  //PU ADC2
	regmap_write(rt5514->regmap, 0x1800222c, 0x00008c50);  //(ADC2) BG VREF=self-bias
#else
	regmap_write(rt5514->regmap, 0x18002004, 0x00808b81);  //PU RCOSC, pll3
	regmap_write(rt5514->regmap, 0x18002008, 0x00220000);  //PD ADC1/2
#endif
	regmap_write(rt5514->regmap, 0x18002f00, 0x00055149);  //dsp stop
#ifdef RT5514_USE_AMIC
	/* 33.8MHz start */
	regmap_write(rt5514->regmap, 0x18002104, 0x44025751);  //CLK, 256fs=/6,
	/* 33.8MHz end */
	regmap_write(rt5514->regmap, 0x180020a4, 0x00804002);  //(PATH) ADC2->ad2 ,ad2(db_PCM)->IB2
	regmap_write(rt5514->regmap, 0x18002228, 0x0000014F);  //(PATH)AMIC-R->ADC2 (0x143 for AMIC-L, 0x153 for AMIC-R)
	regmap_write(rt5514->regmap, 0x18002210, 0x80000000);  //(ADC2) reset ADC2
	regmap_write(rt5514->regmap, 0x18002210, 0x00000000);  //(ADC2) reset ADC2
#else
	regmap_write(rt5514->regmap, 0x18002104, 0x14023541);  //CLK, 256fs=/4,DMIC_CLK_OUT=/16(enable ad2)
	regmap_write(rt5514->regmap, 0x180020a4, 0x00808002);  //(PATH) DMIC_IN1(ri)->ad2, ad2(db_PCM)->IB2
#endif
	regmap_write(rt5514->regmap, 0x18002f08, 0x00000005);  //DSP clk source=pll3, ENABLE DSP clk
	regmap_write(rt5514->regmap, 0x18002030, 0x800007d3);  //(opt)VAD, clr and enable pitch/hello VAD (0x800007d2 for ok VAD, 0x800007d3 for pitch/hello VAD)
	regmap_write(rt5514->regmap, 0x18002064, 0x80000002);  //(opt)VAD, clr and enable ok VAD (0x80000003 for ok VAD, 0x80000002 for pitch/hello VAD)
	regmap_write(rt5514->regmap, 0x1800206c, 0x80000080);  //(opt)VAD type sel(..90h:hello, ..80h:pitch, ..A0h:ok)
	regmap_write(rt5514->regmap, 0x18002f00, 0x00055148);  //dsp run
	regmap_write(rt5514->regmap, 0x18002e04, 0x00000000);  //clear IRQ
	regmap_write(rt5514->regmap, 0x18002030, 0x000007d3);  //(opt)VAD, release and enable pitch/hello VAD (0x7d2 for ok VAD, 0x7d3 for pitch/hello VAD)
	regmap_write(rt5514->regmap, 0x18002064, 0x00000002);  //(opt)VAD, release and enable ok VAD (0x3 for ok VAD, 0x2 for pitch/hello VAD)
}

static void rt5514_reset(struct rt5514_priv *rt5514)
{
	regmap_write(rt5514->regmap, 0x18002000, 0x000010ec);  //regtop reset
	regmap_write(rt5514->regmap, 0xfafafafa, 0x00000001);  //i2c bypass=1
	regmap_write(rt5514->regmap, 0x18002004, 0x00808F81);
	regmap_write(rt5514->regmap, 0x18002008, 0x00270000);
	regmap_write(rt5514->regmap, 0x18002f08, 0x00000006);  //dsp clk source=i2cosc
	regmap_write(rt5514->regmap, 0x18002f10, 0x00000000);  //dsp TOP reset
	regmap_write(rt5514->regmap, 0x18002f10, 0x00000001);  //dsp TOP reset
	regmap_write(rt5514->regmap, 0xfafafafa, 0x00000000);  //i2c bypass=0
	regmap_write(rt5514->regmap, 0x18002000, 0x000010ec);  //regtop reset
	regmap_write(rt5514->regmap, 0x18001104, 0x00000007);
	regmap_write(rt5514->regmap, 0x18001108, 0x00000000);
	regmap_write(rt5514->regmap, 0x1800110c, 0x00000000);
	regmap_write(rt5514->regmap, 0x180014c0, 0x00000001);
	regmap_write(rt5514->regmap, 0x180014c0, 0x00000000);
	regmap_write(rt5514->regmap, 0x180014c0, 0x00000001);
}

static bool rt5514_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT5514_VENDOR_ID1:
	case RT5514_VENDOR_ID2:
	case RT5514_BUFFER_VOICE_WP:
	case RT5514_DSP_WDG_1:
	case RT5514_DSP_WDG_2:
	case RT5514_DSP_WDG_3:
	case RT5514_DSP_CLK_2:
	case RT5514_DSP_CLK_6:
	case RT5514_DSP_CLK_7:
		return true;

	default:
		return false;
	}
}

static bool rt5514_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT5514_BUFFER_VOICE_BASE:
	case RT5514_BUFFER_VOICE_LIMIT:
	case RT5514_BUFFER_VOICE_RP:
	case RT5514_BUFFER_VOICE_WP:
	case RT5514_DSP_WDG_1:
	case RT5514_DSP_WDG_2:
	case RT5514_DSP_WDG_3:
	case RT5514_DSP_CLK_2:
	case RT5514_DSP_CLK_6:
	case RT5514_DSP_CLK_7:
	case RT5514_DRIVER_CTRL0:
	case RT5514_RESET:
	case RT5514_PWR_ANA1:
	case RT5514_PWR_ANA2:
	case RT5514_I2S_CTRL1:
	case RT5514_I2S_CTRL2:
	case RT5514_VAD_CTRL6:
	case RT5514_EXT_VAD_CTRL:
	case RT5514_DIG_IO_CTRL:
	case RT5514_PAD_CTRL1:
	case RT5514_DMIC_DATA_CTRL:
	case RT5514_DIG_SOURCE_CTRL:
	case RT5514_SRC_CTRL:
	case RT5514_DOWNFILTER2_CTRL1:
	case RT5514_PLL_SOURCE_CTRL:
	case RT5514_CLK_CTRL1:
	case RT5514_CLK_CTRL2:
	case RT5514_PLL3_CALIB_CTRL1:
	case RT5514_PLL3_CALIB_CTRL5:
	case RT5514_DELAY_BUF_CTRL1:
	case RT5514_DELAY_BUF_CTRL3:
	case RT5514_DOWNFILTER0_CTRL1:
	case RT5514_DOWNFILTER0_CTRL2:
	case RT5514_DOWNFILTER0_CTRL3:
	case RT5514_DOWNFILTER1_CTRL1:
	case RT5514_DOWNFILTER1_CTRL2:
	case RT5514_DOWNFILTER1_CTRL3:
	case RT5514_ANA_CTRL_LDO10:
	case RT5514_ANA_CTRL_LDO18_16:
	case RT5514_ANA_CTRL_ADC12:
	case RT5514_ANA_CTRL_ADC21:
	case RT5514_ANA_CTRL_ADC22:
	case RT5514_ANA_CTRL_ADC23:
	case RT5514_ANA_CTRL_MICBST:
	case RT5514_ANA_CTRL_ADCFED:
	case RT5514_ANA_CTRL_INBUF:
	case RT5514_ANA_CTRL_VREF:
	case RT5514_ANA_CTRL_PLL3:
	case RT5514_ANA_CTRL_PLL1_1:
	case RT5514_ANA_CTRL_PLL1_2:
	case RT5514_DMIC_LP_CTRL:
	case RT5514_MISC_CTRL_DSP:
	case RT5514_DSP_CTRL1:
	case RT5514_DSP_CTRL3:
	case RT5514_DSP_CTRL4:
	case RT5514_VENDOR_ID1:
	case RT5514_VENDOR_ID2:
		return true;

	default:
		return false;
	}
}

/* {-3, 0, +3, +4.5, +7.5, +9.5, +12, +14, +17} dB */
static unsigned int bst_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 2, TLV_DB_SCALE_ITEM(-300, 300, 0),
	3, 3, TLV_DB_SCALE_ITEM(450, 0, 0),
	4, 4, TLV_DB_SCALE_ITEM(750, 0, 0),
	5, 5, TLV_DB_SCALE_ITEM(950, 0, 0),
	6, 6, TLV_DB_SCALE_ITEM(1200, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(1400, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(1700, 0, 0),
};

static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);

static const char *rt5514_dsp_mode[] = {
	"Stop", "Mode1", "Mode2", "Mode3", "Mode4", "Mode5", "Mode6"
};

static const SOC_ENUM_SINGLE_DECL(rt5514_dsp_mod_enum, 0, 0,
	rt5514_dsp_mode);

static int rt5514_dsp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->dsp_enabled;

	return 0;
}

static unsigned int rt5514_4byte_le_to_uint(const u8 *data)
{
	return data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
}

void rt5514_parse_header(struct snd_soc_codec *codec, const u8 *buf)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	const struct firmware *fw = NULL;
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
	u8 *cmp_buf = NULL;
#endif
	SMicFWHeader sMicFWHeader;
	SMicFWSubHeader sMicFWSubHeader;

	int i, offset = 0;
	char file_path[32];

	sMicFWHeader.Sync = rt5514_4byte_le_to_uint(buf);
	dev_dbg(codec->dev, "sMicFWHeader.Sync = %08x\n", sMicFWHeader.Sync);

	offset += 4;
	sMicFWHeader.Version =  rt5514_4byte_le_to_uint(buf + offset);
	dev_dbg(codec->dev, "sMicFWHeader.Version = %08x\n",
		sMicFWHeader.Version);
	sprintf(fw_ver, "%d", sMicFWHeader.Version);

	offset += 4;
	sMicFWHeader.NumBin = rt5514_4byte_le_to_uint(buf + offset);
	dev_dbg(codec->dev, "sMicFWHeader.NumBin = %08x\n",
		sMicFWHeader.NumBin);

	sMicFWHeader.BinArray =
		kzalloc(sizeof(SMicBinInfo) * sMicFWHeader.NumBin, GFP_KERNEL);

	offset += 4 * 9;

	for (i = 0 ; i < sMicFWHeader.NumBin; i++) {
		offset += 4;
		sMicFWHeader.BinArray[i].Offset =
			rt5514_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Offset = %08x\n",
			i, sMicFWHeader.BinArray[i].Offset);

		offset += 4;
		sMicFWHeader.BinArray[i].Size =
			rt5514_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Size = %08x\n",
			i, sMicFWHeader.BinArray[i].Size);

		offset += 4;
		sMicFWHeader.BinArray[i].Addr =
			rt5514_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x\n",
			i, sMicFWHeader.BinArray[i].Addr);

		rt5514_spi_burst_write(sMicFWHeader.BinArray[i].Addr,
			buf + sMicFWHeader.BinArray[i].Offset,
			((sMicFWHeader.BinArray[i].Size/8)+1)*8);

#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
		if (rt5514->dsp_fw_check) {
			cmp_buf = kmalloc(((sMicFWHeader.BinArray[i].Size/8)+1)*8, GFP_KERNEL);
			if (cmp_buf == NULL) {
				dev_err(codec->dev, "Failed to allocate kernel memory\n");
				goto exit_BinArray;
			}
			rt5514_spi_burst_read(sMicFWHeader.BinArray[i].Addr,
				cmp_buf, ((sMicFWHeader.BinArray[i].Size/8)+1)*8);
			if (memcmp(buf + sMicFWHeader.BinArray[i].Offset, cmp_buf,
				sMicFWHeader.BinArray[i].Size)) {
				rt5514->dsp_fw_check = 3;
				dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x firmware check failed\n",
					i, sMicFWHeader.BinArray[i].Addr);
				kfree(cmp_buf);
				goto exit_BinArray;
			} else {
				rt5514->dsp_fw_check = 2;
				dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x firmware check successful\n",
					i, sMicFWHeader.BinArray[i].Addr);
				kfree(cmp_buf);
			}
		}
#endif
	}

	offset += 4;
	sMicFWSubHeader.NumTD = rt5514_4byte_le_to_uint(buf + offset);
	dev_dbg(codec->dev, "sMicFWSubHeader.NumTD = %08x\n",
		sMicFWSubHeader.NumTD);

	sMicFWSubHeader.TDArray = 
		kzalloc(sizeof(SMicTDInfo) * sMicFWSubHeader.NumTD, GFP_KERNEL);

	for (i = 0 ; i < sMicFWSubHeader.NumTD; i++) {
		offset += 4;
		sMicFWSubHeader.TDArray[i].ID =
			rt5514_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWSubHeader.TDArray[%d].ID = %08x\n",
			i, sMicFWSubHeader.TDArray[i].ID);

		offset += 4;
		sMicFWSubHeader.TDArray[i].Addr =
			rt5514_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWSubHeader.TDArray[%d].Addr = %08x\n",
			i, sMicFWSubHeader.TDArray[i].Addr);

		snprintf(file_path, sizeof(file_path),"SMicTD%u.dat", rt5514->dsp_enabled);
		request_firmware(&fw, file_path, /*codec->dev*/NULL);
		if (fw) {
			rt5514_spi_burst_write(sMicFWSubHeader.TDArray[i].Addr,
				fw->data, ((fw->size/8)+1)*8);
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
			if (rt5514->dsp_fw_check) {
				cmp_buf = kmalloc((((fw->size/8)+1)*8), GFP_KERNEL);
				if (cmp_buf == NULL) {
					dev_err(codec->dev, "Failed to allocate kernel memory\n");
					release_firmware(fw);
					fw = NULL;
					goto exit_TDArray;
				}
				rt5514_spi_burst_read(sMicFWSubHeader.TDArray[i].Addr,
					cmp_buf, ((fw->size/8)+1)*8);
				if (memcmp(fw->data, cmp_buf, fw->size)) {
					rt5514->dsp_fw_check = 3;
					dev_dbg(codec->dev, "sMicFWSubHeader.TDArray[%d].Addr = %08x firmware check failed\n",
						i, sMicFWSubHeader.TDArray[i].Addr);
					kfree(cmp_buf);
					release_firmware(fw);
					fw = NULL;
					goto exit_TDArray;
				} else {
					rt5514->dsp_fw_check = 2;
					dev_dbg(codec->dev, "sMicFWSubHeader.TDArray[%d].Addr = %08x firmware check successful\n",
						i, sMicFWSubHeader.TDArray[i].Addr);
					kfree(cmp_buf);
				}
			}
#endif
			release_firmware(fw);
			fw = NULL;
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
		} else {
			if (rt5514->dsp_fw_check) {
				rt5514->dsp_fw_check = 4;
				printk("%s(%d)Firmware not found\n",__func__,__LINE__);
			}
#endif
		}
	}

exit_TDArray:
	if (sMicFWSubHeader.TDArray)
		kfree(sMicFWSubHeader.TDArray);

exit_BinArray:
	if (sMicFWHeader.BinArray)
		kfree(sMicFWHeader.BinArray);
}

static int rt5514_dsp_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&rt5514->dspcontrol_lock);
	int  PrevIdleMode = rt5514->dsp_idle;
	pr_info("%s record idle mode [%d] \n", __func__, PrevIdleMode);
	rt5514_set_dsp_mode(codec, ucontrol->value.integer.value[0]);
	rt5514_dsp_set_idle_mode(rt5514->codec, PrevIdleMode);
	mutex_unlock(&rt5514->dspcontrol_lock);
	return 0;
}

static int rt5514_set_dsp_mode(struct snd_soc_codec *codec, int DSPMode)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	const struct firmware *fw = NULL;
	bool restart = false;

	if  (rt5514->dsp_enabled == DSPMode)
		return 0;

	if ((DSPMode < 0) || (DSPMode >=  sizeof (rt5514_dsp_mode) / sizeof(rt5514_dsp_mode[0])) )
	{
		return -EINVAL;
	}
	if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
	{
		if (rt5514->dsp_enabled && DSPMode)
			restart = true;

		rt5514->dsp_enabled = DSPMode;

		if (rt5514->dsp_enabled)
		{
			if (restart)
			{
				set_pcm_is_readable(0);
				regcache_cache_bypass(rt5514->regmap, true);
				rt5514_reset(rt5514);
				regcache_cache_bypass(rt5514->regmap, false);
			}
			regcache_cache_bypass(rt5514->regmap, true);
			rt5514_enable_dsp_clock(rt5514);
			request_firmware(&fw, "SMicBin.dat", /*codec->dev*/NULL);
			if (fw)
			{
				rt5514_parse_header(codec, fw->data);
				release_firmware(fw);
				fw = NULL;
			}

			else
			{
				printk("%s(%d)Firmware not found\n",__func__,__LINE__);
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
				if (rt5514->dsp_fw_check)
				{
					rt5514->dsp_fw_check = 4;
				}
#endif
				return -EINVAL;
			}

			rt5514_enable_dsp(rt5514);
			regcache_cache_bypass(rt5514->regmap, false);
			msleep (150);
			// set idle state to off
            pr_info("%s  turn off idle mode in reset 1 \n", __func__);
			rt5514_dsp_set_idle_mode(rt5514->codec, 0);
			set_pcm_is_readable(1);
		}
		else
		{
			set_pcm_is_readable(0);
			regcache_cache_bypass(rt5514->regmap, true);
			rt5514_reset(rt5514);
			regcache_cache_bypass(rt5514->regmap, false);
			regcache_mark_dirty(rt5514->regmap);
			regcache_sync(rt5514->regmap);
			// set idle state to on
			rt5514_dsp_set_idle_mode(rt5514->codec, 1);
		}
	}

	return 0;
}

#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
static int rt5514_dsp_fw_check_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->dsp_fw_check;

	return 0;
}

static int rt5514_dsp_fw_check_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	rt5514->dsp_fw_check = ucontrol->value.integer.value[0];

	return 0;
}

static int rt5514_sw_reset_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->sw_reset;

	return 0;
}

static int rt5514_sw_reset_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	rt5514->sw_reset = ucontrol->value.integer.value[0];
	if (rt5514->sw_reset){
		printk("%s(%d)sw reset\n",__func__,__LINE__);
		rt5514_reset(rt5514);
	}
	printk("%s(%d)sw reset:%d\n",__func__,__LINE__,rt5514->sw_reset);
	return 0;
}

static int rt5514_dsp_stop_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->dsp_stop;

	return 0;
}

static int rt5514_dsp_stop_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;

	rt5514->dsp_stop = ucontrol->value.integer.value[0];
	printk("%s(%d)dsp stall:%d\n",__func__,__LINE__,rt5514->dsp_stop);
	if (rt5514->dsp_stop){
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x1 << 0));
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x0 << 0));
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x1 << 0));
		regmap_read(rt5514->regmap, RT5514_DSP_CTRL1, &val);
		printk("%s(%d)stall, 0x18002f00:0x%x\n",__func__,__LINE__,val);
	} else {
		printk("%s(%d)dsp run\n",__func__,__LINE__);
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x0 << 0));
	}
	return 0;
}

static int rt5514_dsp_core_reset_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->dsp_core_reset;
	return 0;
}

static int rt5514_dsp_core_reset_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;
	mutex_lock(&rt5514->dspcontrol_lock);
	rt5514->dsp_core_reset = ucontrol->value.integer.value[0];
	printk("%s(%d)dsp core reset:%d\n",__func__,__LINE__,rt5514->dsp_core_reset);
	if (rt5514->dsp_core_reset){
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 1), (0x1 << 1));
		regmap_read(rt5514->regmap, RT5514_DSP_CTRL1, &val);
		printk("%s(%d)core reset, 0x18002f00:0x%x\n",__func__,__LINE__,val);
	} else {
		printk("%s(%d)dsp run\n",__func__,__LINE__);
		regmap_update_bits(rt5514->regmap, RT5514_DSP_CTRL1,
			(0x1 << 1), (0x0 << 1));
	}
	mutex_unlock(&rt5514->dspcontrol_lock);
	return 0;
}
#endif
static int rt5514_dsp_idle_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->dsp_idle;

	return 0;
}

static int rt5514_dsp_set_idle_mode(struct snd_soc_codec *codec, int IdleMode)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	if ( rt5514->dsp_idle ==  IdleMode)
	{
		printk("%s IdleMode is the same with previous one%d\n",__func__,IdleMode);
		return 0;
	}

	if (IdleMode)
	{
		set_pcm_is_readable(0);
		rt5514_SetRP_onIdle();
		regmap_write(rt5514->regmap, RT5514_DELAY_BUF_CTRL1, 0x3fff00ee);
		regmap_write(rt5514->regmap, RT5514_DSP_WDG_3, 0x00000001);
		regmap_write(rt5514->regmap, RT5514_DSP_WDG_1, 0x00000004);
		regmap_write(rt5514->regmap, RT5514_DSP_CTRL1, 0x00055149);
		printk("%s(%d)dsp idle on\n",__func__,__LINE__);
	}
	else if (rt5514->dsp_enabled)
	{
		regmap_write(rt5514->regmap, RT5514_DSP_CTRL1, 0x00055148);
		regmap_write(rt5514->regmap, RT5514_DSP_WDG_1, 0x00000005);
		regmap_write(rt5514->regmap, RT5514_DSP_WDG_3, 0x00000001);
		regmap_write(rt5514->regmap, RT5514_DELAY_BUF_CTRL1, 0x3fff00fe);
		printk("%s(%d)dsp idle off\n",__func__,__LINE__);
		set_pcm_is_readable(1);
	}
	else
	{
		printk("%s IdleMode cannot be turn-off because DSP is not enabled  %d\n",__func__,rt5514->dsp_enabled);
	}
	rt5514->dsp_idle = IdleMode;
	return 0;
}

static int rt5514_dsp_idle_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&rt5514->dspcontrol_lock);
	rt5514_dsp_set_idle_mode(codec, ucontrol->value.integer.value[0]);
	printk("%s(%d)dsp idle:%d\n",__func__,__LINE__,ucontrol->value.integer.value[0]);
	mutex_unlock(&rt5514->dspcontrol_lock);
	return 0;
}

static int rt5514_mixer_speech_energy(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->irq_speech_energy;
	return 0;
}

static int rt5514_mixer_noise_energy(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5514->irq_ambient_energy;
	return 0;
}

static const struct snd_kcontrol_new rt5514_snd_controls[] = {
	SOC_DOUBLE_TLV("MIC Boost Volume", RT5514_ANA_CTRL_MICBST,
		RT5514_SEL_BSTL_SFT, RT5514_SEL_BSTR_SFT, 8, 0, bst_tlv),
	SOC_DOUBLE_R_TLV("ADC1 Capture Volume", RT5514_DOWNFILTER0_CTRL1,
		RT5514_DOWNFILTER0_CTRL2, RT5514_AD_GAIN_SFT, 127, 0,
		adc_vol_tlv),
	SOC_DOUBLE_R_TLV("ADC2 Capture Volume", RT5514_DOWNFILTER1_CTRL1,
		RT5514_DOWNFILTER1_CTRL2, RT5514_AD_GAIN_SFT, 127, 0,
		adc_vol_tlv),
	SOC_ENUM_EXT("DSP Control", rt5514_dsp_mod_enum, rt5514_dsp_mode_get,
		rt5514_dsp_mode_put),
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
	SOC_SINGLE_EXT("DSP FW Check", SND_SOC_NOPM, 0, 4, 0,
		rt5514_dsp_fw_check_get, rt5514_dsp_fw_check_put),
	SOC_SINGLE_EXT("SW Reset", SND_SOC_NOPM, 0, 1, 0,
		rt5514_sw_reset_get, rt5514_sw_reset_put),
	SOC_SINGLE_EXT("DSP Stop", SND_SOC_NOPM, 0, 1, 0,
		rt5514_dsp_stop_get, rt5514_dsp_stop_put),
	SOC_SINGLE_EXT("DSP Core Reset", SND_SOC_NOPM, 0, 1, 0,
		rt5514_dsp_core_reset_get, rt5514_dsp_core_reset_put),
#endif
	SOC_SINGLE_EXT("DSP Idle", SND_SOC_NOPM, 0, 1, 0,
		rt5514_dsp_idle_get, rt5514_dsp_idle_put),
	SOC_SINGLE_EXT("DSP SpeechEnergy", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0,
		rt5514_mixer_speech_energy, NULL),
	SOC_SINGLE_EXT("DSP NoiseEnergy", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0,
		rt5514_mixer_noise_energy, NULL),
};

/* ADC Mixer*/
static const struct snd_kcontrol_new rt5514_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("DMIC Switch", RT5514_DOWNFILTER0_CTRL1,
		RT5514_AD_DMIC_MIX_BIT, 1, 1),
	SOC_DAPM_SINGLE("ADC Switch", RT5514_DOWNFILTER0_CTRL1,
		RT5514_AD_AD_MIX_BIT, 1, 1),
};

static const struct snd_kcontrol_new rt5514_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("DMIC Switch", RT5514_DOWNFILTER0_CTRL2,
		RT5514_AD_DMIC_MIX_BIT, 1, 1),
	SOC_DAPM_SINGLE("ADC Switch", RT5514_DOWNFILTER0_CTRL2,
		RT5514_AD_AD_MIX_BIT, 1, 1),
};

static const struct snd_kcontrol_new rt5514_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("DMIC Switch", RT5514_DOWNFILTER1_CTRL1,
		RT5514_AD_DMIC_MIX_BIT, 1, 1),
	SOC_DAPM_SINGLE("ADC Switch", RT5514_DOWNFILTER1_CTRL1,
		RT5514_AD_AD_MIX_BIT, 1, 1),
};

static const struct snd_kcontrol_new rt5514_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("DMIC Switch", RT5514_DOWNFILTER1_CTRL2,
		RT5514_AD_DMIC_MIX_BIT, 1, 1),
	SOC_DAPM_SINGLE("ADC Switch", RT5514_DOWNFILTER1_CTRL2,
		RT5514_AD_AD_MIX_BIT, 1, 1),
};

/* DMIC Source */
static const char * const rt5514_dmic_src[] = {
	"DMIC1", "DMIC2"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5514_stereo1_dmic_enum, RT5514_DIG_SOURCE_CTRL,
	RT5514_AD0_DMIC_INPUT_SEL_SFT, rt5514_dmic_src);

static const struct snd_kcontrol_new rt5514_sto1_dmic_mux =
	SOC_DAPM_ENUM("Stereo1 DMIC Source", rt5514_stereo1_dmic_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5514_stereo2_dmic_enum, RT5514_DIG_SOURCE_CTRL,
	RT5514_AD1_DMIC_INPUT_SEL_SFT, rt5514_dmic_src);

static const struct snd_kcontrol_new rt5514_sto2_dmic_mux =
	SOC_DAPM_ENUM("Stereo2 DMIC Source", rt5514_stereo2_dmic_enum);

/**
 * rt5514_calc_dmic_clk - Calculate the frequency divider parameter of dmic.
 *
 * @rate: base clock rate.
 *
 * Choose divider parameter that gives the highest possible DMIC frequency in
 * 1MHz - 3MHz range.
 */
static int rt5514_calc_dmic_clk(int rate)
{
	int div[] = {2, 3, 4, 8, 12, 16, 24, 32};
	int i;

	if (rate < 1000000 * div[0]) {
		pr_warn("Base clock rate %d is too low\n", rate);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(div); i++) {
		/* find divider that gives DMIC frequency below 3.072MHz */
		if (3072000 * div[i] >= rate)
			return i;
	}

	pr_warn("Base clock rate %d is too high\n", rate);
	return -EINVAL;
}

static int rt5514_set_dmic_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	int idx;

	idx = rt5514_calc_dmic_clk(rt5514->sysclk);
	if (idx < 0)
		dev_err(codec->dev, "Failed to set DMIC clock\n");
	else
		regmap_update_bits(rt5514->regmap, RT5514_CLK_CTRL1,
			RT5514_CLK_DMIC_OUT_SEL_MASK,
			idx << RT5514_CLK_DMIC_OUT_SEL_SFT);

	return idx;
}

static int rt5514_adc_power_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA1,
			RT5514_POW_LDO18_IN | RT5514_POW_LDO18_ADC |
			RT5514_POW_LDO21 | RT5514_POW_BG_LDO18_IN |
			RT5514_POW_BG_LDO21,
			RT5514_POW_LDO18_IN | RT5514_POW_LDO18_ADC |
			RT5514_POW_LDO21 | RT5514_POW_BG_LDO18_IN |
			RT5514_POW_BG_LDO21);
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POW_BG_MBIAS | RT5514_POW_MBIAS |
			RT5514_POW_VREF2 | RT5514_POW_VREF1,
			RT5514_POW_BG_MBIAS | RT5514_POW_MBIAS |
			RT5514_POW_VREF2 | RT5514_POW_VREF1);
		break;

	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA1,
			RT5514_POW_LDO18_IN | RT5514_POW_LDO18_ADC |
			RT5514_POW_LDO21 | RT5514_POW_BG_LDO18_IN |
			RT5514_POW_BG_LDO21, 0);
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POW_BG_MBIAS | RT5514_POW_MBIAS |
			RT5514_POW_VREF2 | RT5514_POW_VREF1, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5514_adcl_power_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POWL_LDO16 | RT5514_POW_ADC1_L |
			RT5514_POW2_BSTL | RT5514_POW_BSTL |
			RT5514_POW_ADCFEDL,
			RT5514_POWL_LDO16 | RT5514_POW_ADC1_L |
			RT5514_POW2_BSTL | RT5514_POW_BSTL |
			RT5514_POW_ADCFEDL);
		break;

	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POWL_LDO16 | RT5514_POW_ADC1_L |
			RT5514_POW2_BSTL | RT5514_POW_BSTL |
			RT5514_POW_ADCFEDL, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5514_adcr_power_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POWR_LDO16 | RT5514_POW_ADC1_R |
			RT5514_POW2_BSTR | RT5514_POW_BSTR |
			RT5514_POW_ADCFEDR,
			RT5514_POWR_LDO16 | RT5514_POW_ADC1_R |
			RT5514_POW2_BSTR | RT5514_POW_BSTR |
			RT5514_POW_ADCFEDR);
		break;

	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(rt5514->regmap, RT5514_PWR_ANA2,
			RT5514_POWR_LDO16 | RT5514_POW_ADC1_R |
			RT5514_POW2_BSTR | RT5514_POW_BSTR |
			RT5514_POW_ADCFEDR, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5514_adc_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(50);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5514_is_sys_clk_from_pll(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	if (rt5514->sysclk_src == RT5514_SCLK_S_PLL1)
		return 1;
	else
		return 0;
}

static const struct snd_soc_dapm_widget rt5514_dapm_widgets[] = {
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC1L"),
	SND_SOC_DAPM_INPUT("DMIC1R"),
	SND_SOC_DAPM_INPUT("DMIC2L"),
	SND_SOC_DAPM_INPUT("DMIC2R"),

	SND_SOC_DAPM_INPUT("AMICL"),
	SND_SOC_DAPM_INPUT("AMICR"),

	SND_SOC_DAPM_PGA("DMIC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DMIC2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		rt5514_set_dmic_clk, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SUPPLY("ADC CLK", RT5514_CLK_CTRL1,
		RT5514_CLK_AD_ANA1_EN_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC Power", SND_SOC_NOPM, 0, 0,
		rt5514_adc_power_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("ADCL Power", SND_SOC_NOPM, 0, 0,
		rt5514_adcl_power_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("ADCR Power", SND_SOC_NOPM, 0, 0,
		rt5514_adcr_power_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("PLL1 LDO ENABLE", RT5514_ANA_CTRL_PLL1_2,
		RT5514_EN_LDO_PLL1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL1 LDO", RT5514_PWR_ANA2,
		RT5514_POW_PLL1_LDO_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL1", RT5514_PWR_ANA2, RT5514_POW_PLL1_BIT, 0,
		NULL, 0),

	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 DMIC Mux", SND_SOC_NOPM, 0, 0,
				&rt5514_sto1_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo2 DMIC Mux", SND_SOC_NOPM, 0, 0,
				&rt5514_sto2_dmic_mux),

	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY("adc stereo1 filter", RT5514_CLK_CTRL1,
		RT5514_CLK_AD0_EN_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("adc stereo2 filter", RT5514_CLK_CTRL1,
		RT5514_CLK_AD1_EN_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("Sto1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5514_sto1_adc_l_mix, ARRAY_SIZE(rt5514_sto1_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5514_sto1_adc_r_mix, ARRAY_SIZE(rt5514_sto1_adc_r_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5514_sto2_adc_l_mix, ARRAY_SIZE(rt5514_sto2_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5514_sto2_adc_r_mix, ARRAY_SIZE(rt5514_sto2_adc_r_mix)),

	SND_SOC_DAPM_ADC_E("Stereo1 ADC MIXL", NULL, RT5514_DOWNFILTER0_CTRL1,
		RT5514_AD_AD_MUTE_BIT, 1, rt5514_adc_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC_E("Stereo1 ADC MIXR", NULL, RT5514_DOWNFILTER0_CTRL2,
		RT5514_AD_AD_MUTE_BIT, 1, rt5514_adc_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC_E("Stereo2 ADC MIXL", NULL, RT5514_DOWNFILTER1_CTRL1,
		RT5514_AD_AD_MUTE_BIT, 1, rt5514_adc_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC_E("Stereo2 ADC MIXR", NULL, RT5514_DOWNFILTER1_CTRL2,
		RT5514_AD_AD_MUTE_BIT, 1, rt5514_adc_event,
		SND_SOC_DAPM_POST_PMU),

	/* ADC PGA */
	SND_SOC_DAPM_PGA("Stereo1 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route rt5514_dapm_routes[] = {
	{ "DMIC1", NULL, "DMIC1L" },
	{ "DMIC1", NULL, "DMIC1R" },
	{ "DMIC2", NULL, "DMIC2L" },
	{ "DMIC2", NULL, "DMIC2R" },

	{ "DMIC1L", NULL, "DMIC CLK" },
	{ "DMIC1R", NULL, "DMIC CLK" },
	{ "DMIC2L", NULL, "DMIC CLK" },
	{ "DMIC2R", NULL, "DMIC CLK" },

	{ "Stereo1 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo1 DMIC Mux", "DMIC2", "DMIC2" },

	{ "Sto1 ADC MIXL", "DMIC Switch", "Stereo1 DMIC Mux" },
	{ "Sto1 ADC MIXL", "ADC Switch", "AMICL" },
	{ "Sto1 ADC MIXR", "DMIC Switch", "Stereo1 DMIC Mux" },
	{ "Sto1 ADC MIXR", "ADC Switch", "AMICR" },

	{ "AMICL", NULL, "ADC CLK" },
	{ "AMICL", NULL, "ADC Power" },
	{ "AMICL", NULL, "ADCL Power" },
	{ "AMICR", NULL, "ADC CLK" },
	{ "AMICR", NULL, "ADC Power" },
	{ "AMICR", NULL, "ADCR Power" },

	{ "PLL1 LDO", NULL, "PLL1 LDO ENABLE" },
	{ "PLL1", NULL, "PLL1 LDO" },

	{ "Stereo1 ADC MIXL", NULL, "Sto1 ADC MIXL" },
	{ "Stereo1 ADC MIXR", NULL, "Sto1 ADC MIXR" },

	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXL" },
	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXR" },
	{ "Stereo1 ADC MIX", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", rt5514_is_sys_clk_from_pll },

	{ "Stereo2 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo2 DMIC Mux", "DMIC2", "DMIC2" },

	{ "Sto2 ADC MIXL", "DMIC Switch", "Stereo2 DMIC Mux" },
	{ "Sto2 ADC MIXL", "ADC Switch", "AMICL" },
	{ "Sto2 ADC MIXR", "DMIC Switch", "Stereo2 DMIC Mux" },
	{ "Sto2 ADC MIXR", "ADC Switch", "AMICR" },

	{ "Stereo2 ADC MIXL", NULL, "Sto2 ADC MIXL" },
	{ "Stereo2 ADC MIXR", NULL, "Sto2 ADC MIXR" },

	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXL" },
	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXR" },
	{ "Stereo2 ADC MIX", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", rt5514_is_sys_clk_from_pll },

	{ "AIF1TX", NULL, "Stereo1 ADC MIX"},
	{ "AIF1TX", NULL, "Stereo2 ADC MIX"},
};

static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int get_adc_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16, 24};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 7;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt5514_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	int pre_div, pre_div_adc, bclk_ms, frame_size;
	unsigned int val_len = 0;

	rt5514->lrck = params_rate(params);
	pre_div = get_clk_info(rt5514->sysclk, rt5514->lrck);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}

	pre_div_adc = get_adc_clk_info(rt5514->sysclk, rt5514->lrck);
	if (pre_div_adc < 0) {
		dev_err(codec->dev, "Unsupported adc clock setting\n");
		return -EINVAL;
	}

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}

	bclk_ms = frame_size > 32 ? 1 : 0;
	rt5514->bclk = rt5514->lrck * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5514->bclk, rt5514->lrck);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val_len = RT5514_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val_len = RT5514_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val_len = RT5514_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(rt5514->regmap, RT5514_I2S_CTRL1, RT5514_I2S_DL_MASK,
		val_len);
	regmap_update_bits(rt5514->regmap, RT5514_CLK_CTRL2,
		RT5514_CLK_SYS_DIV_OUT_MASK | RT5514_SEL_ADC_OSR_MASK,
		pre_div << RT5514_CLK_SYS_DIV_OUT_SFT |
		pre_div << RT5514_SEL_ADC_OSR_SFT);
	regmap_update_bits(rt5514->regmap, RT5514_CLK_CTRL1,
		RT5514_CLK_AD_ANA1_SEL_MASK,
		pre_div_adc << RT5514_CLK_AD_ANA1_SEL_SFT);

	return 0;
}

static int rt5514_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;

	case SND_SOC_DAIFMT_NB_IF:
		reg_val |= RT5514_I2S_LR_INV;
		break;

	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5514_I2S_BP_INV;
		break;

	case SND_SOC_DAIFMT_IB_IF:
		reg_val |= RT5514_I2S_BP_INV | RT5514_I2S_LR_INV;
		break;

	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5514_I2S_DF_LEFT;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5514_I2S_DF_PCM_A;
		break;

	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5514_I2S_DF_PCM_B;
		break;

	default:
		return -EINVAL;
	}

	regmap_update_bits(rt5514->regmap, RT5514_I2S_CTRL1,
		RT5514_I2S_DF_MASK | RT5514_I2S_BP_MASK | RT5514_I2S_LR_MASK,
		reg_val);

	return 0;
}

static int rt5514_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	if (freq == rt5514->sysclk && clk_id == rt5514->sysclk_src)
		return 0;

	switch (clk_id) {
	case RT5514_SCLK_S_MCLK:
		reg_val |= RT5514_CLK_SYS_PRE_SEL_MCLK;
		break;

	case RT5514_SCLK_S_PLL1:
		reg_val |= RT5514_CLK_SYS_PRE_SEL_PLL;
		break;

	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}

	regmap_update_bits(rt5514->regmap, RT5514_CLK_CTRL2,
		RT5514_CLK_SYS_PRE_SEL_MASK, reg_val);

	rt5514->sysclk = freq;
	rt5514->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt5514_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K, bypass K and bypass M flag.
 *
 * Calcualte M/N/K code and bypass K/M flag to configure PLL for codec.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5514_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5514_pll_code *pll_code)
{
	int max_n = RT5514_PLL_N_MAX, max_m = RT5514_PLL_M_MAX;
	int k, n = 0, m = 0, red, n_t, m_t, pll_out, in_t;
	int out_t, red_t = abs(freq_out - freq_in);
	bool m_bypass = false, k_bypass = false;

	if (RT5514_PLL_INP_MAX < freq_in || RT5514_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5514_PLL_K_MAX)
		k = RT5514_PLL_K_MAX;
	if (k < 0) {
		k = 0;
		k_bypass = true;
	}
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k_bypass ? 1 : (k + 2));
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			m_bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			m_bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				m_bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	pr_debug("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = m_bypass;
	pll_code->k_bp = k_bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;

	return 0;
}

static int rt5514_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	struct rt5514_pll_code pll_code;
	int ret;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5514->pll_in = 0;
		rt5514->pll_out = 0;
		regmap_update_bits(rt5514->regmap, RT5514_CLK_CTRL2,
			RT5514_CLK_SYS_PRE_SEL_MASK,
			RT5514_CLK_SYS_PRE_SEL_MCLK);

		return 0;
	}

	if (source == rt5514->pll_src && freq_in == rt5514->pll_in &&
	    freq_out == rt5514->pll_out)
		return 0;

	switch (source) {
	case RT5514_PLL1_S_MCLK:
		regmap_update_bits(rt5514->regmap, RT5514_PLL_SOURCE_CTRL,
			RT5514_PLL_1_SEL_MASK, RT5514_PLL_1_SEL_MCLK);
		break;

	case RT5514_PLL1_S_BCLK:
		regmap_update_bits(rt5514->regmap, RT5514_PLL_SOURCE_CTRL,
			RT5514_PLL_1_SEL_MASK, RT5514_PLL_1_SEL_SCLK);
		break;

	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5514_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "m_bypass=%d k_bypass=%d m=%d n=%d k=%d\n",
		pll_code.m_bp, pll_code.k_bp,
		(pll_code.m_bp ? 0 : pll_code.m_code), pll_code.n_code,
		(pll_code.k_bp ? 0 : pll_code.k_code));

	regmap_write(rt5514->regmap, RT5514_ANA_CTRL_PLL1_1,
		pll_code.k_code << RT5514_PLL_K_SFT |
		pll_code.n_code << RT5514_PLL_N_SFT |
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5514_PLL_M_SFT);
	regmap_update_bits(rt5514->regmap, RT5514_ANA_CTRL_PLL1_2,
		RT5514_PLL_M_BP, pll_code.m_bp << RT5514_PLL_M_BP_SFT |
		pll_code.k_bp << RT5514_PLL_K_BP_SFT);

	rt5514->pll_in = freq_in;
	rt5514->pll_out = freq_out;
	rt5514->pll_src = source;

	return 0;
}

static int rt5514_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
			unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;

	if (rx_mask || tx_mask)
		val |= RT5514_TDM_MODE;

	if (slots == 4)
		val |= RT5514_TDMSLOT_SEL_RX_4CH | RT5514_TDMSLOT_SEL_TX_4CH;


	switch (slot_width) {
	case 20:
		val |= RT5514_CH_LEN_RX_20 | RT5514_CH_LEN_TX_20;
		break;

	case 24:
		val |= RT5514_CH_LEN_RX_24 | RT5514_CH_LEN_TX_24;
		break;

	case 32:
		val |= RT5514_CH_LEN_RX_32 | RT5514_CH_LEN_TX_32;
		break;

	case 16:
	default:
		break;
	}

	regmap_update_bits(rt5514->regmap, RT5514_I2S_CTRL1, RT5514_TDM_MODE |
		RT5514_TDMSLOT_SEL_RX_MASK | RT5514_TDMSLOT_SEL_TX_MASK |
		RT5514_CH_LEN_RX_MASK | RT5514_CH_LEN_TX_MASK, val);

	return 0;
}

static int rt5514_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			if (rt5514->dsp_enabled) {
				rt5514->dsp_enabled = 0;
				rt5514_reset(rt5514);
				regcache_mark_dirty(rt5514->regmap);
				regcache_sync(rt5514->regmap);
			}
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int rt5514_probe(struct snd_soc_codec *codec)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	pr_info("Codec driver version %s\n", VERSION);

	rt5514->codec = codec;

	//rt5514_reg_init(codec);

	rt5514_set_bias_level(codec, SND_SOC_BIAS_OFF);
	mutex_lock(&rt5514->dspcontrol_lock);
	rt5514_dsp_set_idle_mode(codec, 1);
	rt5514_pointer = rt5514;
	mutex_unlock(&rt5514->dspcontrol_lock);
	return 0;
}

static int rt5514_remove(struct snd_soc_codec *codec)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);
	rt5514_set_bias_level(codec, SND_SOC_BIAS_OFF);
	mutex_lock(&rt5514->dspcontrol_lock);
	rt5514_set_dsp_mode(codec, 0);
	rt5514_pointer = NULL;
	mutex_unlock(&rt5514->dspcontrol_lock);
	return 0;
}

static int rt5514_suspend(struct snd_soc_codec *codec)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	rt5514->had_suspend = true;
	//pr_info("%s -- OK!\n", __func__);
	return 0;
}

static int rt5514_resume(struct snd_soc_codec *codec)
{
	struct rt5514_priv *rt5514 = snd_soc_codec_get_drvdata(codec);

	rt5514->had_suspend = false;
	pr_info("%s -- OK!\n", __func__);
	return 0;
}

#define RT5514_STEREO_RATES SNDRV_PCM_RATE_8000_192000
#define RT5514_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5514_aif_dai_ops = {
	.hw_params = rt5514_hw_params,
	.set_fmt = rt5514_set_dai_fmt,
	.set_sysclk = rt5514_set_dai_sysclk,
	.set_pll = rt5514_set_dai_pll,
	.set_tdm_slot = rt5514_set_tdm_slot,
};

struct snd_soc_dai_driver rt5514_dai[] = {
	{
		.name = "rt5514-aif1",
		.id = 0,
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = RT5514_STEREO_RATES,
			.formats = RT5514_FORMATS,
		},
		.ops = &rt5514_aif_dai_ops,
	}
};

static struct snd_soc_codec_driver soc_codec_dev_rt5514 = {
	.probe = rt5514_probe,
	.remove = rt5514_remove,
	.suspend = rt5514_suspend,
	.resume = rt5514_resume,
	.set_bias_level = rt5514_set_bias_level,
	.idle_bias_off = true,
	.controls = rt5514_snd_controls,
	.num_controls = ARRAY_SIZE(rt5514_snd_controls),
	.dapm_widgets = rt5514_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5514_dapm_widgets),
	.dapm_routes = rt5514_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5514_dapm_routes),
};

static const struct regmap_config rt5514_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 2,

	.max_register = RT5514_I2C_BYPASS,
	.volatile_reg = rt5514_volatile_register,
	.readable_reg = rt5514_readable_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5514_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5514_reg),
	.use_single_rw = true,
};

static const struct i2c_device_id rt5514_i2c_id[] = {
	{ "rt5514", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5514_i2c_id);

void rt5514_get_energy_info( uint64_t *ambient_E, uint64_t *speech_E)
{
	int ret;
	uint64_t energy = 0;
	u8 *energy_buf = &energy;

	ret = rt5514_spi_burst_read(RT5514_INT_VOICE_ENGY,energy_buf, sizeof(uint64_t));
	if (ret == true){
		*speech_E = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
			(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
			(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
			(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56;
	} else{
		pr_err("SPI read speech_E error\n");
	}
	ret = rt5514_spi_burst_read(RT5514_INT_NOISE_ENGY,energy_buf, sizeof(uint64_t));
	if (ret == true){
		*ambient_E = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
			(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
			(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
			(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56;
	} else{
		pr_err("SPI read ambient_E error\n");
	}
}

static irqreturn_t rt5514_irq_handler(int irq, void *dev_id)
{
	struct rt5514_priv *rt5514 = dev_id;

	pr_info("%s -- ALC5514 DSP IRQ Issued Successfully!\n", __func__);

	#define PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ 11
	extern int set_dynamic_boost(int duration, int prio_mode);
	set_dynamic_boost(200, PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ);

	if(rt5514->had_suspend)
	{
		reset_pcm_read_pointer();
	}
	schedule_work(&rt5514->handler_work);

	return IRQ_HANDLED;
}

static void rt5514_handler_work(struct work_struct *work)
{
	int iVdIdVal;
	struct rt5514_priv *rt5514 = container_of(work, struct rt5514_priv, handler_work);

	regmap_read(rt5514->regmap, RT5514_VENDOR_ID2, &iVdIdVal);
	pr_info("%s iVdIdVal:0x%x\n", __func__,iVdIdVal);

	if  (iVdIdVal == RT5514_DEVICE_ID)
	{
		schedule_work(&rt5514->hotword_work);
		wake_lock_timeout(&rt5514->vad_wake, msecs_to_jiffies(1500));
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel","RT5514_DSP_metrics_count","DSP_IRQ", 1, "count", NULL, VITALS_NORMAL);

		log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", "voice_dsp:def:DSP_IRQ=1;CT;1:NR");
#endif
	}
	else
	{
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel","RT5514_DSP_metrics_count","DSP_Watchdog", 1, "count", NULL, VITALS_NORMAL);

		log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", "voice_dsp:def:DSP_Watchdog=1;CT;1:NR");
#endif
		schedule_work(&rt5514->watchdog_work);
	}
}

void rt5514_reset_duetoSPI(void)
{
	pr_err("rt5514_reset_duetoSPI enter\n");
	if (!rt5514_pointer)
	{
		return;
	}
	schedule_work(&rt5514_pointer->watchdog_work);
	wake_lock_timeout(&rt5514_pointer->vad_wake, msecs_to_jiffies(3500));
	pr_err("%s -- exit\n", __func__);
}

static void rt5514_do_hotword_work(struct work_struct *work)
{
	struct rt5514_priv *rt5514 = container_of(work, struct rt5514_priv, hotword_work);
	static const char * const hot_event[] = { "ACTION=HOTWORD", NULL };

	rt5514_get_energy_info(&rt5514->irq_speech_energy, &rt5514->irq_ambient_energy);
	pr_info("%s -- send hotword uevent!\n", __func__);
	pr_info("speech_energy:0x%llx,ambient_energy:0x%llx\n",rt5514->irq_speech_energy,rt5514->irq_ambient_energy);

	kobject_uevent_env(&rt5514->codec->dev->kobj, KOBJ_CHANGE, hot_event);
}

static void rt5514_do_watchdog_work(struct work_struct *work)
{
	struct rt5514_priv *rt5514 = container_of(work, struct rt5514_priv, watchdog_work);
	static const char * const wdg_event[] = { "ACTION=WATCHDOG", NULL };
	int PrevDspMode;
	int PrevIdleMode;

	mutex_lock(&rt5514->dspcontrol_lock);
 	PrevDspMode = rt5514->dsp_enabled;
	PrevIdleMode = rt5514->dsp_idle;
        pr_info("%s watchdog DSP Idle Mode Recover [%d] \n", __func__, PrevIdleMode);

	rt5514_set_dsp_mode(rt5514->codec, 0);
	pr_info("%s watchdog cause DSP Reset\n", __func__);

	if (PrevDspMode)
	{
		pr_info("%s watchdog cause DSP Reload Firmware, Mode [%d] \n", __func__, PrevDspMode);
		rt5514_set_dsp_mode(rt5514->codec, PrevDspMode);
	}

	rt5514_dsp_set_idle_mode(rt5514->codec, PrevIdleMode);
	mutex_unlock(&rt5514->dspcontrol_lock);
	//pr_info("%s -- send watchdog uevent!\n", __func__);
	//kobject_uevent_env(&rt5514->codec->dev->kobj, KOBJ_CHANGE, wdg_event);
}

static ssize_t rt5514_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifndef RT5514_RW_BY_SPI
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
#endif
	int count = 0;
	unsigned int i, value;
	int ret;

	for (i = RT5514_BUFFER_VOICE_BASE; i <= RT5514_VENDOR_ID2; i+=4) {
		if (rt5514_readable_register(NULL, i)) {
#ifdef RT5514_RW_BY_SPI
			ret = rt5514_spi_read_addr(i, &value);
#else
			ret = regmap_read(rt5514->regmap, i, &value);
#endif
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
					value);

			if (count >= PAGE_SIZE - 1) {
				/* string cut by snprintf(), add this to prevent potential buffer overflow */
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}

	return count;
}

static ssize_t rt5514_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#ifndef RT5514_RW_BY_SPI
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
#endif
	unsigned int val = 0, addr = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	if (i == count) {
#ifdef RT5514_RW_BY_SPI
		rt5514_spi_read_addr(addr, &val);
#else
		regmap_read(rt5514->regmap, addr, &val);
#endif
		pr_info("0x%08x = 0x%08x\n", addr, val);
	} else {
#ifdef RT5514_RW_BY_SPI
		rt5514_spi_write_addr(addr, val);
#else
		regmap_write(rt5514->regmap, addr, val);
#endif
	}
	return count;
}
static DEVICE_ATTR(rt5514_reg, 0644, rt5514_reg_show, rt5514_reg_store);

static ssize_t rt5514_i2c_id_show(struct device *dev, 
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int value = 0, ret;
	int i,count = 0;

	for (i = (RT5514_VENDOR_ID1);
		i <= (RT5514_VENDOR_ID2); i+=4) {

		ret = regmap_read(rt5514->regmap, i, &value);
		if (ret < 0)
			count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
				"%08x: XXXXXXXX\n", i);
		else
			count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
				value);
		if (count >= PAGE_SIZE - 1) {
			count = PAGE_SIZE - 1;
			break;
		}
	}

	return count;
}
static DEVICE_ATTR(rt5514_id, 0644, rt5514_i2c_id_show, NULL);

static ssize_t rt5514_i2c_version_show(struct device *dev, 
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Firmware version: %s\nDriver version: %s\n",
		fw_ver, VERSION);
}
static DEVICE_ATTR(rt5514_version, 0644, rt5514_i2c_version_show, NULL);

static ssize_t rt5514_i2c_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	int count = 0;
	unsigned int i, value;
	int ret;

	regcache_cache_bypass(rt5514->regmap, true);
	for (i = RT5514_BUFFER_VOICE_WP; i <= RT5514_VENDOR_ID2; i+=4) {
		if (rt5514_readable_register(NULL, i)) {

			ret = regmap_read(rt5514->regmap, i, &value);

			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
					value);

			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	regcache_cache_bypass(rt5514->regmap, false);
	return count;
}

static ssize_t rt5514_i2c_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int val = 0, addr = 0;
	int i;

	regcache_cache_bypass(rt5514->regmap, true);
	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	if (i == count) {
		regmap_read(rt5514->regmap, addr, &val);
		pr_info("0x%08x = 0x%08x\n", addr, val);
	} else {
		regmap_write(rt5514->regmap, addr, val);
	}
	regcache_cache_bypass(rt5514->regmap, false);
	return count;
}
static DEVICE_ATTR(rt5514_reg_i2c, 0644, rt5514_i2c_reg_show, rt5514_i2c_reg_store);
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
static ssize_t rt5514_sensory_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int addr;
	unsigned int frame_cnt, energy_lvl, ww_trig,lpsd_sta;
	int i, total_cnt,rotate_cnt1, rotate_cnt2, count=0;
	int ret,ret1, ret2, ret3, ret4;

	addr = RT5514_DBG_RING1_ADDR;
	rt5514_spi_read_addr(addr, &rt5514->sen_base);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen_limit);
	addr += 8;
	rt5514_spi_read_addr(addr, &rt5514->sen_rp);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen_wp);

	if ( rt5514->sen_wp > rt5514->sen_rp )
	{
		total_cnt = (rt5514->sen_wp - rt5514->sen_rp)/16;
		for(i=0; i<total_cnt; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->sen_rp, &frame_cnt);
			ret2=rt5514_spi_read_addr(rt5514->sen_rp+4, &energy_lvl);
			ret3=rt5514_spi_read_addr(rt5514->sen_rp+8, &ww_trig);
			ret4=rt5514_spi_read_addr(rt5514->sen_rp+12, &lpsd_sta);
			rt5514->sen_rp += 16;
			ret = ret1 && ret2 && ret3 && ret4 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,"frm_cnt:erg_lvl:ww_trig:lpsd_sta = 0x%08x:0x%08x:0x%08x:0x%08x\n",
					frame_cnt,energy_lvl,ww_trig,lpsd_sta);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	else
	{
		rotate_cnt1 = (rt5514->sen_limit-rt5514->sen_rp)/16;
		rotate_cnt2 = (rt5514->sen_wp-rt5514->sen_base)/16;
		for (i=0; i<rotate_cnt1; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->sen_rp, &frame_cnt);
			ret2=rt5514_spi_read_addr(rt5514->sen_rp+4, &energy_lvl);
			ret3=rt5514_spi_read_addr(rt5514->sen_rp+8, &ww_trig);
			ret4=rt5514_spi_read_addr(rt5514->sen_rp+12, &lpsd_sta);
			rt5514->sen_rp += 16;
			ret = ret1 && ret2 && ret3 && ret4 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "frm_cnt:erg_lvl:ww_trig:lpsd_sta = 0x%08x:0x%08x:0x%08x:0x%08x\n",
					frame_cnt,energy_lvl,ww_trig,lpsd_sta);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				return count;
			}
		}
		rt5514->sen_rp = rt5514->sen_base;
		for (i=0; i<rotate_cnt2; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->sen_rp, &frame_cnt);
			ret2=rt5514_spi_read_addr(rt5514->sen_rp+4, &energy_lvl);
			ret3=rt5514_spi_read_addr(rt5514->sen_rp+8, &ww_trig);
			ret4=rt5514_spi_read_addr(rt5514->sen_rp+12, &lpsd_sta);
			rt5514->sen_rp += 16;
			ret = ret1 && ret2 && ret3 && ret4 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "frm_cnt:erg_lvl:ww_trig:lpsd_sta = 0x%08x:0x%08x:0x%08x:0x%08x\n",
					frame_cnt,energy_lvl,ww_trig,lpsd_sta);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	return count;
}
static DEVICE_ATTR(rt5514_sensory, 0644, rt5514_sensory_info_show, NULL);

static ssize_t rt5514_debug_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int addr;
	unsigned int time_stamp, func_id, except_pc;
	int i, total_cnt,rotate_cnt1,rotate_cnt2,count=0;
	int ret,ret1, ret2, ret3;

	addr = RT5514_DBG_RING2_ADDR;
	rt5514_spi_read_addr(addr, &rt5514->dbg_base);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->dbg_limit);
	addr += 8;
	rt5514_spi_read_addr(addr, &rt5514->dbg_rp);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->dbg_wp);

	if ( rt5514->dbg_wp > rt5514->dbg_rp )
	{
		total_cnt = (rt5514->dbg_wp - rt5514->dbg_rp)/12;
		for(i=0; i<total_cnt; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->dbg_rp, &time_stamp);
			ret2=rt5514_spi_read_addr(rt5514->dbg_rp+4, &func_id);
			ret3=rt5514_spi_read_addr(rt5514->dbg_rp+8, &except_pc);
			rt5514->dbg_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "time_stamp:func_id:except_pc = 0x%08x:0x%08x:0x%08x\n", time_stamp,func_id,except_pc);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	else
	{
		rotate_cnt1 = (rt5514->dbg_limit-rt5514->dbg_rp)/12;
		rotate_cnt2 = (rt5514->dbg_wp-rt5514->dbg_base)/12;
		for (i=0; i<rotate_cnt1; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->dbg_rp, &time_stamp);
			ret2=rt5514_spi_read_addr(rt5514->dbg_rp+4, &func_id);
			ret3=rt5514_spi_read_addr(rt5514->dbg_rp+8, &except_pc);
			rt5514->dbg_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "time_stamp:func_id:except_pc = 0x%08x:0x%08x:0x%08x\n",
					time_stamp,func_id,except_pc);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				return count;
			}
		}
		rt5514->dbg_rp = rt5514->dbg_base;
		for (i=0; i<rotate_cnt2; i++)
		{
			ret1=rt5514_spi_read_addr(rt5514->dbg_rp, &time_stamp);
			ret2=rt5514_spi_read_addr(rt5514->dbg_rp+4, &func_id);
			ret3=rt5514_spi_read_addr(rt5514->dbg_rp+8, &except_pc);
			rt5514->dbg_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "time_stamp:func_id:except_pc = 0x%08x:0x%08x:0x%08x\n",
					time_stamp,func_id,except_pc);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	return count;
}
static DEVICE_ATTR(rt5514_debug, 0644, rt5514_debug_info_show, NULL);

static ssize_t rt5514_start_end_score_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int addr;
	unsigned int start_ww, end_ww, score;
	int i, total_cnt,rotate_cnt1,rotate_cnt2,count=0;
	int ret,ret1, ret2, ret3;

	addr = RT5514_DBG_RING3_ADDR;
	rt5514_spi_read_addr(addr, &rt5514->sen2_base);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen2_limit);
	addr += 8;
	rt5514_spi_read_addr(addr, &rt5514->sen2_rp);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen2_wp);

	if ( rt5514->sen2_wp > rt5514->sen2_rp ){
		total_cnt = (rt5514->sen2_wp - rt5514->sen2_rp)/12;
		for(i=0; i<total_cnt; i++){
			ret1=rt5514_spi_read_addr(rt5514->sen2_rp, &start_ww);
			ret2=rt5514_spi_read_addr(rt5514->sen2_rp+4, &end_ww);
			ret3=rt5514_spi_read_addr(rt5514->sen2_rp+8, &score);
			rt5514->sen2_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "start_ww:end_ww:score = 0x%08x:0x%08x:0x%08x\n",
					start_ww,end_ww,score);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	} else {
		rotate_cnt1 = (rt5514->sen2_limit-rt5514->sen2_rp)/12;
		rotate_cnt2 = (rt5514->sen2_wp-rt5514->sen2_base)/12;
		for (i=0; i<rotate_cnt1; i++){
			ret1=rt5514_spi_read_addr(rt5514->sen2_rp, &start_ww);
			ret2=rt5514_spi_read_addr(rt5514->sen2_rp+4, &end_ww);
			ret3=rt5514_spi_read_addr(rt5514->sen2_rp+8, &score);
			rt5514->sen2_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "start_ww:end_ww:score = 0x%08x:0x%08x:0x%08x\n",
					start_ww,end_ww,score);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				return count;
			}
		}
		rt5514->sen2_rp = rt5514->sen2_base;
		for (i=0; i<rotate_cnt2; i++){
			ret1=rt5514_spi_read_addr(rt5514->sen2_rp, &start_ww);
			ret2=rt5514_spi_read_addr(rt5514->sen2_rp+4, &end_ww);
			ret3=rt5514_spi_read_addr(rt5514->sen2_rp+8, &score);
			rt5514->sen2_rp += 12;
			ret = ret1 && ret2 && ret3 ;
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "start_ww:end_ww:score = 0x%08x:0x%08x:0x%08x\n",
					start_ww,end_ww,score);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	return count;
}
static DEVICE_ATTR(rt5514_sensory_start_end_score, 0644, rt5514_start_end_score_show, NULL);

static ssize_t rt5514_sensory_energy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	unsigned int addr;
	uint64_t speech_energy, ambient_energy,frame_cnt;
	u8 *energy_buf = NULL;
	int i, total_cnt,rotate_cnt1,rotate_cnt2,count=0;
	int ret1,ret2,ret3;

	addr = RT5514_DBG_RING4_ADDR;
	rt5514_spi_read_addr(addr, &rt5514->sen3_base);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen3_limit);
	addr += 8;
	rt5514_spi_read_addr(addr, &rt5514->sen3_rp);
	addr += 4;
	rt5514_spi_read_addr(addr, &rt5514->sen3_wp);

	if ( rt5514->sen3_wp > rt5514->sen3_rp ){
		total_cnt = (rt5514->sen3_wp - rt5514->sen3_rp)/24;
		energy_buf = kmalloc(sizeof(unsigned long long),GFP_KERNEL);
		if ( NULL == energy_buf)
		{
			dev_err(rt5514->codec->dev, "Failed to allocate kernel memory\n");
			return 0;
		}
		for(i=0; i<total_cnt; i++){
			ret1 = rt5514_spi_burst_read(rt5514->sen3_rp,energy_buf,
				sizeof(unsigned long long));
			speech_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret2 = rt5514_spi_burst_read(rt5514->sen3_rp+8,energy_buf,
				sizeof(unsigned long long));
			ambient_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret3 = rt5514_spi_burst_read(rt5514->sen3_rp+16,energy_buf,
				sizeof(unsigned long long));
			frame_cnt = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			rt5514->sen3_rp += 24;
			if (ret1&&ret2&&ret3 ==  0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "speech_energy:ambient_energy:frame_cnt = 0x%llx:0x%llx:0x%llx\n",
					speech_energy,ambient_energy,frame_cnt);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
		kfree(energy_buf);
	} else {
		rotate_cnt1 = (rt5514->sen3_limit-rt5514->sen3_rp)/24;
		rotate_cnt2 = (rt5514->sen3_wp-rt5514->sen3_base)/24;
		energy_buf = kmalloc(sizeof(unsigned long long),GFP_KERNEL);
		if ( NULL == energy_buf)
		{
			dev_err(rt5514->codec->dev, "Failed to allocate kernel memory\n");
			return 0;
		}
		for (i=0; i<rotate_cnt1; i++){
			ret1 = rt5514_spi_burst_read(rt5514->sen3_rp,energy_buf,
				sizeof(unsigned long long));
			speech_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret2 = rt5514_spi_burst_read(rt5514->sen3_rp+8,energy_buf,
				sizeof(unsigned long long));
			ambient_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret3 = rt5514_spi_burst_read(rt5514->sen3_rp+16,energy_buf,
				sizeof(unsigned long long));
			frame_cnt = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			rt5514->sen3_rp += 24;
			if (ret1&&ret2&&ret3 ==  0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "speech_energy:ambient_energy:frame_cnt = 0x%llx:0x%llx:0x%llx\n",
					speech_energy,ambient_energy,frame_cnt);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				kfree(energy_buf);
				return count;
			}
		}
		rt5514->sen3_rp = rt5514->sen3_base;
		for (i=0; i<rotate_cnt2; i++){
			ret1 = rt5514_spi_burst_read(rt5514->sen3_rp,energy_buf,
				sizeof(unsigned long long));
			speech_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret2 = rt5514_spi_burst_read(rt5514->sen3_rp+8,energy_buf,
				sizeof(unsigned long long));
			ambient_energy = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			ret3 = rt5514_spi_burst_read(rt5514->sen3_rp+16,energy_buf,
				sizeof(unsigned long long));
			frame_cnt = (uint64_t)*(energy_buf) | (uint64_t)*(energy_buf+1) << 8 |
				(uint64_t)*(energy_buf+2) << 16 | (uint64_t)*(energy_buf+3) << 24 |
				(uint64_t)*(energy_buf+4) << 32 | (uint64_t)*(energy_buf+5) << 40 |
				(uint64_t)*(energy_buf+6) << 48 | (uint64_t)*(energy_buf+7) << 56 ;
			rt5514->sen3_rp += 24;
			if (ret1&&ret2&&ret3 ==  0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,"speech_energy:ambient_energy:frame_cnt = 0x%llx:0x%llx:0x%llx\n",
					speech_energy,ambient_energy,frame_cnt);
			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
		kfree(energy_buf);
	}
	return count;
}
static DEVICE_ATTR(rt5514_sensory_energy, 0644, rt5514_sensory_energy_show, NULL);
#endif
static int rt5514_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5514_priv *rt5514;
	int ret;

	rt5514 = devm_kzalloc(&i2c->dev, sizeof(struct rt5514_priv),
				GFP_KERNEL);
	if (rt5514 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5514);

	rt5514->regmap = devm_regmap_init_i2c(i2c, &rt5514_regmap);
	if (IS_ERR(rt5514->regmap)) {
		ret = PTR_ERR(rt5514->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
        rt5514_reset(rt5514);
	regmap_read(rt5514->regmap, RT5514_VENDOR_ID2, &ret);
	if (ret != RT5514_DEVICE_ID) {
		dev_err(&i2c->dev,
			"Device with ID register %x is not rt5514\n", ret);
		return -ENODEV;          //temporary solution for init fail, do not return even the ID check fails.
	}

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_reg);

	if (ret < 0)
		printk("failed to add rt5514_reg sysfs files\n");

	rt5514->had_suspend = false;
	ret = request_irq(i2c->irq, rt5514_irq_handler,
		IRQF_TRIGGER_RISING|IRQF_ONESHOT, "rt5514", rt5514);
	if (ret < 0)
		dev_err(&i2c->dev, "rt5514_irq not available (%d)\n", ret);
	ret = irq_set_irq_wake(i2c->irq, 1);
	if (ret < 0)
		dev_err(&i2c->dev, "failed to set irq wake (%d)\n", ret);

	INIT_WORK(&rt5514->hotword_work, rt5514_do_hotword_work);
	INIT_WORK(&rt5514->watchdog_work, rt5514_do_watchdog_work);
	INIT_WORK(&rt5514->handler_work, rt5514_handler_work);
	mutex_init(&rt5514->dspcontrol_lock);
	wake_lock_init(&rt5514->vad_wake, WAKE_LOCK_SUSPEND, "rt5514_wake");
	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_reg_i2c);
	if (ret < 0)
		printk("failed to add rt5514_reg_i2c sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_id);
	if (ret < 0)
		printk("failed to add rt5514_id sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_version);
	if (ret < 0)
		printk("failed to add rt5514_version sysfs files\n");
#ifdef CONFIG_SND_SOC_RT5514_TEST_ONLY
	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_sensory);
	if (ret < 0)
		printk("failed to add rt5514_sensory sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_sensory_energy);
	if (ret < 0)
		printk("failed to add rt5514_sensory_energy sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_sensory_start_end_score);
	if (ret < 0)
		printk("failed to add rt5514_sensory2 sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt5514_debug);
	if (ret < 0)
		printk("failed to add rt5514_debug sysfs files\n");
#endif
	return snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5514,
			rt5514_dai, ARRAY_SIZE(rt5514_dai));
}

static int rt5514_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);

	return 0;
}

void rt5514_i2c_shutdown(struct i2c_client *client)
{
	struct rt5514_priv *rt5514 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5514->codec;

	if (codec != NULL)
		rt5514_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt5514_i2c_driver = {
	.driver = {
		.name = "rt5514",
		.owner = THIS_MODULE,
	},
	.probe = rt5514_i2c_probe,
	.remove   = rt5514_i2c_remove,
	.shutdown = rt5514_i2c_shutdown,
	.id_table = rt5514_i2c_id,
};
module_i2c_driver(rt5514_i2c_driver);


MODULE_DESCRIPTION("ASoC ALC5514 driver");
MODULE_AUTHOR("Oder Chiou <oder_chiou@realtek.com>");
MODULE_LICENSE("GPL v2");
