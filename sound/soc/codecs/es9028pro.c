// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ES9028 Output ASoC codec driver
 *
 * Copyright (c) AURALiC Ltd. 2023
 *	Chang Peng <peng.chang@auralic.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
extern bool dac_set_samfreq(unsigned int samfreq);
extern void aura_dac_mute(void);
extern void aura_dac_umute(void);
#define ES9028_FORMATS (SNDRV_PCM_FMTBIT_S16_LE  |		\
			     SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE | \
                 SNDRV_PCM_FMTBIT_DSD_U32_LE | SNDRV_PCM_FMTBIT_DSD_U32_BE)

#define ES9028_RATES   (SNDRV_PCM_RATE_44100  | \
			     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200  | \
			     SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
                 SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_352800 | \
                 SNDRV_PCM_RATE_384000 | SNDRV_PCM_RATE_705600 | \
                 SNDRV_PCM_RATE_768000)



struct es9028_private {
	unsigned int format;
	/* Current rate for deemphasis control */
	unsigned int rate;
};


static int es9028_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int format)
{
    //printk(KERN_ERR "in es9028 output set dai format:%d\n",format);
    /*
	struct snd_soc_component *component = codec_dai->component;
	struct es9028_usbin_private *priv = snd_soc_component_get_drvdata(component);


	priv->format = format;
*/
	return 0;
}


static int es9028_startup(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es9028_priv *priv = snd_soc_component_get_drvdata(component);

    aura_dac_mute();

	return 0;
}
static void es9028_shutdown(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es9028_priv *priv = snd_soc_component_get_drvdata(component);

    aura_dac_mute();

}
static int es9028_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es9028_private *priv = snd_soc_component_get_drvdata(component);
	int val = 0, ret;
    unsigned int freq;
    unsigned int width;  
//    printk(KERN_ERR "sample rate is:%d===format is %d\n",params_rate(params),params_format(params));
    switch (params_format(params))
    {
        case SNDRV_PCM_FORMAT_S16_LE:
        case SNDRV_PCM_FORMAT_S24_LE:
        case SNDRV_PCM_FORMAT_S32_LE:
            width=1;
            break;
        case SNDRV_PCM_FORMAT_DSD_U32_LE:
        case SNDRV_PCM_FORMAT_DSD_U32_BE:
            width = params_physical_width(params);//32
            break;
        default: width = 1;break;
    }
    switch (params_rate(params))
    {
        case 44100:freq = 44100*width;break;
        case 48000:freq = 48000*width;break;
        case 88200:freq = 88200*width;break;
        case 96000:freq = 96000*width;break;
        case 176400:freq = 176400*width;break;
        case 192000:freq = 192000*width;break;
        case 352800:freq = 352800*width;break;
        case 384000:freq = 384000*width;break;
        case 705600:freq = 705600*width;break;
        case 768000:freq = 768000*width;break;
/*
        case 2822400:freq = 2822400;break;
        case 3072000:freq = 3072000;break;
        case 5644800:freq = 5644800;break;
        case 6144000:freq = 6144000;break;
        case 11289600:freq = 11289600;break;
        case 12288000:freq = 12288000;break;
        case 22579200:freq = 22579200;break;
        case 24576000:freq = 24576000;break;
        */
        default:freq=44100;break; 
    }
    dac_set_samfreq(freq);
//    aura_dac_umute();
	return 0;
