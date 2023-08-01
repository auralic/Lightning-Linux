#ifndef _AURALIC_IRKEY_H_
#define _AURALIC_IRKEY_H_

#include <linux/ioctl.h>

#define IRKEY_IOC_MAGIC		0x55

#define IRKEY_SAVE			_IO(IRKEY_IOC_MAGIC, 1)
#define IRKEY_LEARN			_IOW(IRKEY_IOC_MAGIC, 2, int)
#define IRKEY_PRESS			_IOW(IRKEY_IOC_MAGIC, 3, int)
#define IRKEY_CLEAN			_IOW(IRKEY_IOC_MAGIC, 4, int)
#define IRKEY_CLEAN_ALL		_IO(IRKEY_IOC_MAGIC, 5)
#define IRKEY_STOP_LEARN    _IO(IRKEY_IOC_MAGIC, 6)
#define GET_PLUG_LLINK_DOWN_BLOCK		_IOR(IRKEY_IOC_MAGIC, 7, int) //plugin from down
#define GET_PLUG_LLINK_DOWN_IMDIAT		_IOR(IRKEY_IOC_MAGIC, 8, int)
#define SET_READY_LLINK_DOWN	        _IOW(IRKEY_IOC_MAGIC, 9, int) //ready to down

#define GET_READY_LLINK_UP_BLOCK		_IOR(IRKEY_IOC_MAGIC, 10, int) //ready from up
#define GET_READY_LLINK_UP_IMDIAT		_IOR(IRKEY_IOC_MAGIC, 11, int)
#define SET_PLUG_LLINK_UP	            _IOW(IRKEY_IOC_MAGIC, 12, int) //plugout to up 

#define GET_PLUG_LLINK_DOWN2_BLOCK		_IOR(IRKEY_IOC_MAGIC, 13, int) //plugin from down2
#define GET_PLUG_LLINK_DOWN2_IMDIAT		_IOR(IRKEY_IOC_MAGIC, 14, int)
#define SET_READY_LLINK_DOWN2	        _IOW(IRKEY_IOC_MAGIC, 15, int) //ready to down2

#define IRKEY_CMD_MAXNR 	15

#endif /* _AURALIC_IRKEY_H_ */

