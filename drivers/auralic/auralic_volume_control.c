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
#define DEV_VOLUME			1
#define RETRY_COUNT			5
static DEFINE_MUTEX(iic_mutex);
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

extern void aura_set_rst_dac_pin(int value);
extern void aura_set_xtal_sel_pin(int value);
#if DEV_VOLUME
#define     VOL_I2C_ADDR_L   0x20    // ADDR0=0 ADDR1=0 ADDR2=0
#define     VOL_I2C_ADDR_R   0x21    // ADDR0=0 ADDR1=0 ADDR2=0
extern bool aura_iic_read_reg(unsigned short irc_addr, char reg, char *data);
extern bool aura_iic_write_reg(unsigned short irc_addr, char reg, char data);

static unsigned char global_volume_db = 255;
char vol_delay_ms = 10;
unsigned char vol_balance = 128;
static unsigned char reduce_db = 0;
bool vol_set_reduce_output_db(unsigned char db)
{
    reduce_db = db;
    return true;
}
EXPORT_SYMBOL_GPL(vol_set_reduce_output_db);
unsigned char vol_get_reduce_output_db(void)
{
    return reduce_db;
}
EXPORT_SYMBOL_GPL(vol_get_reduce_output_db); 
void vol_setting_delay(char delay)
{
	vol_delay_ms = delay;
}
EXPORT_SYMBOL_GPL(vol_setting_delay);

char vol_getting_delay(void)
{
	return vol_delay_ms;
}
EXPORT_SYMBOL_GPL(vol_getting_delay);

bool vol_read_reg(char reg, char *data, bool isleft)
{
	if(true == isleft)
		return aura_iic_read_reg(VOL_I2C_ADDR_L, reg, data);
	else
		return aura_iic_read_reg(VOL_I2C_ADDR_R, reg, data);
}
EXPORT_SYMBOL_GPL(vol_read_reg);

bool vol_write_reg(char reg, char data, bool isleft)
{
	if(true == isleft)
		return aura_iic_write_reg(VOL_I2C_ADDR_L, reg, data);
	else
		return aura_iic_write_reg(VOL_I2C_ADDR_R, reg, data);
}
EXPORT_SYMBOL_GPL(vol_write_reg);

unsigned char vol_get_vol_balance(void)
{
    return vol_balance;
}
EXPORT_SYMBOL_GPL(vol_get_vol_balance); 

bool vol_write_vol_balance(unsigned char db)
{   
    if(28 <= db && db <= 228)
        vol_balance = db;
    else
        return false;
    
    return true;
}               
EXPORT_SYMBOL_GPL(vol_write_vol_balance); 

