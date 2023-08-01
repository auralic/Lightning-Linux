#ifndef  AURALIC_FILELIST_H
#define  AURALIC_FILELIST_H

#define  FILELIST_DEBUG         0
#define  FILELIST_MODIFY_DEBUG  0
#define  FILELIST_D_PATH_RESERVE 64
enum filelist_status_enum
{
    INFO_IDLE  = 0, // not used
    INFO_USED_TIMER = 1, // used by timer
    INFO_USED_OTHER = 2, // used by others
    INFO_USED_BAD   = 3  // used, but has bad info
};

struct aura_write_info
{
    char stat;
    bool isdir;
    bool iswrite;
    void *buff;
    char *path;
    struct list_head list;
    struct timer_list timer;
    char file[NAME_MAX];
};


extern char MATCH_PATH_STR[20];
extern bool vfs_can_access;
extern raw_spinlock_t filelist_lock;
extern struct list_head filelist_event;   
extern struct task_struct *filelist_task;  
extern bool aura_get_filename_to_buff(const char *name, char *buff, int bufflen);

struct aura_write_info *aura_get_one_info(void);
struct aura_write_info *aura_get_one_info_tmp(void);
struct aura_write_info *aura_get_one_info_write(void);
void aura_put_one_info(struct aura_write_info * info);
void aura_put_one_info_tmp(struct aura_write_info * info);
void aura_put_one_info_write(struct aura_write_info * info);
void aura_start_one_info(struct aura_write_info * info);
bool aura_fresh_one_info_by_filepath(struct aura_write_info * info);

#endif
