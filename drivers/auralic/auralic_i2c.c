/*
 * Driver for EETI eGalax Multiple Touch Controller
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 *
 * based on max11801_ts.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* EETI eGalax serial touch screen controller is a I2C based multiple
 * touch screen controller, it supports 5 point multiple touch. */

/* TODO:
  - auto idle mode support
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>

/* proc filesystem */
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/cdev.h> 
#include "auralic_freq.h"
#define ENABLE_LLINK 0
#define DEV_I2C      	1
#define RETRY_COUNT			5
static char I2C_ADDR = 0x40;
static DEFINE_MUTEX(iic_mutex);
struct i2c_client *i2c_dev = NULL;
#define  IMX_GPIO_NR(bank, nr)		(((bank) - 1) * 32 + (nr))

/* char dev related */
#define		FREQ_DEV_NAME  "freq"
dev_t freq_no;
struct class *freq_class = NULL;

bool is_ak4113 = true;//ak4113 or dix9211
bool usbin_open = false;
bool usbin_close_auto_mute = false;
//int sys_freq = 44100;
int sys_freq = 0;
int sys_volume = 100;
bool fresh_freq = false;
bool fresh_volume = false;
bool fresh_usbset = false;
bool usbin_hang_status   = false;
bool fresh_usbset_close = false;
bool is_freq_vol = false;
bool is_ready_llink_up = false;
bool is_plug_llink_down = false;
wait_queue_head_t freqwq;

typedef struct freqdev_   
{  
    struct cdev cdev;  
}freqdev_t; 
freqdev_t freqdev;

atomic_t freq_read_cnt;
wait_queue_head_t freq_read_wq;

bool print_level = false;

enum print_level_ {
    IRNOR = 0,
    IRDBG = 1,
    IRBUT,
};

