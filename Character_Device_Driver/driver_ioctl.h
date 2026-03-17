#ifndef DRIVER_IOCTL_H
#define DRIVER_IOCTL_H

#include <linux/ioctl.h>

#define DRIVER_MAGIC 'B'

#define IOCTL_GET_OPEN_COUNT    _IOR(DRIVER_MAGIC, 1, int)
#define IOCTL_GET_MSG_LEN       _IOR(DRIVER_MAGIC, 2, int)
#define IOCTL_CLEAR_BUFFER      _IO(DRIVER_MAGIC, 3)
#define IOCTL_GET_VERSION       _IOR(DRIVER_MAGIC, 4, int)
#define IOCTL_SET_MAX_MSG_SIZE  _IOW(DRIVER_MAGIC, 5, int)

#define DRIVER_VERSION_CODE     1

#endif
