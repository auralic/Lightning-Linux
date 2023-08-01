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

#define	SYS_45MHZ			0  // 0: 98Mhz  1: 45Mhz
#define DEV_ES9028      	1
#define RETRY_COUNT			5
static DEFINE_MUTEX(iic_mutex);
struct i2c_client *i2c_dev = NULL;
bool print_level = false;
bool sys_mute = false;
unsigned int sys_mode = 1;
unsigned int sys_samfreq = 44100;
static unsigned int last_samfreq = 48000;


static unsigned int samfreq_array[] = 
{
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

#define SAMFREQ_SIZE  (sizeof(samfreq_array) / sizeof(samfreq_array[0]))

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

extern void aura_set_dit_rst_pin(int value);
extern void aura_set_44_48_sw_pin(int value);

bool samfreq_is_valid(unsigned int samfreq)
{
	int i = 0;

	for(i=0; i < SAMFREQ_SIZE; i++)
		if(samfreq == samfreq_array[i])
			return true;

	return false;
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
EXPORT_SYMBOL_GPL(aura_iic_read_reg);

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
            print(IRNOR, "aura_iic_write_reg(0x%02x): write reg %d failed %d, len=%d!\n", i2c_dev->addr, reg, cnt, ret);
            msleep(10);
            continue;
        }
		result = true;
        break;
    }
    mutex_unlock(&iic_mutex);
   
    return result;
}
EXPORT_SYMBOL_GPL(aura_iic_write_reg);

#if DEV_ES9028
#define     DAC_ADDR   	      0x48    // write 0x90  read 0x91
bool dac_set_samfreq(unsigned int samfreq);

bool dac_read_reg(char reg, char *data)
{
	return aura_iic_read_reg(DAC_ADDR, reg, data);
}
EXPORT_SYMBOL_GPL(dac_read_reg);

bool dac_write_reg(char reg, char data)
{
	return aura_iic_write_reg(DAC_ADDR, reg, data);
}
EXPORT_SYMBOL_GPL(dac_write_reg);

#define     MODE_DIR_LEN_MAX    50
#define     MODE_X1248_COUNT    4  //x1 x2 x4 x8
#define     MODE_STAGE1_LEN     128
#define     MODE_STAGE2_LEN     16
#define     MODE_FILE_NAME_FORMAT  "mode%d/x%d_stage%d.txt"
char        MODE_FILE_DIR[MODE_DIR_LEN_MAX]={"/home/"};

int dsd256_delay_ms = 200; /* delay for switching to dsd256 simple rate */

typedef struct
{
    char len;
    int stage1[MODE_X1248_COUNT][MODE_STAGE1_LEN];
    int stage2[MODE_X1248_COUNT][MODE_STAGE2_LEN];
}COEFF_MODE;

COEFF_MODE aura_mode[5]; /* only use mode[2] mode[3] mode[4] */

void aura_dac_make_file_name(char *name, char name_len, char mode, char x1248, char stage)
{
    memset(name, '\0', name_len);
    snprintf(name, name_len, "%smode%d/x%d_stage%d.txt", MODE_FILE_DIR, mode, x1248, stage);
}

