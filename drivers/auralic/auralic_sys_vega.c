
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

#define		HAS_DAC_PROC		1
#define		HAS_VOL_PROC		1
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

#if HAS_DAC_PROC
#define		DAC_NAME		"dac"

static char *dachelp_str = 
"echo init   x    >  /proc/dac --> init dac with sampling rate\n" \
"echo rst_hard    >  /proc/dac --> reset dac by gpio\n" \
"echo rst_soft    >  /proc/dac --> reset dac by register\n" \
"echo freq   x    >  /proc/dac --> set dac's sampling rate\n" \
"echo mode   x    >  /proc/dac --> set dac's mode\n" \
"echo /x/y/       >  /proc/dac --> set the directory of mode files\n" \
"echo read   x    >  /proc/dac --> read dac's register x\n" \
"echo write  x  y >  /proc/dac --> write dac's register x with value y\n" \
"echo mute_dac  x >  /proc/dac --> x=1: mute dac,  x=0: umute dac\n" \
"echo xmos        >  /proc/dac --> read xmos usbin i2c 4 bytes\n" \
"echo auto_mute x >  /proc/dac --> auto mute dac when usbin close\n" \
;
extern bool restore_volume_db(void);
extern bool is_multiple_sample_rate(unsigned int samfreq);
extern void dac_init(unsigned int samfreq);
extern void dac_soft_reset(void);
extern void dac_hard_reset(void);
extern bool dac_set_samfreq(unsigned int samfreq);
extern unsigned int dac_get_samfreq(void);
extern bool dac_read_reg(char reg, char *data);
extern bool dac_write_reg(char reg, char data);
extern void aura_dac_mute(void);
extern void aura_dac_umute(void);
extern bool dac_getting_mute_state(void);
extern void dac_setting_mode(int mode);
extern int dac_getting_mode(void);
extern char * dac_getting_path(void);
extern void dac_setting_path(char *path, int length);
extern void dac_setting_coefficient(int mode, int fsl);

extern void set_ready_to_link_down(int val);
extern void set_plug_to_link_up(int val);
extern int get_ready_to_link_down(void);
extern int get_ready_from_link_up(void);
extern int get_plug_to_link_up(void);
extern int get_plug_from_link_down(void);
extern void read_xmos_usbin_i2c(void);
extern void usbin_set_dac_auto_mute(bool ismute);
extern bool usbin_get_dac_auto_mute(void);


ssize_t dacproc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{	 
	int len = 0;
	char buff[200] = {0};

	if(0 != *offset)
		return 0;

	len += sprintf(buff+len, "mode    = %d\n", dac_getting_mode());
	len += sprintf(buff+len, "mute    = %s\n", true == dac_getting_mute_state() ? "yes":"no");
	len += sprintf(buff+len, "samfreq = %d\n", dac_get_samfreq());
	len += sprintf(buff+len, "path    = [%s]\n", dac_getting_path());
	len += sprintf(buff+len, "readyfrom_up   = %d\n", get_ready_from_link_up());
	len += sprintf(buff+len, "plugto_up      = %d\n", get_plug_to_link_up());
	len += sprintf(buff+len, "readyto_down   = %d\n", get_ready_to_link_down());
	len += sprintf(buff+len, "plugfrom_down  = %d\n", get_plug_from_link_down());
	len += sprintf(buff+len, "auto_mute      = %s\n", true == usbin_get_dac_auto_mute() ? "yes":"no");

	*offset = len;

	if(0 != copy_to_user(usrbuf, buff, len))
	{
		return 0;
	}

	return len;
}