bool vol_write_lr_db(unsigned char db)
{   
	/* write config value 
	   GPIO is input mode when chip reseted, 
	   so set it as output mode in case chip happened reset
	*/

    unsigned char db_l = db, db_r = db;

    if(128 < vol_balance && vol_balance <= 228)
    {
        if(255 > (vol_balance - 128 + db_l) )
            db_l += (vol_balance - 128);
        else
            db_l = 255;
    }
    else if(28 <= vol_balance && vol_balance < 128)
    {
        if(255 > (128 - vol_balance + db_r) )
            db_r += (128 - vol_balance);
        else
            db_r = 255;
    }

    if(db_l + reduce_db > 255) db_l = 255;
    else db_l = db_l + reduce_db;
    if(db_r + reduce_db > 255) db_r = 255;
    else db_r = db_r + reduce_db;

    print(IRDBG, "balance = %u  db_l = %u   db_r = %u!\n", vol_balance, db_l, db_r);
    
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 6, 0)
	   || false == aura_iic_write_reg(VOL_I2C_ADDR_L, 7, 0))
	{		
		print(IRNOR, "vol_write_lr_db: write left channel config value failed!\n");
		return false;
	}
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 6, 0)
	   || false == aura_iic_write_reg(VOL_I2C_ADDR_R, 7, 0))
	{		
		print(IRNOR, "vol_write_lr_db: write right channel config value failed!\n");
		return false;
	}
			
    /* write set value */
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, db_l)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 0))
	{
		print(IRNOR, "vol_write_lr_db: write left channel set value failed!\n");
		return false;
	}
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, db_r)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 0))
	{
		print(IRNOR, "vol_write_lr_db: write right channel set value failed!\n");
		return false;
	}
	
	/* delay vol_delay_ms ms */
	mdelay(vol_delay_ms);
	
    /* write rst value */
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, db_l)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 255-db_l))
	{
		print(IRNOR, "vol_write_lr_db: write left channel rst value failed!\n");
		return false;
	}
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, db_r)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 255-db_r))
	{
		print(IRNOR, "vol_write_lr_db: write right channel rst value failed!\n");
		return false;
	}
	
	/* delay vol_delay_ms ms */
	mdelay(vol_delay_ms);
	
    /* clear set value and rst value */
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, 0)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 0))
	{
		print(IRNOR, "vol_write_lr_db: clear left channel set value and rst value failed!\n");
		return false;
	}
	if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, 0)
		|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 0))
	{
		print(IRNOR, "vol_write_lr_db: clear right channel set value and rst value failed!\n");
		return false;
	}
	
	print(IRDBG, "vol_write_lr_db: write left/right set=0x%02x rst=0x%02x success!\n", db, 255-db); 	
	
	return true;
}
EXPORT_SYMBOL_GPL(vol_write_lr_db);

bool vol_write_db(unsigned char db, bool isleft)
{   
	/* write config value 
	   GPIO is input mode when chip reseted, 
	   so set it as output mode in case chip happened reset
	*/
    if(db + reduce_db > 255) db = 255;
    else db = db + reduce_db;

	if(isleft == true)
	{		
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 6, 0)
		   || false == aura_iic_write_reg(VOL_I2C_ADDR_L, 7, 0))
		{		
			print(IRNOR, "vol_write_db: write left channel config value failed!\n");
			return false;
		}
				
	    /* write set value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, db)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 0))
		{
			print(IRNOR, "vol_write_db: write left channel set value failed!\n");
			return false;
		}
		
		/* delay vol_delay_ms ms */
		mdelay(vol_delay_ms);
		
	    /* write rst value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, db)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 255-db))
		{
			print(IRNOR, "vol_write_db: write left channel rst value failed!\n");
			return false;
		}
		
		/* delay vol_delay_ms ms */
		mdelay(vol_delay_ms);
		
	    /* clear set value and rst value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_L, 2, 0)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_L, 3, 0))
		{
			print(IRNOR, "vol_write_db: clear left channel set value and rst value failed!\n");
			return false;
		}
		
		print(IRNOR, "vol_write_db: write left set=0x%02x rst=0x%02x success!\n", db, 255-db); 	
	}
	else
	{   
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 6, 0)
		   || false == aura_iic_write_reg(VOL_I2C_ADDR_R, 7, 0))
		{		
			print(IRNOR, "vol_write_db: write right channel config value failed!\n");
			return false;
		}
				
	    /* write set value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, db)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 0))
		{
			print(IRNOR, "vol_write_db: write right channel set value failed!\n");
			return false;
		}
		
		/* delay vol_delay_ms ms */
		mdelay(vol_delay_ms);
		
	    /* write rst value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, db)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 255-db))
		{
			print(IRNOR, "vol_write_db: write right channel rst value failed!\n");
			return false;
		}
		
		/* delay vol_delay_ms ms */
		mdelay(vol_delay_ms);
		
	    /* clear set value and rst value */
		if(false == aura_iic_write_reg(VOL_I2C_ADDR_R, 2, 0)
			|| false == aura_iic_write_reg(VOL_I2C_ADDR_R, 3, 0))
		{
			print(IRNOR, "vol_write_db: clear right channel set value and rst value failed!\n");
			return false;
		}	
		
		print(IRNOR, "vol_write_db: write right set=0x%02x rst=0x%02x success!\n", db, 255-db); 	
	}
	
	return true;
}
EXPORT_SYMBOL_GPL(vol_write_db);