bool aura_dac_analyse_mode_files(void)
{
    bool ret = true;
    #define file_len  200
    int mode;
    char file[file_len];
    int x1248_shift;
    void *buff = NULL;
    char *buftmp = NULL;
    loff_t  pos;
    mm_segment_t fs; 
    struct file *fp = NULL;
    struct page *page = NULL;
    struct iovec iov; // new
	struct kiocb kiocb;
	struct iov_iter iter;

    page = alloc_page(GFP_KERNEL);
    if(NULL == page)
    {
        print(IRNOR, "auralic dac alloc pages failed!\n");
        return false;
    }
    buff = page_address(page);
    
    for(mode=2; mode<=4; mode++)/* mode2  mode3  mode4 */
    {
        for(x1248_shift=0; x1248_shift<4; x1248_shift++)/* 1<<0 1<<1 1<<2 1<<3 */
        {
            /* stage 1 */
            aura_dac_make_file_name(file, file_len, mode, 1<<x1248_shift, 1);
            fp = filp_open(file, O_RDWR, 0644);
            if(!IS_ERR(fp))
            {
                int len;
                //fs = get_fs();
                //set_fs(KERNEL_DS);
		        fs = force_uaccess_begin();
                pos = 0;
                memset(buff, 0, 1024);
                iov.iov_base = (void __user *)buff;
                iov.iov_len = 1024;
                init_sync_kiocb(&kiocb, fp);
                kiocb.ki_pos = fp->f_pos;
                iov_iter_init(&iter, READ, &iov, 1, 1024);
                //if(0 < (len=fp->f_op->read_iter(&kiocb, &iter)))
                loff_t pos = 0;
                fp->f_pos = 0;
                size_t count = 1024;
		        if(0 < (len = kernel_read(fp,buff,count,&pos)))
                {
                    int i, coeff_cnt=0;
                    buftmp = (char *)buff;
                    for(i=0; i<len; i++)
                    {
                        if(',' == ((char *)buff)[i])
                        {
                            if(1 == sscanf(buftmp, "%d,", &aura_mode[mode].stage1[x1248_shift][coeff_cnt++]))
                            {
                                buftmp = (char *)(buff+i+1);
                            }
                            else
                            {
                                ret = false;
                                print(IRDBG, "auralic dac sscanf %s failed!\n", file);
                                break;
                            }
                        }
                    }
                    print(IRDBG, "auralic dac %s, coeff_cnt=%d!\n", file, coeff_cnt);
                    aura_mode[mode].len = coeff_cnt; //2==mode ? 128:64;
                    if(64 != coeff_cnt && 128 != coeff_cnt)
                    {
                        ret = false;
                    }
                }
                else
                {
                    ret = false;
                    print(IRDBG, "auralic dac read %s failed!\n", file);
                }
                filp_close(fp, NULL);
                //set_fs(fs); 
                force_uaccess_end(fs);
            }
            else
            {
                ret = false;
                print(IRDBG, "auralic dac open %s failed!\n", file);
            }
            
            /* stage 2 */
            aura_dac_make_file_name(file, file_len, mode, 1<<x1248_shift, 2);
            fp = filp_open(file, O_RDWR, 0644);
            if(!IS_ERR(fp))
            {
                int len;
                //fs = get_fs();
                //set_fs(KERNEL_DS);
		        fs = force_uaccess_begin();
                pos = 0;
                memset(buff, 0, 128);
                iov.iov_base = (void __user *)buff;
                iov.iov_len = 128;
                init_sync_kiocb(&kiocb, fp);
                kiocb.ki_pos = fp->f_pos;
                iov_iter_init(&iter, READ, &iov, 1, 128);
                //if(0 < (len=fp->f_op->read_iter(&kiocb, &iter)))
                loff_t pos = 0;
                fp->f_pos = 0;
                size_t count = 128;
		        if(0 < (len = kernel_read(fp,buff,count,&pos)))
                {
                    int i, coeff_cnt=0;
                    buftmp = (char *)buff;
                    for(i=0; i<len; i++)
                    {
                        if(',' == ((char *)buff)[i])
                        {
                            if(1 == sscanf(buftmp, "%d,", &aura_mode[mode].stage2[x1248_shift][coeff_cnt++]))
                            {
                                buftmp = (char *)(buff+i+1);
                            }
                            else
                            {
                                ret = false;
                                print(IRDBG, "auralic dac sscanf %s failed!\n", file);
                                break;
                            }
                        }
                    }
                    print(IRDBG, "auralic dac %s, coeff_cnt=%d!\n", file, coeff_cnt);
                    if(16 != coeff_cnt)
                    {
                        ret = false;
                    }
                }
                else
                {
                    ret = false;
                    print(IRDBG, "auralic dac read %s failed!\n", file);
                }
                filp_close(fp, NULL);
                //set_fs(fs); 
                force_uaccess_end(fs);
            }
            else
            {
                ret = false;
                print(IRDBG, "auralic dac open %s failed!\n", file);
            }
        }
    }
    
    free_page((unsigned long)buff);

    return ret;
}

