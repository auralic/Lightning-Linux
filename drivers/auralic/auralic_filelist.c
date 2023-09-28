
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

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/ctype.h>

#include <linux/auralic_filelist.h>


#define         FILELIST_PROC_NAME       "filelist"
#define         FILELIST_NAME_TMP        "/media/nvme0n1p1/list.bin"
#define         AURA_WRITE_INFO_NUM      50 
#define         AURA_WRITE_INFO_NUM_TMP  50 
#define         AURA_WRITE_INFO_NUM_WRITE  50 

char MATCH_PATH_STR[20]={"/media/nvme"};
bool has_newlist = false;
bool is_stoped = true;
bool need_stop = false; // need stop by shell cmd: echo stop > /proc/filelist
bool need_init = true;
bool vfs_can_access = false;
atomic_t info_count;

raw_spinlock_t filelist_lock;
struct list_head filelist_event;
struct task_struct *filelist_task = NULL;

raw_spinlock_t info_lock;
raw_spinlock_t info_lock_tmp;
raw_spinlock_t info_lock_write;

struct aura_write_info aura_info[AURA_WRITE_INFO_NUM];
struct aura_write_info aura_info_tmp[AURA_WRITE_INFO_NUM_TMP];
struct aura_write_info aura_info_write[AURA_WRITE_INFO_NUM_WRITE];

struct kmem_cache *filelist_cache = NULL;


char *helpstring = "echo start     > /proc/filelist --> start writing file change list\n"
                   "echo stop      > /proc/filelist --> stop writing file change list\n"
                   "echo formate   > /proc/filelist --> stop write to list.bin befor formate disk hd\n"
                   "echo /media/xy > /proc/filelist --> set /media/xy as new match path\n"
                   ;

void aura_info_timeout_handle(struct timer_list *t);


bool init_aura_info(void)
{
    int i;
    struct page *page = NULL;

    atomic_set(&info_count, 0);
    raw_spin_lock_init(&info_lock);
    raw_spin_lock_init(&info_lock_tmp);
    raw_spin_lock_init(&info_lock_write);
    
    for(i=0; i<AURA_WRITE_INFO_NUM; i++)
    {
        aura_info[i].path = NULL;
        aura_info[i].stat = INFO_IDLE;
        //init_timer(&aura_info[i].timer);
    	timer_setup(&aura_info[i].timer, aura_info_timeout_handle, 0);
        page = alloc_page(GFP_KERNEL);
        if(NULL == page)
        {
            printk("filelist alloc pages failed!\n");
            return false;
        }

        aura_info[i].path = NULL;
        aura_info[i].buff = page_address(page);
        if(NULL == aura_info[i].buff)
        {
            printk("filelist address page failed!\n");
            return false;
        }
    }

    for(i=0; i<AURA_WRITE_INFO_NUM_TMP; i++)
    {
        page = alloc_page(GFP_KERNEL);
        if(NULL == page)
        {
            printk("filelist alloc pages tmp failed!\n");
            return false;
        }

        aura_info_tmp[i].stat = INFO_IDLE;
        aura_info_tmp[i].path = NULL;
        aura_info_tmp[i].buff = page_address(page);
        if(NULL == aura_info_tmp[i].buff)
        {
            printk("filelist address page tmp failed!\n");
            return false;
        }
    }
    
    for(i=0; i<AURA_WRITE_INFO_NUM_WRITE; i++)
    {
        page = alloc_page(GFP_KERNEL);
        if(NULL == page)
        {
            printk("filelist alloc pages write failed!\n");
            return false;
        }

        aura_info_write[i].stat = INFO_IDLE;
        aura_info_write[i].path = NULL;
        aura_info_write[i].buff = page_address(page);
        if(NULL == aura_info_write[i].buff)
        {
            printk("filelist address page write failed!\n");
            return false;
        }
    }

    return true;
}

struct aura_write_info *aura_get_one_info(void)
{
    int i;    
    unsigned long flags;
    