#define print(level, format, args...)   \
do  \
{   \
    if(true == print_level) \
    { \
        printk(format, ##args); \
    } \
    else if(level == IRNOR) \
    { \
        printk(format, ##args); \
    } \
}   \
while(0)

extern bool is_aes_coax_tos_channel(void);
extern bool is_usbin_channel(void);
extern bool is_streaming_channel(void);
//extern void aura_select_aes_coax_tos_bypass(void);
//extern void dac_mute(void);
//extern void dac_init(unsigned int samfreq);
extern bool restore_volume_db(void);
extern bool dac_read_reg(char reg, char *data);
extern unsigned int dac_get_samfreq(void);
extern void dac_umute(void);
extern void dac_setting_mode(int mode);
extern int dac_getting_mode(void);
extern void dac_setting_coefficient(int mode, int fsl);
extern bool is_dac_mute(void);

static unsigned int samfreq_array[] = 
{
	0, //FSL_UNCLK, unlock
	1, //FSL_PAR, odd-even error
	2, //FSL_AUTO, formate error
	3, //FSL_AUDION,  audio error
	44100,
	48000,
	88200,
	96000,
	176400,
	192000,
	352800,
	384000,
	705600,
	768000,
	2822400,
	3072000,
	5644800,
	6144000,
	11289600,
	12288000,
	22579200,
	24576000
};

#define	 SAMFREQ_ARRAY_SIZE  (sizeof(samfreq_array) / sizeof(samfreq_array[0]))

unsigned int aura_get_freq_by_idx(char idx)
{
	if(SAMFREQ_ARRAY_SIZE > idx)
		return samfreq_array[idx];

	return 0;
}

int aura_get_volume_by_db(char db)
{
	if(0 == db)
		return 100;
	else if(129 > db)
		return 0; // invalid db value
	else
	{
		if(db > 138)
			return (db-129)*100/128 + 1;
		else
			return (db-129)*100/128;
	}
}


bool aura_iic_raw_read(unsigned short irc_addr, char *data, char datalen)
{
    char cnt = 0;
    int len=0;
	bool result = false;
    unsigned char buff[10];
    
    mutex_lock(&iic_mutex);
    while(cnt++ < RETRY_COUNT)
    {
        i2c_dev->addr = irc_addr;
        
        if(datalen != (len=i2c_master_recv(i2c_dev, buff, datalen)))
        {
            print(IRNOR, "aura_iic_raw_read(0x%02x): read failed %d, len=%d, should be(%d)!\n", i2c_dev->addr, cnt, len, datalen);
            msleep(10);
            continue;
        }    

		memcpy(data, buff, len);
		result = true;
        break;
    }
	
    mutex_unlock(&iic_mutex);

	return result;
}

bool aura_iic_read_reg(unsigned short irc_addr, char reg, char *data)
{
    char cnt = 0;
    int len=0;
	bool result = false;
    unsigned char ret=0xff;
    unsigned char buff[2];
    
    mutex_lock(&iic_mutex);
    while(cnt++ < RETRY_COUNT)
    {
        i2c_dev->addr = irc_addr;
        
        /* write set value */
        buff[0] = reg;//read reg
        if(1 != (ret=i2c_master_send(i2c_dev, buff, 1)))
        {
            print(IRNOR, "aura_iic_read_reg(0x%02x): select reg %d failed %d, len=%d!\n", i2c_dev->addr, reg, cnt, ret);
            msleep(10);
            continue;
        }
        
        if(1 != (len=i2c_master_recv(i2c_dev, buff, 1)))
        {
            print(IRNOR, "aura_iic_read_reg(0x%02x): read reg %d failed %d, len=%d!\n", i2c_dev->addr, reg, cnt, len);
            msleep(10);
            continue;
        }    

		*data = buff[0];
		result = true;
        break;
    }
	
    mutex_unlock(&iic_mutex);

	return result;
}

bool aura_iic_write_reg(unsigned short irc_addr, char reg, char data)
{   
    char cnt = 0;
    unsigned char ret=0xff;
    unsigned char buff[3];
	bool result = false;

    mutex_lock(&iic_mutex);
    while(cnt++ < RETRY_COUNT)
    {
        i2c_dev->addr = irc_addr;
		
        /* write set value */
        buff[0] = reg;//
        buff[1] = data;
        if(2 != (ret=i2c_master_send(i2c_dev, buff, 2)))
        {
            print(IRNOR, "aura_iic_write_reg(0x%02x): write reg %d failed %d, len=%d!\n",i2c_dev->addr, reg, cnt, ret);
            msleep(10);
            continue;
        }	
		result = true;
        break;
    }
    mutex_unlock(&iic_mutex);
   
    return result;
}

//#define     I2C_ADDR   	   0x10     // write 0x20  read 0x21
#define		OCKS0			  (1<<2)
#define		OCKS1			  (1<<3)

#define		ERR_PAR			  (1<<0)
#define		ERR_AUDION		  (1<<1)
#define		ERR_UNLCK		  (1<<4)
#define		ERR_AUTO		  (1<<6)

#define		pin156				IMX_GPIO_NR(1,20)
#define		pin158				IMX_GPIO_NR(4,12)
#define		DAC_INT0_PIN		IMX_GPIO_NR(6,31)


#define    DIR_RST    IMX_GPIO_NR(5,1)
#define		DIR_INT0_PIN		IMX_GPIO_NR(3,16)
#define		DIR_INT1_PIN		IMX_GPIO_NR(3,14)
#define		USBIN_INT0_PIN		IMX_GPIO_NR(5,0)


#define 	READY_TO_LLINK_DOWN_PIN    IMX_GPIO_NR(4, 14)//output
#define 	PLUG_FROM_LLINK_DOWN_PIN   IMX_GPIO_NR(4, 12)//input
#define 	READY_FROM_LLINK_UP_PIN    IMX_GPIO_NR(4, 13)//input
#define 	PLUG_TO_LLINK_UP_PIN       IMX_GPIO_NR(4, 15)//output


#define     VEGA2_DETECT_PLUG_BY_INT   0

#if !(VEGA2_DETECT_PLUG_BY_INT)
static struct task_struct *plug_poll_task = NULL;
int polling_delay_ms = 10;


int get_polling_delay_ms(void)
{
    return polling_delay_ms;
}
EXPORT_SYMBOL_GPL(get_polling_delay_ms);

void set_polling_delay_ms(int val)
{
    polling_delay_ms = val;
}
EXPORT_SYMBOL_GPL(set_polling_delay_ms);
#endif

input_channel_t ak_channel = AES;


//input_channel_t ak_channel = AES_BYPASS;

typedef enum ak4113_fsl
{
	AK_44K   = 0x0,
	AK_48K   = 0x2,
	AK_88K   = 0x8,
	AK_96K   = 0xa,
	AK_176K  = 0xc,
	AK_192K  = 0xe
}ak4113_fsl_t;

ak4113_fsl_t ak_fsl_array[] = {AK_44K, AK_48K, AK_88K, AK_96K, AK_176K, AK_192K};

enum ak4113_interrupt
{
	AK_INT_NOR = 0,//normal
	AK_INT_PAR,
	AK_INT_FOR,
	AK_INT_ALL
};

typedef enum sys_fsl
{
	FSL_44K  = 0,
	FSL_48K, // 1
	FSL_88K, // 2
	FSL_96K, // 3
	FSL_176K,// 4
	FSL_192K,// 5
	FSL_UNCLK, // 6 unlock
	FSL_PAR, // 7 odd-even error
	FSL_AUTO, // 8 formate error
	FSL_AUDION, // 9 audio error
	FSL_BUTT
}sys_fsl_t;


sys_fsl_t ak_fsl = FSL_44K;

static struct task_struct *i2c_task = NULL;
static struct task_struct *usbin_task = NULL;
static struct task_struct *dac_task = NULL;
atomic_t ak_irq_cnt = ATOMIC_INIT(0);
atomic_t usbin_irq_cnt = ATOMIC_INIT(0);
atomic_t dac_irq_cnt = ATOMIC_INIT(0);
int dir_interrupt_status = 0;

bool dix9211_read_reg(char reg, char *data)
{
	return aura_iic_read_reg(I2C_ADDR, reg, data);
}
EXPORT_SYMBOL_GPL(dix9211_read_reg);

bool dix9211_write_reg(char reg, char data)
{
	return aura_iic_write_reg(I2C_ADDR, reg, data);
}
EXPORT_SYMBOL_GPL(dix9211_write_reg);

void aura_select_channel_dix9211_aries(input_channel_t channel)
{
	switch(channel)
	{
		case AES:
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x40);
		break;
		
		case TOS1:
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0xc3);
		break;

		case TOS2:
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0xc4);
		break;
		
		case COAX1:
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x81);
		break;

		case COAX2:
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x82);
		break;
		
        default:
		break;
	}
}
void aura_select_channel_dix9211_vega(input_channel_t channel)
{
	switch(channel)
	{
		case AES:
    	//aura_iic_write_reg(I2C_ADDR, 0x34, 0x40);
    	//aura_iic_write_reg(I2C_ADDR, 0x34, 0x02);
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x01);
		break;
		
		case TOS1:
    	//aura_iic_write_reg(I2C_ADDR, 0x34, 0xc2);
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x03);
		break;

		
		case COAX1:
    	//aura_iic_write_reg(I2C_ADDR, 0x34, 0x81);
    	aura_iic_write_reg(I2C_ADDR, 0x34, 0x00);
		break;

		
        default:
		break;
	}
}
void reset_sys_freq(void)
{
            ak_fsl = FSL_UNCLK;
            sys_freq = 0;
            if(false == fresh_freq)
                fresh_freq = true;
            else 
                fresh_freq = false;
            wake_up_interruptible_all(&freqwq);
}
EXPORT_SYMBOL_GPL(reset_sys_freq);
char aura_get_dix9211_fsl_idx(void)
{
	char i , size;
	char data = 0xff;
	
    char retry;
    for(retry=0;retry<5;retry++)
    {
        if(true == aura_iic_read_reg(I2C_ADDR, 38, &data))
        {
            char tmp = data & 0x80;
            if(tmp == 0x00)
            {
                data = data & 0x0f;
                size = sizeof(ak_fsl_array) / sizeof(ak_fsl_array[0]);
                    
                    for(i=0; i < size; i++)
                    {
                        if(data == ak_fsl_array[i])
                            break;
                    }
                
                    return i;
            }
            else if(tmp == 0x80)
            {
                mdelay(20);
            }
        }
	    else
		    print(IRNOR, "auralic can't get ak4113 fsl\n");
    }
    return 0xff;
}