int dac_get_coefficient_index(int fsl)
{
	int idx;
	switch(fsl)
	{
		case 44100:
		case 48000:
			idx = 0;
		break;
		
		case 88200:
		case 96000:
			idx = 1;
		break;
		
		case 176400:
		case 192000:
			idx = 2;
		break;
		
		case 352800:
		case 384000:
			idx = 3;
		break;
		
		case 705600:
		case 768000:
			idx = 4;
		break;
		
		case 2822400:
		case 3072000:
			idx = 5;
		break;
		
		case 5644800:
		case 6144000:
			idx = 6;
		break;
		
		case 11289600:
		case 12288000:
			idx = 7;
		break;
		
		case 22579200:
		case 24576000:
			idx = 8;
		break;

		default:
			return 100;
		break;
	}
	
	return idx;
}

void dac_set_filter_coefficient(int *state1, int *state2, int state1_cnt)
{
    int i;
    int tmp;
        
    /* coefficient write enable */
    dac_write_reg(37, 0x02);
    	
    /* stage1: 128 coefficients */
    for(i=0; i<state1_cnt; i++)
    {
        dac_write_reg(32, i);// set state1 adress
        tmp = state1[i];
        dac_write_reg(33, tmp&0xff);// set coefficients
        dac_write_reg(34, (tmp>>8)&0xff);// set coefficients
        dac_write_reg(35, (tmp>>16)&0xff);// set coefficients
        dac_write_reg(36, (tmp>>24)&0xff);// set coefficients
    }
    
    /* coefficient write disable */
    dac_write_reg(37, 0x00);
    /* coefficient write enable */
    dac_write_reg(37, 0x02);
    
    /* stage2: 16 coefficients */
    for(i=0; i<16; i++)
    {
        dac_write_reg(32, i|0x80);// set state2 adress
        tmp = state2[i];
        dac_write_reg(33, tmp&0xff);// set coefficients
        dac_write_reg(34, (tmp>>8)&0xff);// set coefficients
        dac_write_reg(35, (tmp>>16)&0xff);// set coefficients
        dac_write_reg(36, (tmp>>24)&0xff);// set coefficients
    }
    
    /* coefficient write disable */
    dac_write_reg(37, 0x00);  
}

void dac_setting_mode(int mode)
{
	sys_mode = mode;
}
EXPORT_SYMBOL_GPL(dac_setting_mode);

int dac_getting_mode(void)
{
	return sys_mode;
}
EXPORT_SYMBOL_GPL(dac_getting_mode);

char * dac_getting_path(void)
{
	return MODE_FILE_DIR;
}
EXPORT_SYMBOL_GPL(dac_getting_path);

void dac_mute(void);
void dac_umute(void);

