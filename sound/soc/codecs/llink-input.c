// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * LLINK Output ASoC codec driver
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

#define LLINK_INPUT_FORMATS (SNDRV_PCM_FMTBIT_S16_LE  |		\
			     SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE | \
                 SNDRV_PCM_FMTBIT_DSD_U32_LE | SNDRV_PCM_FMTBIT_DSD_U32_BE)

#define LLINK_INPUT_RATES   (SNDRV_PCM_RATE_44100  | \
			     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200  | \
			     SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
                 SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_352800 | \
                 SNDRV_PCM_RATE_384000 | SNDRV_PCM_RATE_705600 | \
                 SNDRV_PCM_RATE_768000)

#define IMXION(x, y)       ((x-1)*32 + y)
#define dit_rst    IMXION(3,0)
#define select_44_48   IMXION(3,1)
#define llink_sw  IMXION(5,5)
static const struct reg_default llink_input_reg_defaults[] = {
	{ 0x04,	0x00 },
	{ 0x05,	0x85 },
};

static bool llink_input_accessible_reg(struct device *dev, unsigned int reg)
{
	return ((reg == 0x04) || (reg == 0x05));
}

static bool llink_input_writeable_reg(struct device *dev, unsigned int reg)
{
	return llink_input_accessible_reg(dev, reg); 
}

struct llink_input_private {
	struct regmap *regmap;
	unsigned int format;
	/* Current rate for deemphasis control */
	unsigned int rate;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *mclk_gpio;
};


static int llink_input_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int format)
{
    //printk(KERN_ERR "in llink output set dai format:%d\n",format);
    /*
	struct snd_soc_component *component = codec_dai->component;
	struct llink_input_private *priv = snd_soc_component_get_drvdata(component);


	priv->format = format;
*/
	return 0;
}

extern void aura_set_llink_sw_pin(int value);
static int llink_input_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct llink_input_private *priv = snd_soc_component_get_drvdata(component);
	int val = 0, ret;
    int mclk = 0;
    //dev_err(component->dev,"write llink_input register success:%d\n",params_rate(params));
    aura_set_llink_sw_pin(0);
    /*
    if(0 != gpio_request(llink_sw,  "llink_sw"))
    {
        printk(KERN_ERR "auralic llink_sw gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(llink_sw, 0);
    //usleep_range(100000,110000);
    gpio_free(llink_sw);
*/
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
	
    ret = regmap_write(priv->regmap, 0x04, 0x00);//stop llink_input first
    
	ret = regmap_write(priv->regmap, 0x04, val);
    usleep_range(100000,110000);
	ret = regmap_write(priv->regmap, 0x04, val|0x40);
    usleep_range(100,110);
	ret = regmap_write(priv->regmap, 0x05, 0x85);
	if (ret < 0)
    {
        dev_err(component->dev,"write llink_input register erro\n");
		return ret;
    }
        dev_err(component->dev,"write llink_input register success\n");

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
static const struct snd_soc_dai_ops llink_input_dai_ops = {
	.set_fmt	= llink_input_set_dai_fmt,
	.hw_params	= llink_input_hw_params,
	//.no_capture_mute = 1,
	.auto_selectable_formats	= &dummy_dai_formats,
	.num_auto_selectable_formats	= 1,
};

/*
static int llink_input_dai_probe(struct snd_soc_dai *dai)
{
    printk("in llink dai probe\n");
    dai->extend_msg = "this is a llink test.";
    return 0;
}
*/
static struct snd_soc_dai_driver llink_input_dai = {
	.name = "llink_input",
    //.probe = llink_input_dai_probe,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = LLINK_INPUT_RATES,
		.formats = LLINK_INPUT_FORMATS,
	},
	.ops = &llink_input_dai_ops,
};

#ifdef CONFIG_OF
static const struct of_device_id llink_input_dt_ids[] = {
	{ .compatible = "fsl,auralic-llink_input", },
	{ }
};
MODULE_DEVICE_TABLE(of, llink_input_dt_ids);
#endif

