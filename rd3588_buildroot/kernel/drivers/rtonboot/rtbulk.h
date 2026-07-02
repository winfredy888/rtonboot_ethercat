#ifndef _RTBULK_H_
#define _RTBULK_H_

#include "list.h"

#include <linux/semaphore.h>

#include "rtonboot_ioctl.h"

/*#define RTONBOOT_DEBUG 1*/

#undef RTONBOOT_DEBUG

#ifndef RTBULK_DEV_MAJOR
#define RTBULK_DEV_MAJOR 0
#endif

#ifndef RTBULK_DEV_NR_DEVS
#define RTBULK_DEV_NR_DEVS 1
#endif

#define RTBULK_DEVICE_NAME "rtbulk"

#endif