void dac_setting_coefficient(int mode, int fsl)
{	
	bool ismute = false;
	char data = 0;
	char reg7_dat = 0, reg37_dat = 0; 

	if(true == dac_read_reg(7, &data))
	{
		if(data & 1)
			ismute = true;
		else
			ismute = false;
	}
	
	dac_mute();
	
   	if(0 == mode)
	{		
		if(fsl < 2822400)
		{
			/* 176 <--> 384 768 ... < dsd set bit7=1 bypass_osf*/
			dac_read_reg(37, &data);
			data |= 0x80;
			dac_write_reg(37, data);
		}else
		{
			/* dsd 2822.... clear bit7=0 bypass_osf*/
			dac_read_reg(37, &data);
			data &= 0x7f;
			dac_write_reg(37, data);
		}
        
		if(false == ismute)
            dac_umute();
        
		return ;
	}
	
	/* mode 1-4, enable internel fileter */	
	if(fsl < 2822400)
	{
		//pcm
		int x1248 = dac_get_coefficient_index(fsl);
        if(4 > x1248) //384 == 3
        {
            print(IRDBG, "dac_update_coefficient fsl=%dKHz mode=%d x%d\n", 
                             fsl, mode, 1<<x1248);
                                         
            if(2==mode || 3==mode || 4==mode)
                dac_set_filter_coefficient(aura_mode[mode].stage1[x1248], 
                                               aura_mode[mode].stage2[x1248], 
                                               aura_mode[mode].len);
            switch(mode)
            {
                case 1:
                /* use chip's inner filter */					
				//reg7: fast roll-off, reg37: bypass_osf=0 even_stage2=0 prog_coeff_en=0
				reg7_dat = 0x00;//fast rolloff
				reg37_dat = 0x00;//bypass_osf=0 even_stage2=0 prog_coeff_en=0
				break;

                case 2:
                /* coefficient active enable */
                switch(x1248)
                {
                    case 0://x1			
                    //reg7: fast rolloff, reg37: bypass_osf=0 even_stage2=1 prog_coeff_en=1                    
					reg7_dat = 0x00;//fast rolloff
					reg37_dat = 0x05;//bypass_osf=0 even_stage2=1 prog_coeff_en=1
                    break;
                    
                    case 1://x2	
                    //reg7: fast rolloff, reg37: bypass_osf=0 even_stage2=1 prog_coeff_en=1                   
					reg7_dat = 0x00;//fast rolloff
					reg37_dat = 0x05;//bypass_osf=0 even_stage2=1 prog_coeff_en=1
                    break;
                    
                    case 2://x4	
                    //reg7: fast rolloff, reg37: bypass_osf=0 even_stage2=1 prog_coeff_en=1                   
					reg7_dat = 0x00;//fast rolloff
					reg37_dat = 0x05;//bypass_osf=0 even_stage2=1 prog_coeff_en=1
                    break;
                    
                    case 3://x8	
                    //reg7: fast rolloff, reg37: bypass_osf=0 even_stage2=0 prog_coeff_en=1                   
					reg7_dat = 0x00;//fast rolloff
					reg37_dat = 0x01;//bypass_osf=0 even_stage2=0 prog_coeff_en=1
                    break;
                }
                break;
                
                case 3://slow rolloff
                case 4:
                /* coefficient active enable */	
                //reg7: slow rolloff, reg37: bypass_osf=0 even_stage2=1 prog_coeff_en=1                   
				reg7_dat = 0x20;//slow rolloff, linear phase
				reg37_dat = 0x05;//bypass_osf=0 even_stage2=1 prog_coeff_en=1
                break;
            }
        }
	}
	else
	{
		//dsd 
        /* use chip's inner filter */
		//reg7: fast rolloff, reg37: bypass_osf=0 even_stage2=0 prog_coeff_en=0
		reg7_dat = 0x00;//fast rolloff
        reg37_dat = 0x00;//bypass_osf=0 even_stage2=0 prog_coeff_en=0
        switch (mode)
        {
            case 1:
                reg7_dat = 0xE6; //Set 70k DSD filter
                break;

            case 2:
                reg7_dat = 0xE4; //Set 60k DSD filter
                break;

            case 3:
            case 4:
                reg7_dat = 0xE2; //Set 50k DSD filter
                break;

            default:
                reg7_dat = 0xE0; //Should not run to here
                break;
        }
    }

	
	if(true == ismute)
		reg7_dat |= 1;//mute 
		
	dac_write_reg(7, reg7_dat);
	
	dac_write_reg(37, reg37_dat);
}
EXPORT_SYMBOL_GPL(dac_setting_coefficient);

void dac_setting_path(char *path, int length)
{
	if(MODE_DIR_LEN_MAX < length)
	{
		print(IRNOR, "dac set new path %s failed, mode path is too long!\n", path);
		return ;
	}
	
	memset(MODE_FILE_DIR, 0, MODE_DIR_LEN_MAX);
	memcpy(MODE_FILE_DIR, path, length);
	if( false == aura_dac_analyse_mode_files())
	{
		print(IRNOR, "dac set new path %s failed!\n", MODE_FILE_DIR);
	}
	else
	{
		dac_setting_coefficient(sys_mode, sys_samfreq);		
		print(IRNOR, "dac set new path %s success!\n", MODE_FILE_DIR);
	}
}
EXPORT_SYMBOL_GPL(dac_setting_path);

void dac_hard_reset(void)
{
	//aura_set_rst_dac_pin(0);
	aura_set_dit_rst_pin(0);
	mdelay(2);
	aura_set_dit_rst_pin(1);
}
EXPORT_SYMBOL_GPL(dac_hard_reset);

