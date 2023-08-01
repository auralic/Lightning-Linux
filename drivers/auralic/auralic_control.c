
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>

/* proc filesystem */
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mm.h>

#include "auralic_freq.h" 

#define IMXION(x, y)       ((x-1)*32 + y)
/*	
#define enable_output_pin  IMXION(1,  13)
#define rst_dac_pin        IMXION(5,  20)
#define xtal_sel_pin       IMXION(3,  28)
#define usb_rst_usbin_pin  IMXION(3, 27)
#define usb_rst_hub_pin    IMXION(1, 15)
#define usb_rst_dir_pin    IMXION(3, 25)
#define usb_sel_pin    IMXION(3, 23)
*/



bool ctrlprt_level = true;

enum print_level_ {
    CTRLNOR = 0,
    CTRLDBG = 1,
    CTRLBUT,
};

#define ctrlprt(level, format, args...)   \
do  \
{   \
    if(true == ctrlprt_level) \
    { \
        printk(format, ##args); \
    } \
    else if(level == CTRLNOR) \
    { \
        printk(format, ##args); \
    } \
}   \
while(0)	

input_channel_t ctrl_channel = INPUT_NONE_SELECT;

typedef struct gpio_info
{
	unsigned gpio;
	int val;
	char *label;
}gpio_info_t;

#define llink_sw   IMXION(5,5)
#define dir_rst    IMXION(5,1)
#define dit_rst    IMXION(3,0)
#define select_44_48   IMXION(3,1)
#define usb2_vbus_sw IMXION(3,6)
#define mute_sw   IMXION(3,7)
#define ani_sw   IMXION(3,8)
#define phono_sw IMXION(3,9)
#define xmos_rst   IMXION(2,12)
#define display0_rst IMXION(5,24)
#define lvds_rst  IMXION(5,25)
#define src_int0 IMXION(5,29)
#define src_pwen IMXION(4,11)
#define hpd_rst IMXION(4,10)
#define plug_link_up IMXION(4,15)
#define ready_link_down IMXION(4,14)
#define sleep_sw IMXION(4,29)
const unsigned gpio_array[] = {
    llink_sw,
    dir_rst,
    dit_rst,
    select_44_48,
    usb2_vbus_sw,
    mute_sw,
    ani_sw,
    phono_sw,
    xmos_rst,
//    display0_rst,
//    lvds_rst,
    src_int0,
    src_pwen,
    hpd_rst,
    plug_link_up,
    ready_link_down,
    sleep_sw
};

gpio_info_t gpio_info_array[] = 
{
    
	{
		.gpio = llink_sw,
    	.val = 1,
		.label = "llink_sw"
	},
    
	{
		.gpio = dir_rst,
    	.val = 0,
		.label = "dir_rst"
	},
    
	{
		.gpio = dit_rst,
    	.val = 1,
		.label = "dit_rst"
	},
    
	{
		.gpio = select_44_48,
    	.val = 0,
		.label = "select_44_48"
	},
    
	{
		.gpio = usb2_vbus_sw,
    	.val = 0,
		.label = "usb2_vbus_sw"
	},    
	{
		.gpio = mute_sw,
    	.val = 0,
		.label = "mute_sw"
	},    
	{
		.gpio = ani_sw,
    	.val = 0,
		.label = "ani_sw"
	},    
	{
		.gpio = phono_sw,
    	.val = 0,
		.label = "phono_sw"
	},    
	{
		.gpio = xmos_rst,
    	.val = 0,
		.label = "xmos_rst"
	},
    /*
	{
		.gpio = display0_rst,
    	.val = 0,
		.label = "display0_rst"
	},    
	{
		.gpio = lvds_rst,
    	.val = 0,
		.label = "lvds_rst"
	}, 
    */
	{
		.gpio = src_int0,
    	.val = 0,
		.label = "src_int0"
	},
    {
        .gpio = src_pwen,
        .val = 0,
        .label = "src_pwen"
    },
    {
        .gpio = hpd_rst,
        .val =1,
        .label = "hpd_rst"
    },

    {
		.gpio = plug_link_up,
    	.val = 1,
		.label = "plug_link_up"
	},    
	{
		.gpio = ready_link_down,
    	.val = 1,
		.label = "ready_link_down"
	},
    
	{
		.gpio = sleep_sw,
    	.val = 1,
		.label = "sleep_sw"
	} 
};

void aura_set_llink_sw_pin(int value)
{	
	gpio_set_value(llink_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_llink_sw_pin);
void aura_set_dir_rst_pin(int value)
{	
	gpio_set_value(dir_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_dir_rst_pin);
void aura_set_dit_rst_pin(int value)
{	
	gpio_set_value(dit_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_dit_rst_pin);
void aura_set_44_48_sw_pin(int value)
{	
	gpio_set_value(select_44_48, value);	
}
EXPORT_SYMBOL_GPL(aura_set_44_48_sw_pin);

void aura_set_usb2_vbus_sw_pin(int value)
{	
	gpio_set_value(usb2_vbus_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_usb2_vbus_sw_pin);
void aura_set_mute_sw_pin(int value)
{	
	gpio_set_value(mute_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_mute_sw_pin);
void aura_set_ani_sw_pin(int value)
{	
	gpio_set_value(ani_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_ani_sw_pin);
void aura_set_phono_sw_pin(int value)
{	
	gpio_set_value(phono_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_phono_sw_pin);
void aura_set_xmos_rst_pin(int value)
{	
	gpio_set_value(xmos_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_xmos_rst_pin);

void aura_set_display0_rst_pin(int value)
{	
	gpio_set_value(display0_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_display0_rst_pin);
void aura_set_lvds_rst_pin(int value)
{	
	gpio_set_value(lvds_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_lvds_rst_pin);
void aura_set_src_int0_pin(int value)
{	
	gpio_set_value(src_int0, value);	
}
EXPORT_SYMBOL_GPL(aura_set_src_int0_pin);

void aura_set_plug_link_up_pin(int value)
{	
	gpio_set_value(plug_link_up, value);	
}
EXPORT_SYMBOL_GPL(aura_set_plug_link_up_pin);
void aura_set_ready_link_down_pin(int value)
{	
	gpio_set_value(ready_link_down, value);	
}
EXPORT_SYMBOL_GPL(aura_set_ready_link_down_pin);

void aura_set_sleep_sw_pin(int value)
{	
	gpio_set_value(sleep_sw, value);	
}
EXPORT_SYMBOL_GPL(aura_set_sleep_sw_pin);

void aura_set_hpd_rst_pin(int value)
{	
	gpio_set_value(hpd_rst, value);	
}
EXPORT_SYMBOL_GPL(aura_set_hpd_rst_pin);
bool is_aes_coax_tos_channel(void)
{	
	if(AES_COAX_TOS == ctrl_channel)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(is_aes_coax_tos_channel);

bool is_usbin_channel(void)
{	
	if(USB_DEVICE== ctrl_channel)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(is_usbin_channel);
 
bool is_streaming_channel(void)
{	
	if(STREAMING== ctrl_channel)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(is_streaming_channel);



void aura_select_input_channel_for_aries(input_channel_t channel)
{
	
    ctrl_channel = channel;
    switch(channel)
    {
        case AES_COAX_TOS:
            {
                aura_set_dir_rst_pin(0);
                msleep(10);
                aura_set_dir_rst_pin(1);
            }
            break;
		case USB_DEVICE:
            {
                aura_set_xmos_rst_pin(0);
                msleep(10);
                aura_set_xmos_rst_pin(1);
                //aura_set_llink_sw_pin(1);
            }
		break;
        /*
        case LIGHTNING_LINK:
        {
            aura_set_llink_sw_pin(0);
        }
        break;
        */
        default:
            break;
    }
/*	
	switch(channel)
	{
		case STREAMING:
			select_streaming();
		break;
		
		case USB_DEVICE_BYPASS:
			select_usb_device_bypass();
		break;
		
		case AES_COAX_TOS:
			select_aes_coax_tos_bypass();
		break;
		

		default:
		break;
	}
    */
}
EXPORT_SYMBOL_GPL(aura_select_input_channel_for_aries);

void aura_select_input_channel_for_vega(input_channel_t channel)
{
	
    ctrl_channel = channel;
    switch(channel)
    {
        case AES_COAX_TOS:
            {
                aura_set_xmos_rst_pin(0);
                aura_set_ani_sw_pin(0);
                aura_set_dir_rst_pin(0);
                msleep(10);
                aura_set_dir_rst_pin(1);
                //msleep(10);
            }
            break;
		case USB_DEVICE:
            {
                aura_set_dir_rst_pin(0);
                aura_set_ani_sw_pin(0);
                aura_set_xmos_rst_pin(0);
                msleep(10);
                aura_set_xmos_rst_pin(1);
                aura_set_llink_sw_pin(1);
            }
		break;
        case LIGHTNING_LINK:
        {
            aura_set_xmos_rst_pin(0);
            aura_set_dir_rst_pin(0);
            aura_set_ani_sw_pin(0);
            aura_set_llink_sw_pin(0);
        }
        break;
        case STREAMING:
        {
            aura_set_xmos_rst_pin(0);
            aura_set_dir_rst_pin(0);
            aura_set_ani_sw_pin(0);
        }
        break;
        case ANALOG:
        {
            aura_set_xmos_rst_pin(0);
            aura_set_dir_rst_pin(0);
            aura_set_ani_sw_pin(1);
        }
        break;
        default:
            break;
    }
/*	
	switch(channel)
	{
		case STREAMING:
			select_streaming();
		break;
		
		case USB_DEVICE_BYPASS:
			select_usb_device_bypass();
		break;
		
		case AES_COAX_TOS:
			select_aes_coax_tos_bypass();
		break;
		

		default:
		break;
	}
    */
}
EXPORT_SYMBOL_GPL(aura_select_input_channel_for_vega);
int aura_request_gpio(void)
{    
    int i, ret;
    int size  = sizeof(gpio_info_array) / sizeof(gpio_info_array[0]);
    
    for(i=0; i<size; i++)
    {
        if(0 > (ret=gpio_request(gpio_info_array[i].gpio, gpio_info_array[i].label))) // gpio5_8
        {
            ctrlprt(CTRLNOR, "auralic control module request %s(gpio %d) failed, ret=%d!\n", 
				   gpio_info_array[i].label, gpio_info_array[i].gpio, ret);
            return 0;
        }
    }
	
    for(i=0; i<size; i++)
    {
        gpio_direction_output(gpio_info_array[i].gpio, gpio_info_array[i].val);
    }

    ctrlprt(CTRLDBG, "auralic control module request gpio success!\n");
    
    return 0;
}

void aura_free_gpio(void)
{    
    int i;
    int size  = sizeof(gpio_info_array) / sizeof(gpio_info_array[0]);
    
    for(i=0; i<size; i++)
    {
        gpio_free(gpio_info_array[i].gpio);
    }
}

static int __init auralic_control_init(void)
{    
    ctrlprt(CTRLNOR, "loading auralic altair g1 control module!\n");    
    
    return aura_request_gpio();
}

static void __exit auralic_control_exit(void)
{
    ctrlprt(CTRLNOR, "unloading altair g1 control module!\n");
	aura_free_gpio();
}

module_init(auralic_control_init);
module_exit(auralic_control_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AURALiC 2019-03-28");
MODULE_DESCRIPTION("gpio control driver for altair g1");
