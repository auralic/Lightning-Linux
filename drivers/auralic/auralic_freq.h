#ifndef _AURALIC_FREQ_H_
#define _AURALIC_FREQ_H_

#include <linux/ioctl.h>

#define FREQ_IOC_MAGIC		'H'

#define GET_FREQ_BLOCK		_IO(FREQ_IOC_MAGIC, 1)
#define GET_FREQ_IMDIAT		_IOR(FREQ_IOC_MAGIC, 2, int)
#define GET_VOL_BLOCK		_IO(FREQ_IOC_MAGIC, 3)
#define GET_VOL_IMDIAT		_IOR(FREQ_IOC_MAGIC, 4, int)
#define GET_USB_OPEN_BLOCK	_IOR(FREQ_IOC_MAGIC, 5, int)
#define GET_PLUG_LLINK_DOWN_BLOCK		_IOR(FREQ_IOC_MAGIC, 6, int) //plugin from down
#define GET_PLUG_LLINK_DOWN_IMDIAT		_IOR(FREQ_IOC_MAGIC, 7, int)
#define GET_READY_LLINK_UP_BLOCK		_IOR(FREQ_IOC_MAGIC, 8, int) //ready from up
#define GET_READY_LLINK_UP_IMDIAT		_IOR(FREQ_IOC_MAGIC, 9, int)
#define SET_PLUG_LLINK_UP	            _IOW(FREQ_IOC_MAGIC, 10, int) //plugout to up 
#define SET_READY_LLINK_DOWN	        _IOW(FREQ_IOC_MAGIC, 11, int) //ready to down
#define GET_USBIN_OPEN_IMDIAT           _IOR(FREQ_IOC_MAGIC, 12, int)
#define GET_USB_CLOSE_BLOCK             _IOR(FREQ_IOC_MAGIC, 13, int)
#define GET_USBIN_HANG_IMDIAT             _IOR(FREQ_IOC_MAGIC, 14, int)
#define GET_IS_DOP_BLOCK             _IOR(FREQ_IOC_MAGIC, 15, int)
#define GET_IS_DOP_IMDIAT             _IOR(FREQ_IOC_MAGIC, 16, int)
#define GET_CLIP_ERR_BLOCK             _IOR(FREQ_IOC_MAGIC, 17, int)
#define GET_CLIP_ERR_IMDIAT             _IOR(FREQ_IOC_MAGIC, 18, int)
#define GET_DC_ERR_BLOCK             _IOR(FREQ_IOC_MAGIC, 19, int)
#define GET_DC_ERR_IMDIAT             _IOR(FREQ_IOC_MAGIC, 20, int)
#define GET_DIX9211_FREQ             _IOR(FREQ_IOC_MAGIC, 21, int)
#define FREQ_CMD_MAXNR 	21

typedef enum input_channel
{
	AES = 0,
	AES_BYPASS,
	TOS1,
	TOS1_BYPASS,
	TOS2,
	TOS2_BYPASS,
	COAX1,
	COAX1_BYPASS,
	COAX2,
	COAX2_BYPASS,
	ANALOG,
    PHONO,
	STREAMING,
	USB_DEVICE,
	USB_DEVICE_BYPASS,
	LIGHTNING_LINK,
	LIGHTNING_LINK_BYPASS,
	INPUT_NONE_SELECT,
	AES_COAX_TOS
}input_channel_t;

#endif /* _AURALIC_FREQ_H_ */