static ssize_t dacproc_write(struct file *filp, const char __user *usr_buf,
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

	if(0 == strncmp(cmd, "init", strlen("init")) && cnt == 2)
	{		
		sysprt(SYSNOR, "dac init sampfreq = %d by proc!\n", reg);
		dac_init(reg);
        restore_volume_db();
	}
	else if(0 == strncmp(cmd, "xmos", strlen("xmos")))
	{		
		read_xmos_usbin_i2c();
	}
	else if(0 == strncmp(cmd, "rst_hard", strlen("rst_hard")))
	{		
		dac_hard_reset();
		sysprt(SYSNOR, "dac hard reset by proc!\n");
	}
	else if(0 == strncmp(cmd, "rst_soft", strlen("rst_soft")))
	{		
		dac_soft_reset();
		sysprt(SYSNOR, "dac soft reset by proc!\n");
	}
	else if(0 == strncmp(cmd, "freq", strlen("freq")) && cnt == 2)
	{		
		sysprt(SYSNOR, "dac set sampfreq = %d by proc!\n", reg);
		bool should_restore_volume = is_multiple_sample_rate(reg);
        dac_set_samfreq(reg);
        if(should_restore_volume)
        {
            sysprt(SYSNOR,"restore volume....\n");
            restore_volume_db();
        }
	}
    else if('/' == cmd[0] && cnt == 1)
    {
		sysprt(SYSNOR, "dac set path = %s by proc!\n", cmd);
		dac_setting_path(cmd, count);
    }
	else if(0 == strncmp(cmd, "auto_mute", strlen("auto_mute")) && cnt == 2)
	{	
	    if(reg)
        {   
            sysprt(SYSNOR, "set auto_mute = yes!\n");
            usbin_set_dac_auto_mute(true);
        }
        else
        {   
            sysprt(SYSNOR, "set auto_mute = no!\n");
            usbin_set_dac_auto_mute(false);
        }
	}
	else if(0 == strncmp(cmd, "mode", strlen("mode")) && cnt == 2)
	{		
		sysprt(SYSNOR, "dac set mode = %d by proc!\n", reg);
		dac_setting_mode(reg);		
		dac_setting_coefficient(reg, dac_get_samfreq());
		bool should_restore_volume = is_multiple_sample_rate(dac_get_samfreq());
        dac_set_samfreq(dac_get_samfreq());
        if(should_restore_volume)
        {
            restore_volume_db();
        }
	}
	else if(0 == strncmp(cmd, "readyto", strlen("readyto")) && cnt == 2)
	{	
	    set_ready_to_link_down(reg);
        sysprt(SYSNOR, "set readyto = %d!\n", reg);
	}
	else if(0 == strncmp(cmd, "plugto", strlen("plugto")) && cnt == 2)
	{	
	    set_plug_to_link_up(reg);
        sysprt(SYSNOR, "set plugto = %d!\n", reg);
	}
	else if(0 == strncmp(cmd, "read", strlen("read")) && cnt == 2)
	{		
		char tmp = 0xff;
		if(true == dac_read_reg(reg, &tmp))
		{
			sysprt(SYSNOR, "dac read reg[%02d] = 0x%02x!\n", reg, tmp);
		}
	}
	else if(0 == strncmp(cmd, "write", strlen("write")) && cnt == 3)
	{			
		if(true == dac_write_reg(reg, val))
		{
			sysprt(SYSNOR, "dac write reg[%02d] = 0x%02x!\n", reg, val);
		}
	}
	else if(0 == strncmp(cmd, "mute_dac", strlen("mute_dac")) && cnt == 2)
	{		
		if(0 == reg)
		{
			aura_dac_umute();
			sysprt(SYSNOR, "aura_umute_dac by proc!\n");
		}
		else
		{
			aura_dac_mute();
			sysprt(SYSNOR, "aura_mute_dac by proc!\n");
		}			
	}
	else
	{
		sysprt(SYSNOR, "%s", dachelp_str);
	}	
	
out:	
	return count;
}

static const struct  proc_ops dac_proc_op = {
	.proc_read  = dacproc_read,
	.proc_write = dacproc_write,
};
#endif