void aura_set_dix9211_clock(sys_fsl_t fsl)
{
	char data = 0xff;
	static char last;

	if(last == fsl)
		return ;
	
	last = fsl;
    //if(true == aura_iic_read_reg(I2C_ADDR, 0, &data))
	if(true)
    {
		switch(fsl)
		{
			case FSL_44K:
			case FSL_48K:
			data = 0x0a;  // clear ocks1 ocks0
			break;
			
			case FSL_88K:
			case FSL_96K:
			data = 0x05;  // clear ocks1 ocks0
			break;
			
			case FSL_176K:
			case FSL_192K:
            data = 0x00;
			break;

			default:
			break;
		}
		if(data!=0xff)
    	    aura_iic_write_reg(I2C_ADDR, 0x31, data);
	}
	else
	{
		print(IRNOR, "auralic set ak4113 clock failed!\n");
	}
}
void aura_read_dix9211_fsl(void)
{	
	if(false == is_aes_coax_tos_channel())
	{		
		print(IRNOR, "non-aes channel\n");
		return ;
	}
	char data = 0;
    char retry;
    for(retry=0;retry<5;retry++)
    {
        if(aura_iic_read_reg(I2C_ADDR, 0x38, &data))
        {
            char tmp = data & 0x80;
            if(tmp == 0x00)
            {
                switch(data)
                {
                    case 0x08://44.1K
                        ak_fsl = FSL_44K;
                        break;
                    case 0x09://48K
                        ak_fsl = FSL_48K;
                        break;
                    case 0x0b://88.2K
                        ak_fsl = FSL_88K;
                        break;
                    case 0x0c://96K
                        ak_fsl = FSL_96K;
                        break;
                    case 0x0e://176.4K
                        ak_fsl = FSL_176K;
                        break;
                    case 0x0f://192K
                        ak_fsl = FSL_192K;
                        break;
                    default://unlck
                        ak_fsl = FSL_UNCLK;
                        break;
                }
                break;
            }
            else if(tmp == 0x80)
            {
                mdelay(20);
            }
        }
    }
    if(retry >=4) ak_fsl = FSL_UNCLK;
		print(IRNOR, "in read fsl func...0x%x\n",data);
	
	//dir_interrupt_status = (gpio_get_value(DIR_INT1_PIN) << 1) | gpio_get_value(DIR_INT0_PIN);
	if(FSL_192K >= ak_fsl)
	{
		/* valid frequency */
		sys_freq = aura_get_freq_by_idx(ak_fsl+4);// skip 4 invalid samfreq		
		print(IRNOR, "2aes/coax/tos sys_freq=%d\n", sys_freq);

		if(false == fresh_freq)
			fresh_freq = true;
		else 
			fresh_freq = false;
		
		wake_up_interruptible_all(&freqwq);
		
		//aura_set_xmos_in_rst_pin(1);
		//mdelay(300);//wait xmos in boot from spi flash
		//aura_set_input_sel1_pin(1);
	}
	else
	{
		/* invalid frequency */		
		sys_freq = aura_get_freq_by_idx(ak_fsl - FSL_UNCLK);
		print(IRNOR, "3aes/coax/tos sys_freq=%d\n", sys_freq);
		
		if(false == fresh_freq)
			fresh_freq = true;
		else 
			fresh_freq = false;
		
		wake_up_interruptible_all(&freqwq);
		
		//aura_set_xmos_in_rst_pin(0);
		//aura_set_input_sel1_pin(0);
	}
	
	atomic_set(&freq_read_cnt, 1);
	wake_up_interruptible(&freq_read_wq);
}
void aura_init_dix9211_aries(input_channel_t channel)
{
	char data = 0;
        aura_iic_write_reg(I2C_ADDR, 0x25, 0x3F);
        aura_iic_write_reg(I2C_ADDR, 0x27, 0xC3);
        aura_iic_write_reg(I2C_ADDR, 0x28, 0x07);
        aura_iic_write_reg(I2C_ADDR, 0x2A, 0x01);
        aura_iic_write_reg(I2C_ADDR, 0x30, 0x12);
        /* set channel */
        aura_select_channel_dix9211_aries(channel);
	mdelay(100);
}
EXPORT_SYMBOL_GPL(aura_init_dix9211_aries);