void dac_soft_reset(void)
{
	aura_iic_write_reg(DAC_ADDR, 0,  1);// bit0: start soft reset
	//mdelay(2);
	//aura_iic_write_reg(DAC_ADDR, 0,  0);// bit0: stop soft reset
}
EXPORT_SYMBOL_GPL(dac_soft_reset);

void dac_setting_channel_mapping(char value)
{
	aura_iic_write_reg(DAC_ADDR, 38, value);
	aura_iic_write_reg(DAC_ADDR, 39, value);
	aura_iic_write_reg(DAC_ADDR, 40, value);
	aura_iic_write_reg(DAC_ADDR, 41, value);
}

void dac_init(unsigned int samfreq)
{
    print(IRNOR, "auralic dac init %u!\n", samfreq);

    aura_dac_analyse_mode_files();
    last_samfreq = 1234;    //force dac_set_samfreq to run all codes.
	dac_set_samfreq(samfreq);

}
EXPORT_SYMBOL_GPL(dac_init);

void dac_mute(void)
{
    //printk(KERN_ERR "call dac mute\n");
	char data = 0;
	if(true == aura_iic_read_reg(DAC_ADDR, 7, &data))
	{
		data |= 1; //set bit0
		aura_iic_write_reg(DAC_ADDR, 7, data);
	}
	else
	{
	    print(IRNOR, "auralic dac mute failed!\n");
	}
}
EXPORT_SYMBOL_GPL(dac_mute);

void aura_dac_mute(void)
{
	sys_mute = true;
	dac_mute();
}
EXPORT_SYMBOL_GPL(aura_dac_mute);

void dac_umute(void)
{
    //printk(KERN_ERR "call dac unmute\n");
	char data = 0;
	if(true == aura_iic_read_reg(DAC_ADDR, 7, &data))
	{
		data &= 0xfe; // clear bit0
		aura_iic_write_reg(DAC_ADDR, 7, data);
	}
	else
	{
	    print(IRNOR, "auralic dac umute failed!\n");
	}
}

void aura_dac_umute(void)
{
	sys_mute = false;
	dac_umute();
}
EXPORT_SYMBOL_GPL(aura_dac_umute);

bool dac_getting_mute_state(void)
{
	return sys_mute;
}
EXPORT_SYMBOL_GPL(dac_getting_mute_state);
bool is_multiple_sample_rate(unsigned int samfreq)
{
    if(samfreq>last_samfreq?samfreq%last_samfreq:last_samfreq%samfreq)
        return true;
    else 
        return false;
}
EXPORT_SYMBOL_GPL(is_multiple_sample_rate);
	