#if HAS_VOL_PROC
#define		VOL_NAME		"vol"
bool last_is_volume = true;
unsigned char last_db = 0;
unsigned char last_volume = 0;
static char *volhelp_str = 
"echo db     x  >  /proc/vol --> set left/right db with value x\n" \
"echo db_l   x  >  /proc/vol --> set left db with value x\n" \
"echo db_r   x  >  /proc/vol --> set right db with value x\n" \
"echo vol_balance   x  >  /proc/vol --> set left/right volume balance value x (28<= x <=228)\n" \
"echo vol    x  >  /proc/vol --> set left/right volume with value x\n" \
"echo vol_l  x  >  /proc/vol --> set left volume with value x\n" \
"echo vol_r  x  >  /proc/vol --> set right volume with value x\n" \
"echo cfg    x  >  /proc/vol --> set configure rister with value x\n" \
"echo read_l x  >  /proc/vol --> read left channel's reg x\n" \
"echo read_r x  >  /proc/vol --> read right channel's reg x\n" \
"echo delay  x  >  /proc/vol --> set delay = x for writing db\n"
"echo reduce_output  x  >  /proc/vol --> reduce output level by x db\n"
;

extern char vol_getting_delay(void);
extern void vol_setting_delay(char);
extern bool vol_read_reg(char reg, char *data, bool isleft);
extern bool vol_write_reg(char reg, char data, bool isleft);
extern bool vol_write_lr_volume(unsigned char vol);
extern bool vol_write_volume(unsigned char vol, bool isleft);
extern bool vol_write_lr_db(unsigned char db);
extern bool vol_write_db(unsigned char db, bool isleft);
extern bool vol_write_vol_balance(unsigned char db);
extern unsigned char vol_get_vol_balance(void);
extern bool vol_set_reduce_output_db(unsigned char reduce_db);
extern unsigned char vol_get_reduce_output_db(void);
ssize_t volproc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{	 
	int len = 0;
	char buff[200] = {0};

	if(0 != *offset)
		return 0;
    
    len += sprintf(buff+len, "vol = %u\n", last_volume);
    len += sprintf(buff+len, "vol_balance = %u\n", vol_get_vol_balance());
    len += sprintf(buff+len, "reduce_output = %u\n", vol_get_reduce_output_db());

	*offset = len;

	if(0 != copy_to_user(usrbuf, buff, len))
	{
		return 0;
	}

	return len;
}