void aura_init_dix9211_vega(input_channel_t channel)
{
	char data = 0;
        aura_iic_write_reg(I2C_ADDR, 0x25, 0x3F);
        aura_iic_write_reg(I2C_ADDR, 0x27, 0xC3);
        aura_iic_write_reg(I2C_ADDR, 0x28, 0x07);
        aura_iic_write_reg(I2C_ADDR, 0x2A, 0x01);
        aura_iic_write_reg(I2C_ADDR, 0x30, 0x12);
        /* set channel */
        aura_select_channel_dix9211_vega(channel);
	mdelay(100);
}
EXPORT_SYMBOL_GPL(aura_init_dix9211_vega);

irqreturn_t int0_dir_interrupt_handler(int devid, void * data)
{
    char data1 = 0;
    //aura_iic_read_reg(I2C_ADDR, 0x38, &data1);
    print(IRNOR,"in interrupt0..%d\n",data1);
    //return IRQ_HANDLED;
    char tmp = data1 & 0x80;
    if(tmp == 0x00)
    {
        atomic_inc(&ak_irq_cnt);
            if(i2c_task)
            {
                wake_up_process(i2c_task);
            }
    }
    
    else if(tmp == 0x80)
    {
		if(true == is_aes_coax_tos_channel())
		{		
            ak_fsl = FSL_UNCLK;
            sys_freq = 0;
            print(IRNOR, "1aes/coax/tos sys_freq=%d\n", sys_freq);
            if(false == fresh_freq)
                fresh_freq = true;
            else 
                fresh_freq = false;
            wake_up_interruptible_all(&freqwq);
        }
    }
    
    return IRQ_HANDLED;
}
irqreturn_t int1_dir_interrupt_handler(int devid, void * data)
{
    dir_interrupt_status = gpio_get_value(DIR_INT1_PIN);
    print(IRNOR,"int1 interruput...,gpio value is%c\n",dir_interrupt_status);
    if(dir_interrupt_status == 0)//设置为unlock
    {
            ak_fsl = FSL_UNCLK;
            sys_freq = 0;
            print(IRNOR, "aes/coax/tos sys_freq=%d\n", sys_freq);
            if(false == fresh_freq)
                fresh_freq = true;
            else 
                fresh_freq = false;
               wake_up_interruptible_all(&freqwq);
    }
    else //通知task读38号寄存器
    {
        atomic_inc(&ak_irq_cnt);
        if(i2c_task)
        {
            wake_up_process(i2c_task);
        }
    }
    return IRQ_HANDLED;
}

void read_xmos_usbin_i2c(void)
{
	char hubuf[4] = {0};
    if(true == aura_iic_raw_read(0x50, hubuf, 4))
    {
        print(IRNOR, "xmos usbin i2c: 0x%x  0x%x  0x%x 0x%x\n", hubuf[0],  hubuf[1], hubuf[2], hubuf[3]);
    }
    else
    {
        print(IRNOR, "xmos usbin i2c: read failed!\n"); 
    }
}
EXPORT_SYMBOL_GPL(read_xmos_usbin_i2c);

void usbin_set_dac_auto_mute(bool ismute)
{
    usbin_close_auto_mute = ismute;
}
EXPORT_SYMBOL_GPL(usbin_set_dac_auto_mute);

bool usbin_get_dac_auto_mute(void)
{
    return usbin_close_auto_mute;
}
EXPORT_SYMBOL_GPL(usbin_get_dac_auto_mute);