    raw_spin_lock_irqsave(&info_lock, flags);
    for(i=0; i<AURA_WRITE_INFO_NUM; i++)
    {
        if(INFO_IDLE == aura_info[i].stat)
        {
            aura_info[i].stat = INFO_USED_OTHER;
            raw_spin_unlock_irqrestore(&info_lock, flags);
            aura_info[i].isdir = false;
            aura_info[i].iswrite = false;
            atomic_inc(&info_count);
            //debug
            //printk(KERN_DEBUG"aura_get_one_info info=0x%08x\n", (unsigned int)&aura_info[i]);
            return &aura_info[i];
        }
    }
    raw_spin_unlock_irqrestore(&info_lock, flags);
    
    return NULL;
}

struct aura_write_info *aura_get_one_info_tmp(void)
{
    int i;    
    
    raw_spin_lock(&info_lock_tmp);
    for(i=0; i<AURA_WRITE_INFO_NUM_TMP; i++)
    {
        if(INFO_IDLE == aura_info_tmp[i].stat)
        {
            aura_info_tmp[i].stat = INFO_USED_OTHER;
            raw_spin_unlock(&info_lock_tmp);
            return &aura_info_tmp[i];
        }
    }
    raw_spin_unlock(&info_lock_tmp);
    
    return NULL;
}

struct aura_write_info *aura_get_one_info_write(void)
{
    int i;    
    
    raw_spin_lock(&info_lock_write);
    for(i=0; i<AURA_WRITE_INFO_NUM_WRITE; i++)
    {
        if(INFO_IDLE == aura_info_write[i].stat)
        {
            aura_info_write[i].stat = INFO_USED_OTHER;
            raw_spin_unlock(&info_lock_write);
            return &aura_info_write[i];
        }
    }
    raw_spin_unlock(&info_lock_write);
    
    return NULL;
}


void aura_put_one_info(struct aura_write_info * info)
{
    unsigned long flags;
    
    if(NULL == info)
    {
        return;
    }
    raw_spin_lock_irqsave(&info_lock, flags);
    info->path = NULL;
    memset(info->buff, 0 , PATH_MAX);
    memset(info->file, 0 , NAME_MAX);
    info->stat = INFO_IDLE;
    raw_spin_unlock_irqrestore(&info_lock, flags);
    
    //debug
    //printk(KERN_DEBUG"aura_put_one_info info=0x%08x\n", (unsigned int)info);
    atomic_dec(&info_count);
}

void aura_put_one_info_tmp(struct aura_write_info * info)
{
    if(NULL == info)
        return;
    raw_spin_lock(&info_lock_tmp);
    info->path = NULL;
    memset(info->buff, 0 , PATH_MAX);
    memset(info->file, 0 , NAME_MAX);
    info->stat = INFO_IDLE;
    raw_spin_unlock(&info_lock_tmp);
}

void aura_put_one_info_write(struct aura_write_info * info)
{
    if(NULL == info)
        return;
    raw_spin_lock(&info_lock_write);
    info->path = NULL;
    memset(info->buff, 0 , PATH_MAX);
    memset(info->file, 0 , NAME_MAX);
    info->stat = INFO_IDLE;
    raw_spin_unlock(&info_lock_write);
}