static ssize_t volproc_write(struct file *filp, const char __user *usr_buf,
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

	if(0 == strncmp(cmd, "db_l", strlen("db_l")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting db_l = %d by proc!\n", reg);
		vol_write_db(reg, true);
	}
	else if(0 == strncmp(cmd, "db_r", strlen("db_r")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting db_r = %d by proc!\n", reg);
		vol_write_db(reg, false);
	}
	else if(0 == strncmp(cmd, "db", strlen("db")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting db = %d by proc!\n", reg);
		vol_write_lr_db(reg);
        last_db = reg;
        last_is_volume = false;
	}
	else if(0 == strncmp(cmd, "vol_balance", strlen("vol_balance")) && cnt == 2)
	{		
	    if(28<= reg && reg <= 228)
        {   
		    sysprt(SYSNOR, "setting vol_balance = %d by proc!\n", reg);
		    vol_write_vol_balance(reg);
            if(true == last_is_volume)
                vol_write_lr_volume(last_volume);
            else
                vol_write_lr_db(last_db);
        }
        else
        {
		    sysprt(SYSNOR, "invalide vol_balance %d value!\n", reg);
        }
	}
	else if(0 == strncmp(cmd, "vol_l", strlen("vol_l")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting vol_l = %d by proc!\n", reg);
		vol_write_volume(reg, true);
	}
	else if(0 == strncmp(cmd, "vol_r", strlen("vol_r")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting vol_r = %d by proc!\n", reg);
		vol_write_volume(reg, false);
	}
	else if(0 == strncmp(cmd, "vol", strlen("vol")) && cnt == 2)
	{		
		sysprt(SYSNOR, "setting volume = %d by proc!\n", reg);
		vol_write_lr_volume(reg);
        last_volume = reg;
        last_is_volume = true;
	}
	else if(0 == strncmp(cmd, "cfg", strlen("cfg")) && cnt == 2)
	{	
        /*
		vol_write_reg(6, reg, true);
		vol_write_reg(6, reg, false);
		vol_write_reg(7, reg, true);
		vol_write_reg(7, reg, false);
		sysprt(SYSNOR, "cfg = 0x%02x!\n", reg);
        */
	}
	else if(0 == strncmp(cmd, "delay", strlen("delay")) && cnt == 2)
	{				
		vol_setting_delay(reg);
		sysprt(SYSNOR, "seting vol delay=%d by proc!\n", reg);
	}
	else if(0 == strncmp(cmd, "reduce_output", strlen("reduce_output")) && cnt == 2)
	{
		vol_set_reduce_output_db(reg);
         sysprt(SYSNOR, "reduce output of %d db by proc!\n", reg);
	}
	else if(0 == strncmp(cmd, "read_l", strlen("read_l")) && cnt == 2)
	{		
		char tmp = 0xff;
		if(true == vol_read_reg(reg, &tmp, true))
		{
			sysprt(SYSNOR, "vol read_l reg[%02d] = 0x%02x!\n", reg, tmp);
		}
	}
	else if(0 == strncmp(cmd, "read_r", strlen("read_r")) && cnt == 2)
	{			
		char tmp = 0xff;
		if(true == vol_read_reg(reg, &tmp, false))
		{
			sysprt(SYSNOR, "vol read_r reg[%02d] = 0x%02x!\n", reg, tmp);
		}
	}
	else
	{
		sysprt(SYSNOR, "%s", volhelp_str);
	}	
	
out:	
	return count;
}

static const struct  proc_ops vol_proc_op = {
	.proc_read  = volproc_read,
	.proc_write = volproc_write,
};
#endif
#if HAS_INPUT_PROC
#define		INPUT_NAME		"input"

static char *inputhelp_str = 
"echo  aes                    >  /proc/input \n" \
"echo  tos1                    >  /proc/input \n" \
"echo  coax1                   >  /proc/input \n" \
"echo  analog                 >  /proc/input \n" \
"echo  streaming              >  /proc/input \n" \
"echo  usb_device             >  /proc/input \n" \
"echo  lightning_link         >  /proc/input \n" \
"echo  usb_device_bypass       >  /proc/input \n" \
"echo  lightning_link_bypass  >  /proc/input \n" \
;

extern void aura_select_input_channel_for_vega(input_channel_t channel);
extern void aura_xmos_usbin_rst(void);
extern void aura_xmos_out_rst(void);
extern void aura_init_dix9211_vega(input_channel_t channel);
extern void reset_sys_freq(void);

char *sys_channel_str[] = 
{	
	"AES",
	"TOS1",
	"COAX1",
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
        reset_sys_freq();
		aura_select_input_channel_for_vega(AES_COAX_TOS);
        aura_init_dix9211_vega(sys_channel);
	}
	else if(0 == strncmp(cmd, "tos1", strlen("tos1")))
	{		
		sys_channel = TOS1;
        reset_sys_freq();
		aura_select_input_channel_for_vega(AES_COAX_TOS);
		aura_init_dix9211_vega(sys_channel);
	}
	else if(0 == strncmp(cmd, "coax1", strlen("coax1")))
	{		
		sys_channel = COAX1;
        reset_sys_freq();
		aura_select_input_channel_for_vega(AES_COAX_TOS);
		aura_init_dix9211_vega(sys_channel);
	}
	else if(0 == strncmp(cmd, "analog", strlen("analog")))
	{		
		sys_channel = ANALOG;
		aura_select_input_channel_for_vega(sys_channel);
	}
	else if(0 == strncmp(cmd, "usb_device_bypass", strlen("usb_device_bypass")))
	{		
		sys_channel = USB_DEVICE_BYPASS;
		aura_select_input_channel_for_vega(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "streaming", strlen("streaming")))
	{		
		sys_channel = STREAMING;
		aura_select_input_channel_for_vega(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "usb_device", strlen("usb_device")))
	{		
		sys_channel = USB_DEVICE;
		aura_select_input_channel_for_vega(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "lightning_link_bypass", strlen("lightning_link_bypass")))
	{		
		sys_channel = LIGHTNING_LINK_BYPASS;
		aura_select_input_channel_for_vega(sys_channel);
		//dac_init(44100);
        //restore_volume_db();
	}
	else if(0 == strncmp(cmd, "lightning_link", strlen("lightning_link")))
	{		
		sys_channel = LIGHTNING_LINK;
		aura_select_input_channel_for_vega(sys_channel);
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
        aura_select_input_channel_for_vega(AES_COAX_TOS);
		aura_init_dix9211(channel);
		sysprt(SYSNOR, "dix9211 set input channel to AES\n");
	}
    else if(0 == strncmp(cmd, "COAX", strlen("COAX")) && cnt == 1)
	{		
		char tmp = 0xff;
        input_channel_t channel = COAX;
        aura_select_input_channel_for_vega(AES_COAX_TOS);
		aura_init_dix9211(channel);
		{
			sysprt(SYSNOR, "dix9211 set input channel to COAX\n");
		}
	}
    else if(0 == strncmp(cmd, "TOS", strlen("TOS")) && cnt == 1)
	{		
		char tmp = 0xff;
        input_channel_t channel = TOS;
        aura_select_input_channel_for_vega(AES_COAX_TOS);
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
"echo  hpd_rst       x  >  /proc/sys_ctrl\n" \
"echo  plug_link_up       x  >  /proc/sys_ctrl\n" \
"echo  enable_output       x  >  /proc/sys_ctrl\n" \
"echo  mclk_sel       x  >  /proc/sys_ctrl\n" \
"echo  rst_dac       x  >  /proc/sys_ctrl\n" \
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
extern void aura_set_hpd_rst_pin(int value);

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
    else if(0 == strncmp(cmd, "rst_dac", strlen("rst_dac")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "rst_dac = %d!\n", reg);
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
    else if(0 == strncmp(cmd, "enable_output", strlen("enable_output")) && cnt == 2)
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
    else if(0 == strncmp(cmd, "plug_link_up", strlen("plug_link_up")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "plug_link_up = %d!\n", reg);
        aura_set_plug_link_up_pin(reg);
	}
    else if(0 == strncmp(cmd, "ready_link_down", strlen("ready_link_down")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "ready_link_down = %d!\n", reg);
        aura_set_ready_link_down_pin(reg);
	}
    else if(0 == strncmp(cmd, "sleep_sw", strlen("sleep_sw")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "sleep_sw = %d!\n", reg);
        aura_set_sleep_sw_pin(reg);
	}
    else if(0 == strncmp(cmd, "hpd_rst", strlen("hpd_rst")) && cnt == 2)
	{		
    	sysprt(SYSNOR, "hpd_rst = %d!\n", reg);
        aura_set_hpd_rst_pin(reg);
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
	#if HAS_DAC_PROC
	if (NULL == proc_create(DAC_NAME, 0755, NULL, &dac_proc_op))	 
	{
		sysprt(SYSNOR, "auralic create /proc/%s failed!", DAC_NAME);
		return -1;
	}
	#endif
	#if HAS_VOL_PROC
	if (NULL == proc_create(VOL_NAME, 0755, NULL, &vol_proc_op))	 
	{
		sysprt(SYSNOR, "auralic create /proc/%s failed!", VOL_NAME);
		return -1;
	}
	#endif
	
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
	#if HAS_DAC_PROC
	remove_proc_entry(DAC_NAME, NULL);
	#endif
	
	#if HAS_VOL_PROC
	remove_proc_entry(VOL_NAME, NULL);
	#endif
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