extern void aura_dac_mute(void);
irqreturn_t usbin_int0_handler(int devid, void * data)
{
	//printk(KERN_ERR "interrupt usbin_int0 = %d\n", gpio_get_value(USBIN_INT0_PIN));

	if(0 == gpio_get_value(USBIN_INT0_PIN))
	{
		atomic_inc(&usbin_irq_cnt);
		if(usbin_task)
		{
		    wake_up_process(usbin_task);
		}
	}
    return IRQ_HANDLED;
}
extern void aura_set_44_48_sw_pin(int value);
int usbin_int0_fn(void *data)
{	
	char hubuf[4] = {0};
    printk(KERN_ERR "auralic start usbin_int0_fn\n");

	if(true == is_usbin_channel() || true == is_streaming_channel())
    {
	    if(true == aura_iic_raw_read(0x50, hubuf, 4))
		{
			sys_freq = aura_get_freq_by_idx(hubuf[1]);
			sys_volume = hubuf[2];
		}
	}
			
    while(!kthread_should_stop())
    {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
		
		if(false == is_usbin_channel() && false == is_streaming_channel())
			continue;
		
        while(0 < atomic_read(&usbin_irq_cnt))
        {
            //atomic_set(&usbin_irq_cnt, 0);//
            atomic_dec(&usbin_irq_cnt);//
            hubuf[0] = 0;
            if(true == aura_iic_raw_read(0x50, hubuf, 4))
        	{        		
				if(hubuf[0] & 1)
				{	
					//dac_mute();	//already muted by usb open
					is_freq_vol = true;
					sys_freq = aura_get_freq_by_idx(hubuf[1]);
					
					if(false == fresh_freq)
						fresh_freq = true;
					else 
						fresh_freq = false;
					
					wake_up_interruptible_all(&freqwq);
				}
				
				if(hubuf[0] & 2)
				{
					is_freq_vol = true;
					sys_volume = aura_get_volume_by_db(hubuf[2]);					
					
					if(false == fresh_volume)
						fresh_volume = true;
					else 
						fresh_volume = false;
					
					wake_up_interruptible_all(&freqwq);
				}
				
				if(hubuf[0] & 4)// usb settting
				{				
				    //usb by pass mute dac
					//dac_mute();		

                    if(0x20 == hubuf[3])
                    {
                        if(true == usbin_close_auto_mute)
                        {
                            //aura_dac_mute();
                        }
                        
                        usbin_open = false;
    					if(false == fresh_usbset_close)
    						fresh_usbset_close = true;
    					else 
    						fresh_usbset_close = false;
    					
    					wake_up_interruptible_all(&freqwq);
    					
    					print(IRNOR, "detect xmos usbin close buff[3]=0x%x  dac %sauto muted!\n", 
                              hubuf[3], true == usbin_close_auto_mute ? "":"non-");
                    }
                    else
                    {
                        usbin_open = true;
    					if(false == fresh_usbset)
    						fresh_usbset = true;
    					else 
    						fresh_usbset = false;
    					
    					wake_up_interruptible_all(&freqwq);
    					
    					print(IRNOR, "detect xmos usbin open buff[3]=0x%x\n", hubuf[3]);
                    }
				}	

				if(true == is_freq_vol)
				{
					is_freq_vol = false;
					print(IRNOR, "xmos usbin sys_freq=%d  sys_volume=%d!\n", sys_freq, sys_volume);
                   /* 
                    if(sys_freq % 44100) 
                        aura_set_44_48_sw_pin(1);//48x
                    else
                        aura_set_44_48_sw_pin(0);//44x
                        */
        		}
			}
        }
    }
    
    set_current_state(TASK_RUNNING);
	
    print(IRNOR, "auralic leave usbin_int0_fn\n");
    
    return 0;
}
int i2c_task_fn(void *data)
{
    print(IRNOR, "auralic start i2c_task_fn\n");
    while(!kthread_should_stop())
    {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
		if(false == is_aes_coax_tos_channel())
			continue;		
		
        while(0 < atomic_read(&ak_irq_cnt))
        {
            atomic_set(&ak_irq_cnt, 0);
            //atomic_dec(&ak_irq_cnt);//
            {
                mdelay(10);
                aura_read_dix9211_fsl();
			/* set ocks0 and ocks1 */
			aura_set_dix9211_clock(ak_fsl);
            }
        }
    }
    set_current_state(TASK_RUNNING);
    print(IRNOR, "auralic leave i2c_task_fn\n");
    return 0;
}
int aura_freq_open(struct inode *inode, struct file *filp)     
{
    return 0;
}  

int aura_freq_release(struct inode *inode, struct file *filp)      
{   
    return 0;  
}  

static ssize_t aura_freq_read(struct file *filp, char __user *usrbuf, size_t count, loff_t *ppos)  
{	
	char len = 0;
	char buff[100];
	
	if(filp->f_flags & O_NONBLOCK)  
    {  
    	len += sprintf(buff, "%d", (int)ak_fsl);
		if(0 != copy_to_user(usrbuf, buff, len))
	    {
	        return 0;
	    }
		print(IRDBG, "read fsl non-block, fsl=%d, len=%d\n", (int)ak_fsl, len);
		return len;
    }
	else
	{
		if(0 == wait_event_interruptible(freq_read_wq, 0 != atomic_read(&freq_read_cnt)))
		{
			atomic_set(&freq_read_cnt, 0);
	    	len += sprintf(buff, "%d", (int)ak_fsl);
			if(0 != copy_to_user(usrbuf, buff, len))
		    {
		        return 0;
		    }
			print(IRDBG, "read fsl blocked, fsl=%d\n", (int)ak_fsl);
			
			return len;
		}
	}
	
	return 0;
}  

ssize_t aura_freq_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	return count;
} 

void aura_get_sysfreq_block(void)
{
	if(false == fresh_freq)
		wait_event_interruptible(freqwq, fresh_freq);
	else
		wait_event_interruptible(freqwq, (!fresh_freq));
}
EXPORT_SYMBOL_GPL(aura_get_sysfreq_block);

unsigned int aura_get_sysfreq_imdiat(void)
{
	return sys_freq;
}
EXPORT_SYMBOL_GPL(aura_get_sysfreq_imdiat);

