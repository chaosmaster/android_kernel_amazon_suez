/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include "mt_afe_def.h"
#include <linux/types.h>
#include "mt_afe_control.h"
#include "mt_afe_debug.h"
#include <linux/module.h>
#include <sound/soc.h>

#ifdef CONFIG_SND_SOC_RT5514
#include "../../codecs/rt5514.h"

static int mt8173_rt5514_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5514_SCLK_S_MCLK,
				params_rate(params) * 256, SND_SOC_CLOCK_IN);
	if (ret)
		return ret;
	return 0;
}

static struct snd_soc_ops mt8173_rt5514_ops = {
	.hw_params = mt8173_rt5514_hw_params,
};

static struct snd_soc_dai_link_component mt8173_rt5514_codecs[] = {
	{
		.dai_name = "rt5514-aif1",
	},
};

#endif

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8173_dais[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-rx-dai",
	 },
	{
	 .name = "HDMI_PCM_OUTPUT",
	 .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "BTSCO",
	 .stream_name = MT_SOC_BTSCO_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 .platform_name = MT_SOC_BTSCO_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "DL1AWB_CAPTURE",
	 .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "MultiMedia2_Capture",
	 .stream_name = MT_SOC_UL2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL2_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "HDMI_RAW_OUTPUT",
	 .stream_name = MT_SOC_HDMI_RAW_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_RAW_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_RAW_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "SPDIF_OUTPUT",
	 .stream_name = MT_SOC_SPDIF_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_SPDIF_CPU_DAI_NAME,
	 .platform_name = MT_SOC_SPDIF_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	 {
	 .name = "I2S0_AWB_CAPTURE",
	 .stream_name = MT_SOC_I2S0_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_I2S0_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "MRGRX",
	 .stream_name = MT_SOC_MRGRX_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_CPU_DAI_NAME,
	 .platform_name = MT_SOC_MRGRX_PLARFORM_NAME,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
	 },
	{
	 .name = "MRGRX_CAPTURE",
	 .stream_name = MT_SOC_MRGRX_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_MRGRX_AWB_PLARFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	 {
	 .name = "MultiMedia_DL2",
	 .stream_name = MT_SOC_DL2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL2_PCM,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
	 },
	 {
	 .name = "PLATOFRM_CONTROL",
	 .stream_name = MT_SOC_ROUTING_STREAM_NAME,
	 .cpu_dai_name = "snd-soc-dummy-dai",
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .no_pcm = 1,
	 },
};
#ifdef CONFIG_SND_SOC_RT5514
static struct snd_soc_dai_link mt8173_rt5514_dsp_dais[] = {
	{
	.name = "RT5514 DSP",
	.stream_name = "DSP_Capture",
	.cpu_dai_name = "spi32766.0",
	.platform_name = "spi32766.0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	},
};

static struct snd_soc_dai_link
		mt8173_rt5514_dais[ARRAY_SIZE(mt8173_dais)
		+ ARRAY_SIZE(mt8173_rt5514_dsp_dais)];
#endif

static int mt8173_channel_cap_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8173_channel_cap_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mt_afe_get_board_channel_type();
	return 0;
}

static const char *const mt8173_channel_cap[] = {
	"Stereo", "MonoLeft", "MonoRight"
};

static const struct soc_enum mt8173_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8173_channel_cap), mt8173_channel_cap),
};

static const struct snd_kcontrol_new mt8173_controls[] = {
	SOC_ENUM_EXT("Board Channel Config", mt8173_control_enum[0],
		     mt8173_channel_cap_get,
		     mt8173_channel_cap_set),
};

static struct snd_soc_card mt8173_card = {
	.name = "mt-snd-card",
	.dai_link = mt8173_dais,
	.num_links = ARRAY_SIZE(mt8173_dais),
	.controls = mt8173_controls,
	.num_controls = ARRAY_SIZE(mt8173_controls),
};

static int mt8173_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8173_card;
	struct device *dev = &pdev->dev;
#ifdef CONFIG_SND_SOC_RT5514
	const char *rt5514_dai_name = "";
	int found = 0;
#endif
	int ret;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_MACHINE_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

#ifdef CONFIG_SND_SOC_RT5514
	ret = snd_soc_of_get_dai_name(pdev->dev.of_node, &rt5514_dai_name);
	if (ret < 0)
		pr_err("%s snd_soc_of_get_dai_name fail %d\n", __func__, ret);
	else
		found = 1;
	if (found) {
		mt8173_dais[5].codec_name = NULL;
		mt8173_dais[5].codec_dai_name = NULL;
		mt8173_rt5514_codecs[0].of_node =
			of_parse_phandle(pdev->dev.of_node, "sound-dai", 0);
		mt8173_dais[5].codecs = mt8173_rt5514_codecs;
		mt8173_dais[5].num_codecs = 1;
		mt8173_dais[5].ops = &mt8173_rt5514_ops;
		memcpy(mt8173_rt5514_dais,
				mt8173_dais, sizeof(mt8173_dais));
		memcpy((mt8173_rt5514_dais + ARRAY_SIZE(mt8173_dais)),
				mt8173_rt5514_dsp_dais,
				sizeof(mt8173_rt5514_dsp_dais));
		mt8173_card.dai_link = mt8173_rt5514_dais;
		mt8173_card.num_links =
				ARRAY_SIZE(mt8173_rt5514_dais);
	}
#endif
	card->dev = dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		return ret;
	}

	ret = mt_afe_platform_init(dev);
	if (ret) {
		pr_err("%s mt_afe_platform_init fail %d\n", __func__, ret);
		snd_soc_unregister_card(card);
		return ret;
	}

	mt_afe_debug_init();

	return 0;
}

static int mt8173_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	mt_afe_platform_deinit(&pdev->dev);

	mt_afe_debug_deinit();

	return 0;
}

static const struct of_device_id mt8173_machine_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_MACHINE_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8173_machine_dt_match);

static struct platform_driver mt8173_machine_driver = {
	.driver = {
		   .name = MT_SOC_MACHINE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_machine_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mt8173_dev_probe,
	.remove = mt8173_dev_remove,
};

module_platform_driver(mt8173_machine_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC driver for MT8173 SUEZ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