#if 0
	priv->rate = params_rate(params);

	switch (params_rate(params)) {
	case 44100:
    case 48000:
		val = 0x20;
		break;
	case 88200:
	case 96000:
		val = 0x00;
		break;
	case 176400:
	case 192000:
		val = 0x30;
		break;
	default:
		dev_err(component->dev, "unsupported sampling rate\n");
		return -EINVAL;
	}
    if(params_rate(params) % 22050)//48x
        mclk = 1;
    else //44x
        mclk = 0;
    /*
    if(priv->mclk_gpio)
    {
    dev_err(component->dev,"set mclk gpio to :%d\n",mclk);
    gpiod_set_value(priv->mclk_gpio, mclk);
    usleep_range(100000,110000);
    }
    */
	gpio_set_value(select_44_48, mclk);	
	
    ret = regmap_write(priv->regmap, 0x04, 0x00);//stop es9028_usbin first
    
	ret = regmap_write(priv->regmap, 0x04, val);
    usleep_range(100000,110000);
	ret = regmap_write(priv->regmap, 0x04, val|0x40);
    usleep_range(100,110);
	ret = regmap_write(priv->regmap, 0x05, 0x85);
	if (ret < 0)
    {
        dev_err(component->dev,"write es9028_usbin register erro\n");
		return ret;
    }
        dev_err(component->dev,"write es9028_usbin register success\n");

	return 0;
#endif
}

static u64 dummy_dai_formats =
	SND_SOC_POSSIBLE_DAIFMT_I2S	|
	SND_SOC_POSSIBLE_DAIFMT_RIGHT_J	|
	SND_SOC_POSSIBLE_DAIFMT_LEFT_J	|
	SND_SOC_POSSIBLE_DAIFMT_DSP_A	|
	SND_SOC_POSSIBLE_DAIFMT_DSP_B	|
	SND_SOC_POSSIBLE_DAIFMT_AC97	|
	SND_SOC_POSSIBLE_DAIFMT_PDM	|
	SND_SOC_POSSIBLE_DAIFMT_GATED	|
	SND_SOC_POSSIBLE_DAIFMT_CONT	|
	SND_SOC_POSSIBLE_DAIFMT_NB_NF	|
	SND_SOC_POSSIBLE_DAIFMT_NB_IF	|
	SND_SOC_POSSIBLE_DAIFMT_IB_NF	|
	SND_SOC_POSSIBLE_DAIFMT_IB_IF;
static const struct snd_soc_dai_ops es9028_dai_ops = {
	.startup	= es9028_startup,
	.shutdown	= es9028_shutdown,
	.set_fmt	= es9028_set_dai_fmt,
	.hw_params	= es9028_hw_params,
	//.no_capture_mute = 1,
	.auto_selectable_formats	= &dummy_dai_formats,
	.num_auto_selectable_formats	= 1,
};

static struct snd_soc_dai_driver es9028_dai = {
	.name = "es9028pro",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = ES9028_RATES,
		.formats = ES9028_FORMATS,
	},
	.ops = &es9028_dai_ops,
};

#ifdef CONFIG_OF
static const struct of_device_id es9028_dt_ids[] = {
	{ .compatible = "fsl,auralic-es9028pro", },
	{ }
};
MODULE_DEVICE_TABLE(of, es9028_dt_ids);
#endif


static const struct snd_soc_component_driver soc_component_dev_es9028 = {
    .idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};


static int es9028_probe(struct platform_device *pdev)
{
	int ret;
	struct es9028_private *priv;
    
	//	dev_err(&pdev->dev, "probe es9028: \n");
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	
    return devm_snd_soc_register_component(&pdev->dev,
		&soc_component_dev_es9028,
		&es9028_dai, 1);
}

static int es9028_remove(struct platform_device *pdev)
{
	printk(KERN_ERR "remove es9028 driver\n");
    return 0;

}
static struct platform_driver es9028_driver = {
	.driver = {
		.name = "es9028pro",
		.of_match_table = of_match_ptr(es9028_dt_ids),
	},
	.probe		= es9028_probe,
	.remove		= es9028_remove,
};


static int __init es9028_init(void)
{
	int err;

	err = platform_driver_register(&es9028_driver);
	if (err < 0)
		return err;


	return 0;


}
module_init(es9028_init);

static void __exit es9028_exit(void)
{
	platform_driver_unregister(&es9028_driver);
}
module_exit(es9028_exit);
MODULE_DESCRIPTION("AURALiC ES9028  ALSA SoC Codec Driver");
MODULE_AUTHOR("Chang Peng <peng.chang@auralic.com>");
MODULE_LICENSE("GPL");