bool dac_set_samfreq(unsigned int samfreq)
{
    char data;
    bool ismute = false;
    if(false == samfreq_is_valid(samfreq))
	{
		print(IRNOR, "dac receive invalid samfreq = %d\n", samfreq);
		return false;
	}
	
	print(IRNOR, "dac receive valid samfreq = %d\n", samfreq);

    #if 0 // freq change mute dac operated by app
	if(true == dac_read_reg(7, &data))
	{
		if(data & 1)
			ismute = true;
		else
			ismute = false;
	}
	
	dac_mute();
	mdelay(50);
    #endif
    
    if(samfreq>last_samfreq?samfreq%last_samfreq:last_samfreq%samfreq)
    {
        aura_set_dit_rst_pin(0);  //reset DAC if we need to switch clock source
        //print(IRNOR, "aura_set_rst_dac_pin 0\n");
    }
        
	if(samfreq % 44100)
		aura_set_44_48_sw_pin(1); // 48x
	else
		aura_set_44_48_sw_pin(0); // 44x

    mdelay(1);
    aura_set_dit_rst_pin(1);
    //print(IRNOR, "aura_set_rst_dac_pin 1\n");

    if(samfreq>last_samfreq?samfreq%last_samfreq:last_samfreq%samfreq)
    {
        mdelay(10);
        dac_mute();
    }
    if(samfreq>last_samfreq?samfreq%last_samfreq:last_samfreq%samfreq)
    {
        //THD2,THD3 setting
        aura_iic_write_reg(DAC_ADDR,28,5);
        aura_iic_write_reg(DAC_ADDR,29,0);
        aura_iic_write_reg(DAC_ADDR,30,22);
        aura_iic_write_reg(DAC_ADDR,31,0);
    }
	if(samfreq < 2822400)
	{
		aura_iic_write_reg(DAC_ADDR, 1, 0);//pcm
		dac_setting_channel_mapping(0x45);
	}
	else
	{
		aura_iic_write_reg(DAC_ADDR, 1, 3);//dsd
		dac_setting_channel_mapping(0x23);
	}

    if(samfreq>last_samfreq?samfreq%last_samfreq:last_samfreq%samfreq)
    {
        aura_iic_write_reg(DAC_ADDR, 10, 0xe0);// default freq = 44100
	    aura_iic_write_reg(DAC_ADDR, 12, 0x00);
        //print(IRNOR, "switch clock init\n");
    }

    //90MHZ
    if(0 == sys_mode)
    {   
        /* in mode 0, 705K 768K is not supported now, set it as 352K 384K */
    	switch(samfreq)
    	{
    		case 44100:
    		case 48000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x01);
            break;
                
    		case 2822400:
    		case 3072000: 
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x20);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 88200:
    		case 96000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x02);
            break;
                
    		case 5644800:
    		case 6144000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x40);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 176400:
    		case 192000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x04);
            break;
                
    		case 11289600:
    		case 12288000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x80);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 352800:
    		case 384000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x08);
            break;
                
    		case 22579200:
    		case 24576000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x01);
    		break;
    		
    		case 705600:
    		case 768000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x01);// 705K 768K is not supported now, set it as 352K 384K here
    		break;
    	}	
    }
    else
    {
    	switch(samfreq)
    	{
    		case 44100:
    		case 48000:
    		case 2822400:
    		case 3072000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x20);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 88200:
    		case 96000:
    		case 5644800:
    		case 6144000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x40);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 176400:
    		case 192000:
    		case 11289600:
    		case 12288000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x80);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x00);
    		break;
    		
    		case 352800:
    		case 384000:
    		case 22579200:
    		case 24576000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x01);
    		break;
    		
    		case 705600:
    		case 768000:
    		aura_iic_write_reg(DAC_ADDR, 42, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 43, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 44, 0x00);
    		aura_iic_write_reg(DAC_ADDR, 45, 0x01);
    		break;
    	}	
    }

	if(last_samfreq != samfreq)
	{
		dac_setting_coefficient(sys_mode, samfreq);
	}
	
	sys_samfreq = samfreq;
	last_samfreq = samfreq;

    #if 0 // freq change umute dac operated by app
    if(false == ismute)
    {
        //mdelay(200);
		dac_umute();
    }
    #endif

	return true;
}
EXPORT_SYMBOL_GPL(dac_set_samfreq);

unsigned int dac_get_samfreq(void)
{
	return sys_samfreq;
}
EXPORT_SYMBOL_GPL(dac_get_samfreq);

#endif
static int auralic_i2c_probe(struct i2c_client *client,
				             const struct i2c_device_id *id)
{
    print(IRNOR, "loading auralic dac module!\n");
    
    i2c_dev = client;

	#ifdef DEV_ES9028/* create proc file /proc/dac */	
	//dac_init();
	#endif

	#if DEV_VOLUME
	/* config gpio as output */
	vol_init_volume_module();
	vol_write_volume(0, true);
	vol_write_volume(0, false);
	#endif
   
    return 0;
}

static int auralic_i2c_remove(struct i2c_client *client)
{	
    print(IRNOR, "unloading auralic dac module!\n");
	return 0;
}


static const struct i2c_device_id auralic_i2c_id[] = {
	{ "es9028", 0 },
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
	{ .compatible = "fsl,es9028" },
	{ /* sentinel */ }
};

static struct i2c_driver auralic_i2c_driver = {
	.driver = {
		.name	= "auralic_iic_dac",
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
MODULE_DESCRIPTION("auralic iic driver for vega2");
MODULE_LICENSE("GPL");