bool aura_fresh_one_info_by_filepath(struct aura_write_info * info)
{
    int i;
    int offset;
    unsigned long flags;

    if(NULL == info || NULL == info->path)
        return false;

    offset = (int)((unsigned long)info->path - (unsigned long)info->buff);
    
    raw_spin_lock_irqsave(&info_lock, flags);
    for(i=0; i<AURA_WRITE_INFO_NUM; i++)
    {
        if(INFO_USED_TIMER == aura_info[i].stat)
        {
            if(0 == strncmp(info->path, aura_info[i].path, PATH_MAX-offset))
            {
                raw_spin_unlock_irqrestore(&info_lock, flags);
                if(0 == mod_timer_pending(&aura_info[i].timer, jiffies + HZ))
                {                    
                    //printk(KERN_DEBUG"aura_fresh_one_info info=0x%08x\n", (unsigned int)&aura_info[i]);
                    return true;
                }
                else
                {                    
                    //printk(KERN_DEBUG"aura_fresh_one_info info=0x%08x xxxx\n", (unsigned int)info);
                    return true;
                }
            }
        }
    }
    raw_spin_unlock_irqrestore(&info_lock, flags);
    
    return false;
}
/*
void aura_info_timeout_handle(unsigned long data)
{
    struct aura_write_info * info;
    info = (struct aura_write_info *)data;
    
    raw_spin_lock(&filelist_lock);
    list_add_tail(&info->list, &filelist_event);
    raw_spin_unlock(&filelist_lock);
    if(NULL != filelist_task)
        wake_up_process(filelist_task);
    //printk(KERN_DEBUG"aura_info_timeout_handle info=0x%08x\n", (unsigned int)info);
}
*/
void aura_info_timeout_handle(struct timer_list *t)
{
    struct aura_write_info * info;
    //info = (struct aura_write_info *)data;
    info = from_timer(info,t,timer);
    raw_spin_lock(&filelist_lock);
    list_add_tail(&info->list, &filelist_event);
    raw_spin_unlock(&filelist_lock);
    if(NULL != filelist_task)
        wake_up_process(filelist_task);
    //debug
    //printk(KERN_DEBUG"aura_info_timeout_handle info=0x%08x\n", (unsigned int)info);
}

void aura_start_one_info(struct aura_write_info * info)
{
    info->timer.expires = jiffies + HZ;
	//info->timer.data = (unsigned long)info;
	//info->timer.function = aura_info_timeout_handle;
	
//	if(info->timer.entry.next == NULL)
	{
	    //printk(KERN_DEBUG"aura_start_one_info info=0x%08x\n", (unsigned int)info);
//    	timer_setup(&info->timer, aura_info_timeout_handle, 0);
        mod_timer(&info->timer, jiffies + HZ);
        info->stat = INFO_USED_TIMER;
	}
//	else
	{
//	    printk(KERN_ERR "filelist: add a exist timer, info=0x%08x path=[%s] file=[%s]!\n", 
//	           (unsigned int)info, info->path==NULL ? "null":info->path, info->file);
	}
}

bool aura_get_filename_to_buff(const char *name, char *buff, int bufflen)
{
    int i, idx=0;
    int len = strlen(name);

    for(i=len-1; i>0; i--)
    {
        if('/' == name[i])
        {
            break;
        }
    }

    if('/' == name[i])
        idx = i+1;
    else
        idx = i;
        
    if(bufflen < len-idx)
        return false;

    memcpy(buff, name+idx, len-idx);
    return true;
}


char * auralic_get_filename_from_path(char *path)
{
    int i;
    char len = strlen(path);

    for(i=len-1; i>0; i--)
    {
        if('/' == path[i])
            return path+i+1;
    }

    // i==0
    if('/' == path[i])
        return path+i+1;
    else
        return path;
}