long aura_freq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
	//int *parg = (int *)arg;
    int __user *parg = (int __user *)arg;
    char buff[4];
    
    if (_IOC_TYPE(cmd) != FREQ_IOC_MAGIC) 
	{
		print(IRNOR, "auralic vega2 ioctl invalid magic, magic=0x%x ioc_type=0x%x\n", FREQ_IOC_MAGIC, _IOC_TYPE(cmd));
        return -EINVAL;
	}

    if (_IOC_NR(cmd) > FREQ_CMD_MAXNR)  
	{
		print(IRNOR, "auralic vega2 ioctl invalid ioc_nr, IRKEY_CMD_MAXNR=0x%x ioc_nr=0x%x\n", FREQ_CMD_MAXNR, _IOC_NR(cmd));
        return -EINVAL;
	}

	switch(cmd)
	{
		case GET_FREQ_BLOCK:
		aura_get_sysfreq_block();
		print(IRDBG, "ioctl fresh freq = %d\n", sys_freq);
		break;
		
		case GET_FREQ_IMDIAT:
		print(IRDBG, "ioctl get freq = %d\n", sys_freq);
		unsigned int freq = aura_get_sysfreq_imdiat();
        put_user(freq,parg);
		break;
		
		case GET_VOL_BLOCK:
		if(false == fresh_volume)
			wait_event_interruptible(freqwq, fresh_volume);
		else
			wait_event_interruptible(freqwq, (!fresh_volume));

		print(IRDBG, "ioctl fresh sys_volume = %d\n", sys_volume);
		break;
		
		case GET_VOL_IMDIAT:
		print(IRDBG, "ioctl get volume = %d\n", sys_volume);
		//*parg = sys_volume;
        put_user(sys_volume,parg);
		break;	
		
		case GET_USB_OPEN_BLOCK:
		if(false == fresh_usbset)
			wait_event_interruptible(freqwq, fresh_usbset);
		else
			wait_event_interruptible(freqwq, (!fresh_usbset));

		print(IRDBG, "ioctl usb open blocked(dac mute)\n");
		break;	
		
		case GET_USB_CLOSE_BLOCK:
		if(false == fresh_usbset_close)
			wait_event_interruptible(freqwq, fresh_usbset_close);
		else
			wait_event_interruptible(freqwq, (!fresh_usbset_close));

		print(IRDBG, "ioctl usb close blocked(dac mute)\n");
		break;	
        
		case GET_USBIN_OPEN_IMDIAT:
        if(usbin_open == true)
        {
            //*parg =  1;
            put_user(1,parg);
            print(IRDBG, "ioctl get usbin_open = true\n");
        }
        else
        {
            //*parg =  0;
            put_user(0,parg);
            print(IRDBG, "ioctl get usbin_open = false\n");
        }
		break;
        
		case GET_USBIN_HANG_IMDIAT:
        //*parg =  0;
        if(true == is_usbin_channel())
        {
            int value = 0;
            if(true != aura_iic_raw_read(0x50, buff, 4))
            {
                //*parg =  1;
                value = 1;
            }
            else
            {
                if(SAMFREQ_ARRAY_SIZE <= buff[1])
                    value = 1;
                    //*parg =  1;
            }
            put_user(value,parg);
        }
        else
        {
            put_user(0,parg);
        }
        
        //print(IRDBG, "ioctl get usbin_hang = %s\n", value == 0 ? "false":"yes");
		break;
		case GET_DIX9211_FREQ:
        char tmp;
        unsigned int freq1 = 0;
		if(true == dix9211_read_reg(56, &tmp))
        {
            char tmp1 = tmp & 0x80;
            if(tmp1 == 0x00)
            {
                switch(tmp)
                {
                    case 0x08://44.1K
                        freq1 = 44100;
                        break;
                    case 0x09://48K
                        freq1 = 48000;
                        break;
                    case 0x0b://88.2K
                        freq1 = 88200;
                        break;
                    case 0x0c://96K
                        freq1 = 96000;
                        break;
                    case 0x0e://176.4K
                        freq1 = 176400;
                        break;
                    case 0x0f://192K
                        freq1 = 192000;
                        break;
                    default://unlck
                        freq1 = 0;
                        break;
                }
            }
        }
            put_user(freq1,parg);
		break;
#if ENABLE_LLINK	
		case GET_PLUG_LLINK_DOWN_BLOCK:
		if(false == is_plug_llink_down)
			wait_event_interruptible(freqwq, is_plug_llink_down);
		else
			wait_event_interruptible(freqwq, (!is_plug_llink_down));

		print(IRDBG, "ioctl got plug_llink_down rising\n");
		break;
		
		case GET_PLUG_LLINK_DOWN_IMDIAT:
		print(IRDBG, "ioctl get plug_llink_down = %d\n", gpio_get_value(PLUG_FROM_LLINK_DOWN_PIN));
		int value1 = gpio_get_value(PLUG_FROM_LLINK_DOWN_PIN);
		put_user(value1,parg);
        break;
		
		case GET_READY_LLINK_UP_BLOCK:
		if(false == is_ready_llink_up)
			wait_event_interruptible(freqwq, is_ready_llink_up);
		else
			wait_event_interruptible(freqwq, (!is_ready_llink_up));

		print(IRDBG, "ioctl got ready_link_up rising\n");
		break;
		
		case GET_READY_LLINK_UP_IMDIAT:
		print(IRDBG, "ioctl get ready_llink_up = %d\n", gpio_get_value(READY_FROM_LLINK_UP_PIN));
		int value2 = gpio_get_value(READY_FROM_LLINK_UP_PIN);
        put_user(value2,parg);
		break;
		
		case SET_PLUG_LLINK_UP:
		print(IRDBG, "ioctl set plug_llink_up = %lu\n", arg);
        gpio_set_value(PLUG_TO_LLINK_UP_PIN, arg);
		break;
		
		case SET_READY_LLINK_DOWN:
		print(IRDBG, "ioctl set ready_llink_down = %lu\n", arg);
        gpio_set_value(READY_TO_LLINK_DOWN_PIN, arg);
		break;
#endif
	}
	
    return ret;
}
#if ENABLE_LLINK
void set_ready_to_link_down(int val)
{
    gpio_set_value(READY_TO_LLINK_DOWN_PIN, val);
}
EXPORT_SYMBOL_GPL(set_ready_to_link_down);

