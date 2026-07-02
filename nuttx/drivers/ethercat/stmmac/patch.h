#ifndef __STMMC_PATCH_H__
#define __STMMC_PATCH_H__

#include <sys/types.h>

#include "nuttx_net.h"
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>

#include <sys/endian.h>

#define ETH_DATA_LEN 1500
#define ETH_ZLEN  60
#define ETH_HLEN  14
#define ETH_FRAME_LEN 1514
#define ETH_FCS_LEN	4

typedef uint64_t     u64;
typedef int64_t      s64;
typedef int32_t      s32;
typedef uint32_t     u32;
typedef unsigned char u8;
typedef uint16_t     u16;


#define KERN_EMERG	0	/* system is unusable */
#define KERN_ALERT	1	/* action must be taken immediately */
#define KERN_CRIT	2	/* critical conditions */
#define KERN_ERR	3	/* error conditions */
#define KERN_WARNING 4	/* warning conditions */
#define KERN_NOTICE	5	/* normal but significant condition */
#define KERN_INFO	6	/* informational */
#define KERN_DEBUG	7	/* debug-level messages */

#define KERN_CONT 0

#define max(a, b)   a > b ? a : b
#define min(a, b)   a > b ? b : a

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define uswap_16(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))
#define _uswap_64(x, sfx) \
	((((x) & 0xff00000000000000##sfx) >> 56) | \
	 (((x) & 0x00ff000000000000##sfx) >> 40) | \
	 (((x) & 0x0000ff0000000000##sfx) >> 24) | \
	 (((x) & 0x000000ff00000000##sfx) >>  8) | \
	 (((x) & 0x00000000ff000000##sfx) <<  8) | \
	 (((x) & 0x0000000000ff0000##sfx) << 24) | \
	 (((x) & 0x000000000000ff00##sfx) << 40) | \
	 (((x) & 0x00000000000000ff##sfx) << 56))
	 
# define uswap_64(x) _uswap_64(x, ull)	 

#if BYTE_ORDER == LITTLE_ENDIAN
# define cpu_to_le16(x)		(x)
# define cpu_to_le32(x)		(x)
# define cpu_to_le64(x)		(x)
# define le16_to_cpu(x)		(x)
# define le32_to_cpu(x)		(x)
# define le64_to_cpu(x)		(x)
# define cpu_to_be16(x)		uswap_16(x)
# define cpu_to_be32(x)		uswap_32(x)
# define cpu_to_be64(x)		uswap_64(x)
# define be16_to_cpu(x)		uswap_16(x)
# define be32_to_cpu(x)		uswap_32(x)
# define be64_to_cpu(x)		uswap_64(x)
#else
# define cpu_to_le16(x)		uswap_16(x)
# define cpu_to_le32(x)		uswap_32(x)
# define cpu_to_le64(x)		uswap_64(x)
# define le16_to_cpu(x)		uswap_16(x)
# define le32_to_cpu(x)		uswap_32(x)
# define le64_to_cpu(x)		uswap_64(x)
# define cpu_to_be16(x)		(x)
# define cpu_to_be32(x)		(x)
# define cpu_to_be64(x)		(x)
# define be16_to_cpu(x)		(x)
# define be32_to_cpu(x)		(x)
# define be64_to_cpu(x)		(x)
#endif

extern u64 volatile jiffies;

static u64 inline usecs_to_jiffies(u32 us)
{
	u64 temp = (u64)us * (u64)TICK_PER_SEC;
	
	temp /= 1000;
	
	temp /= 1000;
	
	return max(temp, 1);
}

static u64 inline msecs_to_jiffies(u32 ms)
{
	u64 temp = (u64)ms * (u64)TICK_PER_SEC;
	
	temp /= 1000;
	
	return max(temp, 1);
}

#define MAX_ERRNO  4095

static inline void *ERR_PTR(long error)
{
   return (void *) error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

static inline bool IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

static inline long IS_ERR(const void *ptr)
{
   return IS_ERR_VALUE((unsigned long)ptr);
}

#define max_t(type, a, b) max(((type) a), ((type) b))

u64 do_div(u64 dividend, u64 divisor);
		
void printk(int level, const char *format, ...);

void pr_debug(const char *format, ...);

void pr_info(const char *format, ...);

void pr_err(const char *format, ...) ;

void pr_warn(const char *format, ...);

typedef void (*ec_pollfunc_t)(struct net_driver_s *);

struct ec_device
{
	struct net_driver_s * dev;
	int open;
	ec_pollfunc_t poll;
	uint8_t * recv_buf;
	int total_bytes;
	int recv_len;
};

typedef struct ec_device ec_device_t;	

void ecdev_set_link(
        ec_device_t *device, /**< EtherCAT device */
        uint8_t state /**< new link state */
        );
        
void ecdev_receive(
        ec_device_t *device, /**< EtherCAT device */
        const void *data, /**< pointer to received data */
        size_t size /**< number of bytes received */
        );
        
ec_device_t * ecdev_offer(struct net_driver_s *net_dev, ec_pollfunc_t poll /*,
        struct module_s * module*/)  ;
        
int ecdev_open(ec_device_t *device /**< EtherCAT device */);

void ecdev_close(ec_device_t *device /**< EtherCAT device */);

void ecdev_withdraw(ec_device_t *device /**< EtherCAT device */);        

typedef unsigned int (* real_timer_callback)(void *);

struct timer_wdparm {
	struct wdog_s wdog;
    real_timer_callback real_callback;
	void * real_priv;
	unsigned int timeout;
	struct work_s timer_work;
	int pending;
};

struct timer_dev_s {
    struct timer_wdparm wdparm;
};

struct work_struct {
    struct wdog_s work_timer;
    void (*func)(void *);
    void *data;
    int pending;
    struct work_s work_struct_work;
};


void timer_init(FAR struct timer_dev_s *dev, unsigned int timeout, real_timer_callback real_callback,
	void * real_priv);

void del_timer_sync(FAR struct timer_dev_s *dev);

void mod_timer(FAR struct timer_dev_s *dev, unsigned int new_timeout);

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

void print_hex_dump_bytes(const char *prefix_str, int prefix_type,
			  const void *buf, size_t len);
			  
void rtnl_lock(void);

void rtnl_unlock(void);

void rtnl_init(void);

void work_init(struct work_struct *work, void (*func)(void *), void *data) ;

void queue_delayed_work(struct work_struct *work, unsigned long delay) ;

void cancel_delayed_work_sync(struct work_struct *work);

#define cancel_delayed_work cancel_delayed_work_sync

#endif