size_t aura_strlen(const char *s)
{
	const char *sc;

    if(NULL == s)
    {
        printk("filelist: detect strlen null string pointer!\n");
        return 0;
    }
        
	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

#if 1 // char converts
/* Converts a hex character to its integer value */
char from_hex(char ch) 
{  
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) 
{  
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

void url_encode(char *dst, char *str) 
{  
    char *pstr = str, *pbuf = dst;  
    while (*pstr) 
    {    
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' || *pstr == '/')     
        {      
            *pbuf++ = *pstr;    
        }
        //     else if (*pstr == ' ') 
        //    {
        //        *pbuf++ = '+';
        //    }    
        else
        {
            *pbuf++ = '%';
            *pbuf++ = to_hex(*pstr >> 4);
            *pbuf++ = to_hex(*pstr & 15);    
        }
        pstr++;
    }
    *pbuf = '\0';
}


#endif

int filelist_process_fn(void *data)
{ 
    mm_segment_t fs; 
    unsigned long flags;
    struct file *fp = NULL;
    struct aura_write_info *info;
	void *buftmp = NULL;
    struct page *page = NULL;
    #define new_kernel 1
    #if new_kernel
    int writed;
    struct iovec iov; // new
	struct kiocb kiocb;
	struct iov_iter iter;
	#endif
	
    page = alloc_pages(GFP_KERNEL, 1);// 8K
    if(NULL == page)
    {
        printk("filelist alloc pages for tmp failed!\n");
        filelist_task = NULL;
        return 1;
    }
    buftmp = page_address(page);
	
    fp = filp_open(FILELIST_NAME_TMP, O_RDWR | O_CREAT, 0644);
    if (IS_ERR(fp))
    {
        printk("auralic create filelist file failed!\n");
        filelist_task = NULL;
        free_pages((unsigned long)buftmp, 1);
        return 1;
    }
    
	printk(KERN_DEBUG "auralic %s starting\n", __func__);
    //fs = get_fs();
    //set_fs(KERNEL_DS);
		fs = force_uaccess_begin();
    vfs_can_access = true;
    need_init = false;
    
	while (!kthread_should_stop()) 
	{	
		set_current_state(TASK_INTERRUPTIBLE);
		if(true == need_stop)
		{
	        is_stoped = true;
	        if(true == need_init)// need init again
	        {
	            goto out;
	        }
	        else
	        {
    		    schedule();
    	        continue;
	        }
		}
		else
		{
		    is_stoped = false;
		}
		
		while(!list_empty(&filelist_event))
        {
            int pathlen, filelen;
            __set_current_state(TASK_RUNNING);
            raw_spin_lock_irqsave(&filelist_lock, flags);
            info = list_entry(filelist_event.next, struct aura_write_info, list);
            list_del(&info->list);
            raw_spin_unlock_irqrestore(&filelist_lock, flags);

            has_newlist = true;
            fp->f_op->llseek(fp, 0, SEEK_END);
            if(NULL != info->path && !IS_ERR(info->path))
            {
                pathlen =  aura_strlen(info->path);
                filelen = aura_strlen(info->file);
                if(PATH_MAX < pathlen+filelen || NAME_MAX < filelen)
                {
                    printk("invalid filelist len(%d+%d=%d)!\n", pathlen, filelen, pathlen+filelen);
                    printk(KERN_DEBUG"invalid filelist, path=[%d:%s] file=[%d:%s]\n", 
                                      pathlen, info->path, filelen, info->file);
                }
                else
                {   
                    #if 0
                    fp->f_op->write(fp, info->path, aura_strlen(info->path), &fp->f_pos);
                    if(false == info->iswrite)
                    {
                        fp->f_op->write(fp, "/", 1, &fp->f_pos);
                        fp->f_op->write(fp, info->file, aura_strlen(info->file), &fp->f_pos);
                        if(true == info->isdir)
                            fp->f_op->write(fp, "/", 1, &fp->f_pos);
                    }
                    #else
                    memset(buftmp, 0 , PATH_MAX);
                    url_encode(buftmp, info->path);
                    pathlen =  aura_strlen(buftmp);
                    if(PATH_MAX > pathlen)
                    {
#if new_kernel
                        iov.iov_base = (void __user *)buftmp;
                        iov.iov_len = pathlen;
                        init_sync_kiocb(&kiocb, fp);
                        kiocb.ki_pos = fp->f_pos;
                        iov_iter_init(&iter, WRITE, &iov, 1, pathlen);
                        //writed=fp->f_op->write_iter(&kiocb, &iter);
                        loff_t pos = 0;
                        writed = kernel_write(fp,buftmp,pathlen,&fp->f_pos);
                        //if(0 < writed)
                        //    fp->f_pos += writed;
#else
                        fp->f_op->write(fp, buftmp, pathlen, &fp->f_pos);
#endif
                        if(false == info->iswrite)
                        {
#if new_kernel
                            iov.iov_base = (void __user *)"/";
                            iov.iov_len = 1;
                            init_sync_kiocb(&kiocb, fp);
                            kiocb.ki_pos = fp->f_pos;
                            iov_iter_init(&iter, WRITE, &iov, 1, 1);
                            //writed=fp->f_op->write_iter(&kiocb, &iter);
                            loff_t pos = 0;
                            writed = kernel_write(fp,"/",1,&fp->f_pos);
                            //if(0 < writed)
                            //    fp->f_pos += writed;
#else
                            fp->f_op->write(fp, "/", 1, &fp->f_pos);
#endif
                            memset(buftmp, 0, NAME_MAX);
                            url_encode(buftmp, info->file);
                            filelen =  aura_strlen(buftmp);
#if new_kernel
                            iov.iov_base = (void __user *)buftmp;
                            iov.iov_len = filelen;
                            init_sync_kiocb(&kiocb, fp);
                            kiocb.ki_pos = fp->f_pos;
                            iov_iter_init(&iter, WRITE, &iov, 1, filelen);
                            //writed=fp->f_op->write_iter(&kiocb, &iter);
                            writed = kernel_write(fp,buftmp,filelen,&fp->f_pos);
                            //if(0 < writed)
                            //    fp->f_pos += writed;
#else
                            fp->f_op->write(fp, buftmp, filelen, &fp->f_pos);
#endif
                            if(true == info->isdir)
                            {
#if new_kernel
                                iov.iov_base = (void __user *)"/";
                                iov.iov_len = 1;
                                init_sync_kiocb(&kiocb, fp);
                                kiocb.ki_pos = fp->f_pos;
                                iov_iter_init(&iter, WRITE, &iov, 1, 1);
                                //    writed=fp->f_op->write_iter(&kiocb, &iter);
                                //loff_t pos = 0;
                                writed = kernel_write(fp,"/",1,&fp->f_pos);
                                //if(0 < writed)
                                //    fp->f_pos += writed;
#else
                                fp->f_op->write(fp, "/", 1, &fp->f_pos);
#endif
                            }
                        }
                    }
#endif
                }
            }
#if new_kernel
            iov.iov_base = (void __user *)"\n";
            iov.iov_len = 1;
            init_sync_kiocb(&kiocb, fp);
            kiocb.ki_pos = fp->f_pos;
            iov_iter_init(&iter, WRITE, &iov, 1, 1);
            //writed=fp->f_op->write_iter(&kiocb, &iter);
            //loff_t pos = 0;
            writed = kernel_write(fp,"\n",1,&fp->f_pos);
            //if(0 < writed)
            //    fp->f_pos += writed;
#else
            fp->f_op->write(fp, "\n", strlen("\n"), &fp->f_pos);
#endif
            aura_put_one_info(info);
            info = NULL;
            if(true == need_stop)
                break;
        }
        
	    schedule();
		
	} /* end while (!kthread_should_stop()) */
	
	__set_current_state(TASK_RUNNING);

    
out:        
    filp_close(fp, NULL);
    //set_fs(fs);
        force_uaccess_end(fs);
	printk(KERN_ERR "auralic %s exiting!\n", __func__);
    free_pages((unsigned long)buftmp, 1);
	    
	return 0;
}

                
ssize_t filelist_proc_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{
    int len = 0, tmp;
    char buff[200] = {0};
    
    if(0 != *offset)
        return 0;
    tmp = atomic_read(&info_count);
    if(tmp)
    {
        len += sprintf(buff+len, "status: %s(%d)\n", "busy", tmp);
    }
    else
    {
        len += sprintf(buff+len, "status: %s\n", "idle");
    }
    
    if(true == has_newlist)
    {
        len += sprintf(buff+len, "newlist: %s\n", "yes");
    }
    else
    {
        len += sprintf(buff+len, "newlist: %s\n", "no");
    }
    
    if(true == is_stoped)
    {
        len += sprintf(buff+len, "process: %s\n", "stoped");
    }
    else
    {
        len += sprintf(buff+len, "process: %s\n", "running");
    }

    len += sprintf(buff+len, "path = [%s]\n", MATCH_PATH_STR);
    
    *offset = len;
    
    if(0 != copy_to_user(usrbuf, buff, len))
    {
        return 0;
    }
    
    return len;
}


static ssize_t filelist_proc_write(struct file *filp, const char __user *usr_buf,
                              size_t count, loff_t *f_pos)
{
    char len;
    char buff[200] = {0};

    len = count < 199 ? count : 199;
    memset(buff, '\0', 199);
    if(0 != copy_from_user(buff, usr_buf, len))
    {
        goto out;
    }
    
    if(0 == strncmp(buff, "start", 5))
    {
        if(true == need_init)
        {            
            raw_spin_lock_init(&filelist_lock);
            INIT_LIST_HEAD(&filelist_event);
            
            if(false == init_aura_info())
            {
                pr_err("filelist init aura_info failed!\n");
                goto out;
            }
            
            filelist_task = kthread_run(filelist_process_fn, NULL, "filelist");
            if (IS_ERR(filelist_task)) 
            {
                pr_err("create filelist task failed!\n");
                goto out;
            }
        }
        need_stop = false;
        printk(KERN_DEBUG"auralic filelist started!\n");
        wake_up_process(filelist_task);
    }
    else if(0 == strncmp(buff, "stop", 4))
    {
        is_stoped = false;
        need_stop = true;
        has_newlist = false;
        if(NULL != filelist_task)
        {
            wake_up_process(filelist_task);
            while(false == is_stoped)
            {
                msleep(50);
            }
        }
        printk(KERN_DEBUG"auralic filelist stoped!\n");
    }
    else if(0 == strncmp(buff, "formate", strlen("formate")))
    {
        vfs_can_access = false;
        need_init  = true;
        is_stoped = false;
        need_stop = true;
        has_newlist = false;
        if(NULL != filelist_task)
        {
            wake_up_process(filelist_task);
            while(false == is_stoped)
            {
                msleep(50);
            }
        }
        msleep(1000);//wait all timer to timeout
        filelist_task = NULL;
        printk(KERN_DEBUG"auralic filelist stoped for formate!\n");
    }
    else if(0 == strncmp(buff, "/media/", strlen("/media/")))
    {
        char len = strlen(buff);
        if(len < 20)
        {
            memset(MATCH_PATH_STR, 0, 20);
            memcpy(MATCH_PATH_STR, buff, len-1);
            printk("auralic filelist set new match path [%s] success!\n", MATCH_PATH_STR);
        }
        else
        {
            printk("auralic filelist path string can't more than 20 letters!\n");
        }
    }
    else
    {
        printk(helpstring);
    }
	
out:		                
    return count;
}


static const struct  proc_ops filelist_proc_op = {
    .proc_read = filelist_proc_read,
    .proc_write = filelist_proc_write,
};
     
static int __init filelist_init(void)
{    
    printk(KERN_DEBUG"enter auralic filelist module!\n");

    proc_create(FILELIST_PROC_NAME, 0755, NULL, &filelist_proc_op);

    return 0;
}

static void __exit filelist_exit(void)
{
    int i;
    
    printk(KERN_DEBUG"exit auralic filelist module!\n");
    
    if (filelist_task)
        kthread_stop(filelist_task);
            
	remove_proc_entry(FILELIST_PROC_NAME, NULL);

	
    for(i=0; i<AURA_WRITE_INFO_NUM; i++)
    {
        if(aura_info[i].buff)
            free_page((unsigned long)aura_info[i].path);
        aura_info[i].buff = NULL;
    }
    
    for(i=0; i<AURA_WRITE_INFO_NUM_TMP; i++)
    {
        if(aura_info_tmp[i].buff)
            free_page((unsigned long)aura_info_tmp[i].path);
        aura_info_tmp[i].buff = NULL;
    }
    
    for(i=0; i<AURA_WRITE_INFO_NUM_WRITE; i++)
    {
        if(aura_info_write[i].buff)
            free_page((unsigned long)aura_info_write[i].buff);
        aura_info_write[i].buff = NULL;
    }
}

module_init(filelist_init);
module_exit(filelist_exit);
        
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongfa.hu@auralic.com");
MODULE_DESCRIPTION("record file change time");