void set_plug_to_link_up(int val)
{
    gpio_set_value(PLUG_TO_LLINK_UP_PIN, val);
}
EXPORT_SYMBOL_GPL(set_plug_to_link_up);

int get_ready_to_link_down(void)
{
    return gpio_get_value(READY_TO_LLINK_DOWN_PIN);
}
EXPORT_SYMBOL_GPL(get_ready_to_link_down);

int get_plug_to_link_up(void)
{
    return gpio_get_value(PLUG_TO_LLINK_UP_PIN);
}
EXPORT_SYMBOL_GPL(get_plug_to_link_up);

int get_plug_from_link_down(void)
{
    return gpio_get_value(PLUG_FROM_LLINK_DOWN_PIN);
}
EXPORT_SYMBOL_GPL(get_plug_from_link_down);

int get_ready_from_link_up(void)
{
    return gpio_get_value(READY_FROM_LLINK_UP_PIN);
}
EXPORT_SYMBOL_GPL(get_ready_from_link_up);
#endif
int plug_poll_task_fn(void *data)
{	
    int plugfromdown, readyfromup;
	int new_key;
	
    print(IRNOR, "auralic start plug_poll_task_fn\n");
    
    plugfromdown = gpio_get_value(PLUG_FROM_LLINK_DOWN_PIN);
    readyfromup  = gpio_get_value(READY_FROM_LLINK_UP_PIN);
    
    while(!kthread_should_stop())
    {
        
        schedule_timeout_interruptible(HZ/(1000/polling_delay_ms));
        
		new_key = gpio_get_value(PLUG_FROM_LLINK_DOWN_PIN); 
        if(plugfromdown != new_key)
        {
            plugfromdown = new_key;
            print(IRNOR, "interrupt  plug_llink_down = %d\n", new_key);
    		if(false == is_plug_llink_down)
    			is_plug_llink_down = true;
    		else 
    			is_plug_llink_down = false;
    		
    		wake_up_interruptible_all(&freqwq);
        }
        
		new_key = gpio_get_value(READY_FROM_LLINK_UP_PIN); 
        if(readyfromup != new_key)
        {
            readyfromup = new_key;
            print(IRNOR, "interrupt ready_llink_up = %d\n", new_key);					
    		if(false == is_ready_llink_up)
    			is_ready_llink_up = true;
    		else 
    			is_ready_llink_up = false;
    		
    		wake_up_interruptible_all(&freqwq);
        }
    }
    
    set_current_state(TASK_RUNNING);
	
    print(IRNOR, "auralic leave plug_poll_task_fn\n");
    
    return 0;
}
static const struct file_operations aura_freq_fops = 
{      
	.owner = THIS_MODULE, 
	.open = aura_freq_open,
	.release = aura_freq_release,
	.read = aura_freq_read,
	.write = aura_freq_write,
	.unlocked_ioctl = aura_freq_ioctl,
};

void aura_regist_freq_dev(void)
{
	if(0 < alloc_chrdev_region(&freq_no, 0, 1, FREQ_DEV_NAME))
	{
		print(IRNOR, "auralic register /dev/%s failed!\n", FREQ_DEV_NAME);
		return ;
	}
	
	cdev_init(&freqdev.cdev, &aura_freq_fops);
	freqdev.cdev.owner = THIS_MODULE;
	freqdev.cdev.ops = &aura_freq_fops;
	cdev_add(&freqdev.cdev, freq_no, 1); 

	freq_class = class_create(THIS_MODULE, FREQ_DEV_NAME);
	device_create(freq_class, NULL, freq_no, NULL, FREQ_DEV_NAME);
	
	init_waitqueue_head(&freq_read_wq);
	atomic_set(&freq_read_cnt, 0);
}

