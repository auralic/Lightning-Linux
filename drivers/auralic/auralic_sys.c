
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
#include <linux/kthread.h>

/* proc filesystem */
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mm.h>

#include "auralic_freq.h" 

#define		HAS_INPUT_PROC		1
#define		HAS_DIX9211_PROC		1
#define		HAS_SYS_MISC_PROC	1
#define		HAS_SYS_CTRL_PROC	1

input_channel_t sys_channel = INPUT_NONE_SELECT;

bool sysprt_level = false;
enum print_level_ {
    SYSNOR = 0,
    SYSDBG = 1,
    SYSBUT,
};

#define sysprt(level, format, args...)   \
do  \
{   \
    if(true == sysprt_level) \
    { \
        printk(format, ##args); \
    } \
    else if(level == SYSNOR) \
    { \
        printk(format, ##args); \
    } \
}   \
while(0)	

#if HAS_INPUT_PROC
#define		INPUT_NAME		"input"

static char *inputhelp_str = 
"echo  aes                    >  /proc/input \n" \
"echo  tos1                    >  /proc/input \n" \
"echo  tos2                    >  /proc/input \n" \
"echo  coax1                   >  /proc/input \n" \
"echo  coax2                   >  /proc/input \n" \
"echo  analog                 >  /proc/input \n" \
"echo  streaming              >  /proc/input \n" \
"echo  usb_device             >  /proc/input \n" \
"echo  lightning_link         >  /proc/input \n" \
"echo  usb_device_bypass       >  /proc/input \n" \
"echo  lightning_link_bypass  >  /proc/input \n" \
;

extern void aura_select_input_channel_for_aries(input_channel_t channel);
extern void aura_xmos_usbin_rst(void);
extern void aura_xmos_out_rst(void);
extern void aura_init_dix9211_aries(input_channel_t channel);


char *sys_channel_str[] = 
{	
	"AES",
	"TOS1",
	"TOS2",
	"COAX1",
	"COAX2",
	"ANALOG",
	"STREAMING",
	"USB_DEVICE",
	"USB_DEVICE_BYPASS",
	"LIGHTNING_LINK",
	"LIGHTNING_LINK_BYPASS",
	"INPUT_NONE_SELECT"
};

char * input_get_channel_str(input_channel_t channel)
{
	if(AES_COAX_TOS > channel)
		return sys_channel_str[channel];
	else
		return NULL;
}

ssize_t inputproc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{	 
	int len = 0;
	char buff[200] = {0};

	if(0 != *offset)
		return 0;
	
	len += sprintf(buff+len, "chanel = %s\n", input_get_channel_str(sys_channel));

	*offset = len;

	if(0 != copy_to_user(usrbuf, buff, len))
	{
		return 0;
	}

	return len;
}

static ssize_t inputproc_write(struct file *filp, const char __user *usr_buf,
							 size_t count, loff_t *f_pos)
{
	int val, reg, cnt;
	char cmd[30] = {0};
	char buff[100] = {0};

	val = count < 100 ? count : 99;
	if(0 != copy_from_user(buff, usr_buf, val))
	{
		goto out;
	}
	
	buff[99] = '\0';
	
	if(0 == (cnt=sscanf(buff, "%s %d %d", cmd, &reg, &val)))
	{
		sysprt(SYSNOR, "invalid command for gpio\n");
		goto out;
	}

	if(0 == strncmp(cmd, "aes", strlen("aes")))
	{		
		sys_channel = AES;
		aura_select_input_channel_for_aries(AES_COAX_TOS);
        aura_init_dix9211_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "tos1", strlen("tos1")))
	{		
		sys_channel = TOS1;
		aura_select_input_channel_for_aries(AES_COAX_TOS);
		aura_init_dix9211_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "tos2", strlen("tos2")))
	{		
		sys_channel = TOS2;
		aura_select_input_channel_for_aries(AES_COAX_TOS);
		aura_init_dix9211_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "coax1", strlen("coax1")))
	{		
		sys_channel = COAX1;
		aura_select_input_channel_for_aries(AES_COAX_TOS);
		aura_init_dix9211_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "coax2", strlen("coax2")))
	{		
		sys_channel = COAX2;
		aura_select_input_channel_for_aries(AES_COAX_TOS);
		aura_init_dix9211_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "analog", strlen("analog")))
	{		
		sys_channel = ANALOG;
		aura_select_input_channel_for_aries(sys_channel);
	}
	else if(0 == strncmp(cmd, "usb_device_bypass", strlen("usb_device_bypass")))
	{		
		sys_channel = USB_DEVICE_BYPASS;
		aura_select_input_channel_for_aries(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "streaming", strlen("streaming")))
	{		
		sys_channel = STREAMING;
		aura_select_input_channel_for_aries(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "usb_device", strlen("usb_device")))
	{		
		sys_channel = USB_DEVICE;
		aura_select_input_channel_for_aries(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "lightning_link_bypass", strlen("lightning_link_bypass")))
	{		
		sys_channel = LIGHTNING_LINK_BYPASS;
		aura_select_input_channel_for_aries(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "lightning_link", strlen("lightning_link")))
	{		
		sys_channel = LIGHTNING_LINK;
		aura_select_input_channel_for_aries(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else
	{
		sysprt(SYSNOR, "%s", inputhelp_str);
	}	
	
out:	
	return count;
}

static const struct  proc_ops input_proc_op = {
	.proc_read  = inputproc_read,
	.proc_write = inputproc_write,
};
#endif

#if HAS_DIX9211_PROC
#define		DIX9211_NAME		"dix9211"

static char *dix9211help_str = 
"echo read  x     >  /proc/dix9211 --> read reg x\n" \
"echo write x  y  >  /proc/dix9211 --> write reg x = y\n" \
;
extern bool dix9211_read_reg(char reg, char *data);
extern bool dix9211_write_reg(char reg, char data);
ssize_t dix9211proc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{	 
	int len = 0;
	char buff[200] = {0};

	if(0 != *offset)
		return 0;

	*offset = len;

	if(0 != copy_to_user(usrbuf, buff, len))
	{
		return 0;
	}

	return len;
}

static ssize_t dix9211proc_write(struct file *filp, const char __user *usr_buf,
							 size_t count, loff_t *f_pos)
{
	int val, reg, cnt;
	char cmd[30] = {0};
	char buff[100] = {0};

	val = count < 100 ? count : 99;
	if(0 != copy_from_user(buff, usr_buf, val))
	{
		goto out;
	}
	
	buff[99] = '\0';
	
	if(0 == (cnt=sscanf(buff, "%s %d %d", cmd, &reg, &val)))
	{
		sysprt(SYSNOR, "invalid command for gpio\n");
		goto out;
	}

	if(0 == strncmp(cmd, "read", strlen("read")) && cnt == 2)
	{		
		char tmp = 0xff;
		if(true == dix9211_read_reg(reg, &tmp))
		{
			sysprt(SYSNOR, "dix9211 read reg[%0xd] = 0x%02x!\n", reg, tmp);
		}
	}
	else if(0 == strncmp(cmd, "write", strlen("write")) && cnt == 3)
	{		
		if(true == dix9211_write_reg(reg, val))
		{
			sysprt(SYSNOR, "dix9211 write reg[%0xd] = 0x%02x!\n", reg, val);
		}
	}
    /*
    else if(0 == strncmp(cmd, "AES", strlen("AES")) && cnt == 1)
	{		
		char tmp = 0xff;
        input_channel_t channel = AES;
        aura_select_input_channel(AES_COAX_TOS);
		aura_init_dix9211(channel);
		sysprt(SYSNOR, "dix9211 set input channel to AES\n");
	}
    else if(0 == strncmp(cmd, "COAX", strlen("COAX")) && cnt == 1)
	{		
		char tmp = 0xff;
        input_channel_t channel = COAX;
        aura_select_input_channel(AES_COAX_TOS);
		aura_init_dix9211(channel);
		{
			sysprt(SYSNOR, "dix9211 set input channel to COAX\n");
		}
	}
    else if(0 == strncmp(cmd, "TOS", strlen("TOS")) && cnt == 1)
	{		
		char tmp = 0xff;
        input_channel_t channel = TOS;
        aura_select_input_channel(AES_COAX_TOS);
		aura_init_dix9211(channel);
		{
			sysprt(SYSNOR, "dix9211 set input channel to TOSLINK\n");
		}
	}
    */
	else
	{
		sysprt(SYSNOR, "%s", dix9211help_str);
	}	
	
out:	
	return count;
}

static const struct  proc_ops dix9211_proc_op = {
	.proc_read  = dix9211proc_read,
	.proc_write = dix9211proc_write,
};
#endif

#if HAS_SYS_MISC_PROC
#define		SYS_MISC_NAME		"sys_misc"

static char *mischelp_str = 
"echo rst_dac        x  >  /proc/sys_misc set the rst pin of dac\n" \
"echo xtal_sel       x  >  /proc/sys_misc \n" \
"echo usb_sel       x  >  /proc/sys_misc \n" \
"echo xmos_usbin_rst    >  /proc/sys_misc \n";


ssize_t miscproc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{   
    return 0;
    /*
    int len = 0;
    char buff[200] = {0};

    if(0 != *offset)
        return 0;
    
    len += sprintf(buff+len, "poll_delay = %d ms\n", get_polling_delay_ms());

    *offset = len;

    if(0 != copy_to_user(usrbuf, buff, len))
    {
        return 0;
    }

    return len;
    */
}

static ssize_t miscproc_write(struct file *filp, const char __user *usr_buf,
                             size_t count, loff_t *f_pos)
{
    int val, reg, cnt;
	char cmd[30] = {0};
    char buff[100] = {0};

    val = count < 100 ? count : 99;
    if(0 != copy_from_user(buff, usr_buf, val))
    {
        goto out;
    }
    
    buff[99] = '\0';
    
    if(0 == (cnt=sscanf(buff, "%s %d %d", cmd, &reg, &val)))
    {
    	sysprt(SYSNOR, "invalid command for gpio\n");
    	goto out;
    }

	if(0 == strncmp(cmd, "rst_dac", strlen("rst_dac")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "set rst_dac = %d\n", reg);
	//	aura_set_rst_dac_pin(reg);
	}
	else if(0 == strncmp(cmd, "xtal_sel", strlen("xtal_sel")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "set xtal_sel_pin = %d\n", reg);
	//	aura_set_xtal_sel_pin(reg);
	}
	else if(0 == strncmp(cmd, "usb_sel", strlen("usb_sel")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "set usb_sel_pin = %d\n", reg);
	//	aura_set_usb_sel_pin(reg);
	}
	else if(0 == strncmp(cmd, "xmos_usbin_rst", strlen("xmos_usbin_rst")))
	{		
    	sysprt(SYSNOR, "call xmos_usbin_rst by proc\n");
	//	aura_xmos_usbin_rst();
	}
	else
	{
		sysprt(SYSNOR, "%s", mischelp_str);
	}	
    
out:    
    return count;
}


static const struct  proc_ops sysmisc_proc_op = {
    .proc_read  = miscproc_read,
    .proc_write = miscproc_write,
};
#endif

#if HAS_SYS_CTRL_PROC
#define		SYS_CTRL_NAME		"sys_ctrl"

static char *syshelp_str = 
"echo dbgon          >  /proc/sys_ctrl\n" \
"echo dbgoff         >  /proc/sys_ctrl\n" \
"echo  link_sw       x  >  /proc/sys_ctrl\n" \
"echo  dir_rst       x  >  /proc/sys_ctrl\n" \
"echo  dit_rst       x  >  /proc/sys_ctrl\n" \
"echo  select_44_48       x  >  /proc/sys_ctrl\n" \
"echo  usb2_vbus_sw       x  >  /proc/sys_ctrl\n" \
"echo  mute_sw       x  >  /proc/sys_ctrl\n" \
"echo  ani_sw       x  >  /proc/sys_ctrl\n" \
"echo  phono_sw       x  >  /proc/sys_ctrl\n" \
"echo  xmos_rst       x  >  /proc/sys_ctrl\n" \
"echo  src_int0       x  >  /proc/sys_ctrl\n" \
"echo  sleep_sw       x  >  /proc/sys_ctrl\n" \
"echo  mclk_sel       x  >  /proc/sys_ctrl\n" \
//"echo  plug_link_up       x  >  /proc/sys_ctrl\n" 
"echo  ready_link2_down       x  >  /proc/sys_ctrl\n" \
"echo  ready_link_down        x  >  /proc/sys_ctrl\n";

extern void aura_set_llink_sw_pin(int value);
extern void aura_set_dir_rst_pin(int value);
extern void aura_set_dit_rst_pin(int value);
extern void aura_set_44_48_sw_pin(int value);
extern void aura_set_usb2_vbus_sw_pin(int value);
extern void aura_set_mute_sw_pin(int value);
extern void aura_set_ani_sw_pin(int value);
extern void aura_set_phono_sw_pin(int value);
extern void aura_set_xmos_rst_pin(int value);
extern void aura_set_display0_rst_pin(int value);
extern void aura_set_lvds_rst_pin(int value);
extern void aura_set_src_int0_pin(int value);
extern void aura_set_plug_link_up_pin(int value);
extern void aura_set_ready_link_down_pin(int value);
extern void aura_set_sleep_sw_pin(int value);

ssize_t sysproc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{    
    int len = 0;
    char buff[200] = {0};

    if(0 != *offset)
        return 0;	
		
	len += sprintf(buff+len, "debug = %s\n", true == sysprt_level ? "on":"off");

    *offset = len;

    if(0 != copy_to_user(usrbuf, buff, len))
    {
        return 0;
    }

    return len;
}

static ssize_t sysproc_write(struct file *filp, const char __user *usr_buf,
                             size_t count, loff_t *f_pos)
{
    int val, reg, cnt;
	char cmd[30] = {0};
    char buff[100] = {0};

    val = count < 100 ? count : 99;
    if(0 != copy_from_user(buff, usr_buf, val))
    {
        goto out;
    }
    
    buff[99] = '\0';
    
    if(0 == (cnt=sscanf(buff, "%s %d %d", cmd, &reg, &val)))
    {
    	sysprt(SYSNOR, "invalid command for gpio\n");
    	goto out;
    }

	if(0 == strncmp(cmd, "dbgon", strlen("dbgon")))
	{		
    	sysprt(SYSNOR, "debug = on!\n");
		sysprt_level = true;
	}
	else if(0 == strncmp(cmd, "dbgoff", strlen("dbgoff")))
	{		
    	sysprt(SYSNOR, "debug = off!\n");
		sysprt_level = false;
	}
    else if(0 == strncmp(cmd, "llink_sw", strlen("llink_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "llink_sw = %d!\n", reg);
        aura_set_llink_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "dir_rst", strlen("dir_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "dir_rst = %d!\n", reg);
        aura_set_dir_rst_pin(reg);
	}
    else if(0 == strncmp(cmd, "dit_rst", strlen("dit_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "dit_rst = %d!\n", reg);
        aura_set_dit_rst_pin(reg);
	}
    else if(0 == strncmp(cmd, "select_44_48", strlen("select_44_48")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "select_44_48 = %d!\n", reg);
        aura_set_44_48_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "usb2_vbus_sw", strlen("usb2_vbus_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "usb2_vbus_sw = %d!\n", reg);
        aura_set_usb2_vbus_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "mclk_sel", strlen("mclk_sel")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "mclk_sel = %d!\n", reg);
        if(reg == 0)
            aura_set_usb2_vbus_sw_pin(1);
        else
            aura_set_usb2_vbus_sw_pin(0);
	}
    else if(0 == strncmp(cmd, "mute_sw", strlen("mute_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "mute_sw = %d!\n", reg);
        aura_set_mute_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "ani_sw", strlen("ani_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "ani_sw = %d!\n", reg);
        aura_set_ani_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "phono_sw", strlen("phono_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "phono_sw = %d!\n", reg);
        aura_set_phono_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "xmos_rst", strlen("xmos_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "xmos_rst = %d!\n", reg);
        aura_set_xmos_rst_pin(reg);
	}
    else if(0 == strncmp(cmd, "display0_rst", strlen("display0_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "display0_rst = %d!\n", reg);
        aura_set_display0_rst_pin(reg);
	}
    else if(0 == strncmp(cmd, "lvds_rst", strlen("lvds_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "lvds_rst = %d!\n", reg);
        aura_set_lvds_rst_pin(reg);
	}
    else if(0 == strncmp(cmd, "src_int0", strlen("src_int0")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "src_int0 = %d!\n", reg);
        aura_set_src_int0_pin(reg);
	}
    /*
    else if(0 == strncmp(cmd, "plug_link_up", strlen("plug_link_up")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "plug_link_up = %d!\n", reg);
        aura_set_plug_link_up_pin(reg);
	}
    */
    else if(0 == strncmp(cmd, "ready_link_down", strlen("ready_link_down")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "ready_link_down = %d!\n", reg);
        aura_set_ready_link_down_pin(reg);
	}
    else if(0 == strncmp(cmd, "ready_link2_down", strlen("ready_link2_down")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "ready_link2_down = %d!\n", reg);
        aura_set_plug_link_up_pin(reg);
	}
    else if(0 == strncmp(cmd, "sleep_sw", strlen("sleep_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "sleep_sw = %d!\n", reg);
        aura_set_sleep_sw_pin(reg);
	}
	else
	{
		sysprt(SYSNOR, "%s", syshelp_str);
	}	
    
out:    
    return count;
}


static const struct  proc_ops sysctrl_proc_op = {
    .proc_read  = sysproc_read,
    .proc_write = sysproc_write,
};
#endif


static struct task_struct *sys_task = NULL;
/*
int sys_task_fn(void *data)
{	
	unsigned int freq_new = 0, freq_last = 0;
	
    sysprt(SYSNOR, "auralic start sys_task_fn\n");
    
    while(!kthread_should_stop())
    {
		msleep(20);
		
        freq_new = aura_get_sysfreq_imdiat();
		if(freq_last == freq_new)
			continue;
		
		freq_last = freq_new;

    }
    
    set_current_state(TASK_RUNNING);
	
    sysprt(SYSNOR, "auralic leave sys_task_fn\n");
    
    return 0;
}
*/

static int __init auralic_gpio_init(void)
{    
    sysprt(SYSNOR, "loading auralic altair g1 sys module!\n");    
/*
	sys_task = kthread_run(sys_task_fn, NULL, "sys_task");
    if(IS_ERR(sys_task))
    {
        sys_task = NULL;
        pr_err("auralic create sys task thread failed!\n");
        return -ENOMEM;
    }
*/
	
    #if HAS_INPUT_PROC
	if (NULL == proc_create(INPUT_NAME, 0755, NULL, &input_proc_op))	 
	{
		sysprt(SYSNOR, "auralic create /proc/%s failed!", INPUT_NAME);
		return -1;
	}
	#endif

	#if HAS_DIX9211_PROC
	if (NULL == proc_create(DIX9211_NAME, 0755, NULL, &dix9211_proc_op))	 
	{
		sysprt(SYSNOR, "auralic create /proc/%s failed!", DIX9211_NAME);
		return -1;
	}
	#endif

	#if HAS_SYS_MISC_PROC
	if (NULL == proc_create(SYS_MISC_NAME, 0755, NULL, &sysmisc_proc_op))    
    {
        sysprt(SYSNOR, "auralic create /proc/%s failed!", SYS_MISC_NAME);
        return -1;
    }
	#endif

	#if HAS_SYS_CTRL_PROC
	if (NULL == proc_create(SYS_CTRL_NAME, 0755, NULL, &sysctrl_proc_op))    
    {
        sysprt(SYSNOR, "auralic create /proc/%s failed!", SYS_CTRL_NAME);
        return -1;
    }
	#endif

	return 0;
}

static void __exit auralic_gpio_exit(void)
{
    sysprt(SYSNOR, "unloading altair g1 sys module!\n");
		
    if(NULL != sys_task)
    {
        kthread_stop(sys_task);        
        wake_up_process(sys_task);
        sys_task = NULL;
    }
	#if HAS_INPUT_PROC
	remove_proc_entry(INPUT_NAME, NULL);
	#endif
	
	#if HAS_DIX9211_PROC
	remove_proc_entry(DIX9211_NAME, NULL);
	#endif
	
	#if HAS_SYS_MISC_PROC
	remove_proc_entry(SYS_MISC_NAME, NULL);
	#endif
	
	#if HAS_SYS_CTRL_PROC
	remove_proc_entry(SYS_CTRL_NAME, NULL);
	#endif
}

module_init(auralic_gpio_init);
module_exit(auralic_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AURALiC 2019-03-28");
MODULE_DESCRIPTION("gpio driver for altair g1");
