// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CS8406 ASoC codec driver
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
#define CS8406_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE  |		\
			     SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define CS8406_PCM_RATES   (SNDRV_PCM_RATE_44100  | \
			     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200  | \
			     SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
                 SNDRV_PCM_RATE_192000)

#define IMXION(x, y)       ((x-1)*32 + y)
#define dit_rst    IMXION(3,0)
#define select_44_48   IMXION(3,1)
#define llink_sw  IMXION(5,5)
extern void aura_set_44_48_sw_pin(int value);
extern void aura_set_llink_sw_pin(int value);
static const struct reg_default cs8406_reg_defaults[] = {
	{ 0x04,	0x00 },
	{ 0x05,	0x85 },
};

static bool cs8406_accessible_reg(struct device *dev, unsigned int reg)
{
	return ((reg == 0x04) || (reg == 0x05));
}

static bool cs8406_writeable_reg(struct device *dev, unsigned int reg)
{
	return cs8406_accessible_reg(dev, reg); 
}

struct cs8406_private {
	struct regmap *regmap;
	unsigned int format;
	/* Current rate for deemphasis control */
	unsigned int rate;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *mclk_gpio;
};


static int cs8406_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int format)
{
	struct snd_soc_component *component = codec_dai->component;
	struct cs8406_private *priv = snd_soc_component_get_drvdata(component);


	priv->format = format;

	return 0;
}

extern int set_sai_param(char *msg);
static int cs8406_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct cs8406_private *priv = snd_soc_component_get_drvdata(component);
	int val = 0, ret;
    int mclk = 0;
	priv->rate = params_rate(params);
    //set_sai_param("asdfks");
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
    aura_set_44_48_sw_pin(mclk);
    /*
    if(0 != gpio_request(llink_sw,  "llink_sw"))
    {
        printk(KERN_ERR "auralic llink_sw gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(llink_sw, 1);
    printk(KERN_ERR "set llink sw to 1\n");
    //usleep_range(100000,110000);
    gpio_free(llink_sw);
    */
    aura_set_llink_sw_pin(1);
    /*
    if(0 != gpio_request(select_44_48,  "select_44_48"))
    {
        printk(KERN_ERR "auralic select_44_48 gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(select_44_48, mclk);
    //usleep_range(100000,110000);
    gpio_free(select_44_48);
	*/
    ret = regmap_write(priv->regmap, 0x04, 0x00);//stop cs8406 first
    
	ret = regmap_write(priv->regmap, 0x04, val);
    usleep_range(100000,110000);
	ret = regmap_write(priv->regmap, 0x04, val|0x40);
    usleep_range(100,110);
	ret = regmap_write(priv->regmap, 0x05, 0x85);
	if (ret < 0)
    {
        dev_err(component->dev,"write cs8406 register erro\n");
		return ret;
    }
        //dev_err(component->dev,"write cs8406 register success\n");

	return 0;
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
static const struct snd_soc_dai_ops cs8406_dai_ops = {
	.set_fmt	= cs8406_set_dai_fmt,
	.hw_params	= cs8406_hw_params,
	.no_capture_mute = 1,
	.auto_selectable_formats	= &dummy_dai_formats,
	.num_auto_selectable_formats	= 1,
};

/*
static int cs8406_dai_probe(struct snd_soc_dai *dai)
{
    printk("in cs8406 dai probe\n");
    dai->extend_msg = "this is a test.";
    return 0;
}
*/
static struct snd_soc_dai_driver cs8406_dai = {
	.name = "cs8406-dit",
    //.id =0,
    //.probe = cs8406_dai_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = CS8406_PCM_RATES,
		.formats = CS8406_PCM_FORMATS,
	},
	.ops = &cs8406_dai_ops,
};

#ifdef CONFIG_OF
static const struct of_device_id cs8406_dt_ids[] = {
	{ .compatible = "fsl,auralic-cs8406", },
	{ }
};
MODULE_DEVICE_TABLE(of, cs8406_dt_ids);
#endif

static const struct regmap_config cs8406_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= 0x7f,
    .cache_type = REGCACHE_RBTREE,
	.reg_defaults		= cs8406_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(cs8406_reg_defaults),
	.writeable_reg		= cs8406_writeable_reg,
	.readable_reg		= cs8406_accessible_reg,
};

static const struct snd_soc_component_driver soc_component_dev_cs8406 = {
    .idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct i2c_device_id cs8406_i2c_id[] = {
	{"cs8406", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cs8406_i2c_id);

static int cs8406_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret;
	struct cs8406_private *priv;
    
		//dev_err(&client->dev, "probe cs8406: \n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	

	priv->regmap = devm_regmap_init_i2c(client, &cs8406_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&client->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(client, priv);
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
	//usleep_range(100000, 200000);//100 ~ 200 ms
    gpio_free(dit_rst);
    */
/*
    if(0 != gpio_request(select_44_48,  "select_44_48"))
    {
        printk(KERN_ERR "auralic select_44_48 gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(select_44_48, 0);
	//usleep_range(100000, 200000);//100 ~ 200 ms
    gpio_free(select_44_48);
*/
/*
    if(0 != gpio_request(llink_sw,  "llink_sw"))
    {
        printk(KERN_ERR "auralic llink_sw gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(llink_sw, 1);
    gpio_free(llink_sw);
    */
    aura_set_llink_sw_pin(1);
	
    usleep_range(100000, 200000);//100 ~ 200 ms

	return devm_snd_soc_register_component(&client->dev,
		&soc_component_dev_cs8406,
		&cs8406_dai, 1);
}

static int cs8406_i2c_remove(struct i2c_client *client)
{
//    gpio_free(dit_rst);
//    gpio_free(select_44_48);
	printk(KERN_ERR "remove cs8406 driver\n");
    return 0;

}
static struct i2c_driver cs8406_i2c_driver = {
	.driver = {
		.name	= "cs8406",
		.of_match_table = of_match_ptr(cs8406_dt_ids),
	},
	.id_table	= cs8406_i2c_id,
	.probe		= cs8406_i2c_probe,
	.remove		= cs8406_i2c_remove,
};

module_i2c_driver(cs8406_i2c_driver);

MODULE_DESCRIPTION("CIRRUS LOGIC CS8406 ALSA SoC Codec Driver");
MODULE_AUTHOR("Chang Peng <peng.chang@auralic.com>");
MODULE_LICENSE("GPL");
