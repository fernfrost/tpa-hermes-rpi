/*
 * ASoC Codec for TPA Hermes-RPi.
 *
 * Based on work from:
 *  Gael Chauffaut <gael.chauffaut@gmail.com>
 *  Florian Meier <florian.meier@koalo.de>
 *		Copyright 2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <sound/soc.h>

static struct gpio_descs *mult_gpios;
static unsigned int tpa_hermes_rpi_rate;

const unsigned int channels = 2;
const unsigned int bclk_ratio = 32 * channels;

static int snd_tpa_hermes_rpi_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	int ret = 0;

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	tpa_hermes_rpi_rate = params_rate(params);

	ret = snd_soc_dai_set_bclk_ratio(asoc_rtd_to_cpu(rtd, 0), bclk_ratio);

	return ret;
}

static const unsigned int tpa_hermes_rpi_rates[] = {
	44100,
	48000,
	88200,
	96000,
	176400,
	192000,
};

static struct snd_pcm_hw_constraint_list tpa_hermes_rpi_constraints = {
	.list = tpa_hermes_rpi_rates,
	.count = ARRAY_SIZE(tpa_hermes_rpi_rates),
};

static int snd_tpa_hermes_rpi_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	ret = snd_pcm_hw_constraint_list(substream->runtime,
									 0,
									 SNDRV_PCM_HW_PARAM_RATE,
									 &tpa_hermes_rpi_constraints);

	if (ret < 0)
	{
		return ret;
	}

	ret = snd_pcm_hw_constraint_single(substream->runtime,
									   SNDRV_PCM_HW_PARAM_CHANNELS,
									   channels);

	if (ret < 0)
	{
		return ret;
	}

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
									   SNDRV_PCM_HW_PARAM_FORMAT,
									   SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE);

	return ret;
}

static int snd_tpa_hermes_rpi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	DECLARE_BITMAP(mult, 5);

	memset(mult, 0, sizeof(mult));

	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	{
		__assign_bit(4, mult, 1);
		if ((tpa_hermes_rpi_rate % 48000) == 0)
		{
			__assign_bit(0, mult, 1);
		}

		switch (tpa_hermes_rpi_rate)
		{
		case 384000:
		case 352800:
			break;
		case 192000:
		case 176400:
			__assign_bit(1, mult, 1);
			break;
		case 96000:
		case 88200:
			__assign_bit(1, mult, 1);
			__assign_bit(2, mult, 1);
			break;
		case 48000:
		case 44100:
			__assign_bit(2, mult, 1);
			__assign_bit(3, mult, 1);
			break;
		default:
			return -EINVAL;
		}
		break;
	}
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		return -EINVAL;
	}

	gpiod_set_array_value_cansleep(mult_gpios->ndescs,
								   mult_gpios->desc,
								   mult_gpios->info,
								   mult);

	return 0;
}

static struct snd_soc_ops snd_tpa_hermes_rpi_ops = {
	.hw_params = snd_tpa_hermes_rpi_hw_params,
	.startup = snd_tpa_hermes_rpi_startup,
	.trigger = snd_tpa_hermes_rpi_trigger,
};

static int snd_tpa_hermes_rpi_init(struct snd_soc_pcm_runtime *rtd)
{
	return snd_soc_dai_set_bclk_ratio(asoc_rtd_to_cpu(rtd, 0), bclk_ratio);
}

SND_SOC_DAILINK_DEFS(tpa_hermes_rpi,
					 DAILINK_COMP_ARRAY(COMP_CPU("bcm2708-i2s.0")),
					 DAILINK_COMP_ARRAY(COMP_CODEC("tpa-hermes-rpi-codec", "tpa-hermes-rpi-dai")),
					 DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2708-i2s.0")));

static struct snd_soc_dai_link snd_tpa_hermes_rpi_dai[] = {
	{
		.name = "Twisted Pear Audio Hermes-RPI",
		.stream_name = "Twisted Pear Audio Hermes-RPI HiFi",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.ops = &snd_tpa_hermes_rpi_ops,
		.init = snd_tpa_hermes_rpi_init,
		SND_SOC_DAILINK_REG(tpa_hermes_rpi),
	},
};

static struct snd_soc_card snd_tpa_hermes_rpi = {
	.name = "snd_tpa_hermes_rpi",
	.driver_name = "TwistedPearAudioHermesRPI",
	.owner = THIS_MODULE,
	.dai_link = snd_tpa_hermes_rpi_dai,
	.num_links = ARRAY_SIZE(snd_tpa_hermes_rpi_dai),
};

static int snd_tpa_hermes_rpi_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_tpa_hermes_rpi.dev = &pdev->dev;
	if (pdev->dev.of_node)
	{
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai;

		dai = &snd_tpa_hermes_rpi_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

		if (i2s_node)
		{
			dai->cpus->dai_name = NULL;
			dai->cpus->of_node = i2s_node;
			dai->platforms->name = NULL;
			dai->platforms->of_node = i2s_node;
		}
		else
		{
			dev_err(&pdev->dev, "invalid i2s controller");
			return -EINVAL;
		}

		mult_gpios = devm_gpiod_get_array(&pdev->dev, "mult", GPIOD_OUT_LOW);
		if (IS_ERR(mult_gpios))
		{
			return PTR_ERR(mult_gpios);
		}
	}

	ret = devm_snd_soc_register_card(&pdev->dev, &snd_tpa_hermes_rpi);
	if (ret && ret != -EPROBE_DEFER)
	{
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
	}

	return ret;
}

static struct of_device_id snd_tpa_hermes_rpi_of_match[] = {
	{
		.compatible = "tpa,tpa-hermes-rpi",
	},
	{},
};

MODULE_DEVICE_TABLE(of, snd_tpa_hermes_rpi_of_match);

static struct platform_driver snd_tpa_hermes_rpi_driver = {
	.driver = {
		.name = "snd-tpa-hermes-rpi",
		.owner = THIS_MODULE,
		.of_match_table = snd_tpa_hermes_rpi_of_match,
	},
	.probe = snd_tpa_hermes_rpi_probe,
};

module_platform_driver(snd_tpa_hermes_rpi_driver);

MODULE_DESCRIPTION("ASoC Driver for Twisted Pear Audio Hermes-RPi");
MODULE_LICENSE("GPL v2");