void aura_unregist_freq_dev(void)
{
	device_destroy(freq_class, freq_no); 
	class_destroy(freq_class);
    cdev_del(&freqdev.cdev); 
    unregister_chrdev_region(freq_no,1);
}
#define TEST_ADDR 0x40 //dix9211 address
static int auralic_i2c_probe(struct i2c_client *client,
				             const struct i2c_device_id *id)
{
    print(IRNOR, "loading auralic i2c module!\n");
	/*
    if(0 != gpio_request(DIR_RST,  "dir_rst"))
    {
             print(IRNOR, "auralic dir_rst gpio request failed!\n");
             return 0;
         }
     
    gpio_direction_output(DIR_RST, 1);

    mdelay(100);
    */
    
    i2c_dev = client;
    char data = 0;
    if(aura_iic_read_reg(TEST_ADDR, 0x20, &data))
    {
        print(IRNOR,"read success\n");
        is_ak4113 = false;
        I2C_ADDR = 0x40;
    }
    //print(IRNOR,"is ak4113:%d\n",is_ak4113);
	
    aura_regist_freq_dev();
        if(0 != request_irq(gpio_to_irq(DIR_INT0_PIN), 
                    int0_dir_interrupt_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "dir_int0", NULL))
        {
            print(IRNOR, "auralic dir int0 request interrupt failed!\n");
                return -ENOMEM;
        }


        if(0 != request_irq(gpio_to_irq(DIR_INT1_PIN), 
                    int1_dir_interrupt_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "dir_int1", NULL))
        {
            print(IRNOR, "auralic dir int1 request interrupt failed!\n");
                return -ENOMEM;
        }
	if(0 != request_irq(gpio_to_irq(USBIN_INT0_PIN), 
        usbin_int0_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "usbin_int0", NULL))
    {
        print(IRNOR, "auralic irkey request usbin int0 interrupt failed!\n");
        return -ENOMEM;
    }
#if ENABLE_LLINK
    if(0 != gpio_request(PLUG_FROM_LLINK_DOWN_PIN,  "PLUG_FROM_LLINK_DOWN_PIN"))
    {
        print(IRNOR, "auralic PLUG_FROM_LLINK_DOWN_PIN gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_input(PLUG_FROM_LLINK_DOWN_PIN);
    
    if(0 != gpio_request(READY_FROM_LLINK_UP_PIN,  "READY_FROM_LLINK_UP_PIN"))
    {
        print(IRNOR, "auralic READY_FROM_LLINK_UP_PIN gpio request failed!\n");
        return -ENOMEM;
    }
    gpio_direction_input(READY_FROM_LLINK_UP_PIN);

	plug_poll_task = kthread_run(plug_poll_task_fn, NULL, "plug_poll_task");
    if(IS_ERR(plug_poll_task))
    {
        plug_poll_task = NULL;
        pr_err("auralic create plug polling task thread failed!\n");
        return -ENOMEM;
    }
    gpio_direction_output(READY_TO_LLINK_DOWN_PIN, 1); // 1=not ready  0=ready
    gpio_direction_output(PLUG_TO_LLINK_UP_PIN, 0);    // 1=not plug   0=plug
#endif
    usbin_task = kthread_run(usbin_int0_fn, NULL, "usbin_task");
    if(IS_ERR(usbin_task))
    {
        usbin_task = NULL;
        pr_err("auralic create usbin task thread failed!\n");
        return -ENOMEM;
    }
	#ifdef DEV_I2C

    i2c_task = kthread_run(i2c_task_fn, NULL, "i2c_task");
    if(IS_ERR(i2c_task))
    {
        i2c_task = NULL;
        pr_err("auralic create i2c task thread failed!\n");
        return -ENOMEM;
    }
	
	
	init_waitqueue_head(&freqwq);
	
	#endif
   
    return 0;
}

static int auralic_i2c_remove(struct i2c_client *client)
{
	#ifdef DEV_I2C

    if(NULL != i2c_task)
    {
        kthread_stop(i2c_task);
        wake_up_process(i2c_task);
        i2c_task = NULL;
    }
    if(NULL != usbin_task)
    {
        kthread_stop(usbin_task);
        wake_up_process(usbin_task);
        usbin_task = NULL;
    }
    if(NULL != dac_task)
    {
        kthread_stop(dac_task);
        wake_up_process(dac_task);
        dac_task = NULL;
    }
#if ENABLE_LLINK
    if(NULL != plug_poll_task)
    {
        kthread_stop(plug_poll_task);        
        wake_up_process(plug_poll_task);
        plug_poll_task = NULL;
    }
#endif
    //gpio_free(DIR_RST);	
	aura_unregist_freq_dev();
	free_irq(gpio_to_irq(DIR_INT0_PIN), NULL);
	free_irq(gpio_to_irq(DIR_INT1_PIN), NULL);
	free_irq(gpio_to_irq(USBIN_INT0_PIN), NULL);
#if ENABLE_LLINK 
    gpio_free(READY_TO_LLINK_DOWN_PIN);
    gpio_free(PLUG_TO_LLINK_UP_PIN);
    gpio_free(PLUG_FROM_LLINK_DOWN_PIN);
    gpio_free(READY_FROM_LLINK_UP_PIN);
#endif
    #endif


    print(IRNOR, "unloading auralic i2c module!\n");
	return 0;
}

static const struct i2c_device_id auralic_i2c_id[] = {
	{ "ak4113", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, auralic_i2c_id);
#ifdef CONFIG_PM_SLEEP
static int auralic_suspend(struct device *dev)
{
	return 0;
}
static int auralic_resume(struct device *dev)
{
	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(auralic_pm_ops, auralic_suspend, auralic_resume);

static struct of_device_id auralic_dac_dt_ids[] = {
	{ .compatible = "fsl,auralic" },
	{ /* sentinel */ }
};

static struct i2c_driver auralic_i2c_driver = {
	.driver = {
		.name	= "auralic_ak4113_driver",
		.owner	= THIS_MODULE,
		.pm	    = &auralic_pm_ops,
		.of_match_table	= of_match_ptr(auralic_dac_dt_ids),
	},
	.id_table	= auralic_i2c_id,
	.probe		= auralic_i2c_probe,
	.remove		= auralic_i2c_remove,
};
module_i2c_driver(auralic_i2c_driver);
MODULE_AUTHOR("AURALiC LIMTED.");
MODULE_DESCRIPTION("auralic i2c driver for dix9211/xmos_usbin");
MODULE_LICENSE("GPL");