static const struct regmap_config llink_input_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= 0x7f,
    .cache_type = REGCACHE_RBTREE,
	.reg_defaults		= llink_input_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(llink_input_reg_defaults),
	.writeable_reg		= llink_input_writeable_reg,
	.readable_reg		= llink_input_accessible_reg,
};

static const struct snd_soc_component_driver soc_component_dev_llink_input = {
    .idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};


static int llink_input_probe(struct platform_device *pdev)
{
	int ret;
	struct llink_input_private *priv;
    
		//dev_err(&pdev->dev, "probe llink_input: \n");
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	
/*
	priv->regmap = devm_regmap_init_i2c(client, &llink_input_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&client->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(client, priv);
    */
    /*	
    priv->mclk_gpio = devm_gpiod_get_optional(&client->dev, "mclk-gpio",
						GPIOD_OUT_LOW);
	if (IS_ERR(priv->mclk_gpio)) {
		ret = PTR_ERR(priv->mclk_gpio);
		dev_err(&client->dev, "Failed to get mclk line: %d\n", ret);
		return ret;
	}
    if(!priv->mclk_gpio)
        printk(KERN_ERR "mclk gpio request error...\n");

	priv->reset_gpio = devm_gpiod_get_optional(&client->dev, "reset-gpio",
						GPIOD_OUT_LOW);
	if (IS_ERR(priv->reset_gpio)) {
		ret = PTR_ERR(priv->reset_gpio);
		dev_err(&client->dev, "Failed to get reset line: %d\n", ret);
		return ret;
	}
    if(!priv->reset_gpio)
        printk(KERN_ERR "reset gpio request error...\n");
	usleep_range(100, 200);//100 ~ 200 us
    gpiod_set_value_cansleep(priv->reset_gpio, 1);
*/
/*
    if(0 != gpio_request(dit_rst,  "dit_rst"))
    {
        printk(KERN_ERR "auralic dit_rst gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(dit_rst, 1);
    */
    aura_set_llink_sw_pin(0);
    /*
    if(0 != gpio_request(llink_sw,  "llink_sw"))
    {
        printk(KERN_ERR "auralic llink_sw gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(llink_sw, 0);


	usleep_range(100000, 200000);//100 ~ 200 ms

    gpio_free(llink_sw);
	*/
    return devm_snd_soc_register_component(&pdev->dev,
		&soc_component_dev_llink_input,
		&llink_input_dai, 1);
}

static int llink_input_remove(struct platform_device *pdev)
{
    //gpio_free(dit_rst);
    //gpio_free(select_44_48);
	printk(KERN_ERR "remove llink_input driver\n");
    return 0;

}
/*
static struct i2c_driver llink_input_i2c_driver = {
	.driver = {
		.name	= "llink_input",
		.of_match_table = of_match_ptr(llink_input_dt_ids),
	},
	.id_table	= llink_input_i2c_id,
	.probe		= llink_input_i2c_probe,
	.remove		= llink_input_i2c_remove,
};
*/
static struct platform_driver llink_input_driver = {
	.driver = {
		.name = "llink_input",
		.of_match_table = of_match_ptr(llink_input_dt_ids),
	},
	.probe		= llink_input_probe,
	.remove		= llink_input_remove,
};

//module_i2c_driver(llink_input_i2c_driver);

static int __init llink_input_init(void)
{
	int err;

	err = platform_driver_register(&llink_input_driver);
	if (err < 0)
		return err;


	return 0;


}
module_init(llink_input_init);

static void __exit llink_input_exit(void)
{
	platform_driver_unregister(&llink_input_driver);
}
module_exit(llink_input_exit);
MODULE_DESCRIPTION("AURALiC Lightning Link Input ALSA SoC Codec Driver");
MODULE_AUTHOR("Chang Peng <peng.chang@auralic.com>");
MODULE_LICENSE("GPL");