void vol_init_volume_module(void)
{
    /* write config value 
	   GPIO is input mode when chip reseted, 
	   so set it as output mode
	*/
	aura_iic_write_reg(VOL_I2C_ADDR_R, 6, 0);
	aura_iic_write_reg(VOL_I2C_ADDR_R, 7, 0);
	
	aura_iic_write_reg(VOL_I2C_ADDR_L, 6, 0);
	aura_iic_write_reg(VOL_I2C_ADDR_L, 7, 0);
}
EXPORT_SYMBOL_GPL(vol_init_volume_module);

#if 0
bool user_value_to_hex(char value, char *hex)
{
    bool ret = true;
    char tail = 0;

    tail = 0;
    if(0 == value)
    {
        *hex = 255;
    }
    else if(10 >= value) // 1 -- 10
    {
        *hex = 160 - value * 4;// + 1;// 2 * (80 - value * 2), +1 for mode0
    }
    else if(40 >= value) // 11 -- 40
    {
        *hex = 140 - value * 2;// + 1;// 2 * (70 - value * 1), +1 for mode0
    }
    else if(100 >= value) // 41 -- 100
    {
        *hex = 100 - value;// + 1; // 2 * (50 - value*0.5), +1 for mode0
    }
    else
    {
        ret = false;
    }
        
    return ret;
}
#else
bool user_value_to_hex(char value, char *hex)
{
    bool ret = true;
    char tail = 0;

    tail = 0;
    if(25 <= value && value <= 100)
    {
        *hex = (100 - value)*1;// 100 - 25 step 0.5db
    }
    else if(10 <= value && value < 25)
    {
        *hex = 75 + (25 - value)*2;// 25 - 10 step 1db
    }
    else if(6 <= value && value < 10)
    {
        *hex = 105 + (10 - value)*4;// 10 - 6 step 2db
    }
    else if(3 <= value && value < 6)
    {
        *hex = 121 + (6 - value)*6;// 6 - 3 step 3db
    }
    else if(1 <= value && value < 3)
    {
        *hex = 139 + (3 - value)*10;// 3 - 1 step 5db
    }
    else if(0 == value)
    {
        *hex = 255;// -127.5db
    }
    else
    {
        ret = false;
    }
        
    return ret;
}
#endif

bool vol_write_volume(unsigned char vol, bool isleft)
{   
    char db;
	
	if(true == user_value_to_hex(vol, &db))
	{
        vol_write_db(db, isleft);
    }
	else
	{
        print(IRNOR, "auralic invalide volume value, %d\n", vol);
	}
    
    return true;
}
EXPORT_SYMBOL_GPL(vol_write_volume);

bool vol_write_lr_volume(unsigned char vol)
{   
    char db;
	
	if(true == user_value_to_hex(vol, &db))
	{
        vol_write_lr_db(db);
    }
	else
	{
        print(IRNOR, "auralic invalide volume value, %d\n", vol);
	}
    
    return true;
}               
EXPORT_SYMBOL_GPL(vol_write_lr_volume);    
bool restore_volume_db(void)
{
    return true;
}
EXPORT_SYMBOL_GPL(restore_volume_db);
#endif
static int __init init_vega_g2_volume_control(void)
{
    print(IRNOR,"vega2 volume control module init!\n");
	vol_init_volume_module();
	vol_write_volume(0, true);
	vol_write_volume(0, false);
    return 0;
}
static void __exit exit_vega_g2_volume_control(void)
{
    print(IRNOR,"vega2 volume control module exit!\n");
}
module_init(init_vega_g2_volume_control);
module_exit(exit_vega_g2_volume_control);


MODULE_AUTHOR("AURALiC LIMTED.");
MODULE_DESCRIPTION("auralic iic driver for vega2");
MODULE_LICENSE("GPL");
