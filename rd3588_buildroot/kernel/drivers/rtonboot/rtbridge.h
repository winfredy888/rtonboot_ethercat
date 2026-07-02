#ifndef _RTBRIDGE_H_
#define _RTBRIDGE_H_

#include "list.h"

#include <linux/semaphore.h>

#include "rtonboot_ioctl.h"

#include "rt_common_internal.h"
/*#define RTONBOOT_DEBUG 1*/
#undef RTONBOOT_DEBUG

#ifndef RTBRIDGE_DEV_MAJOR
#define RTBRIDGE_DEV_MAJOR 0
#endif

#ifndef RTBRIDGE_DEV_NR_DEVS
#define RTBRIDGE_DEV_NR_DEVS 1
#endif

#define RTBRIDGE_DEVICE_NAME "rtbridge"
#define SEND_SLEEP_MS 100

#define MSGID_CREATE_MSG 0
#define MSGID_DESTORY_MSG 1
#define MSGID_RECEIVE_BROADCAST_MSG 2

#define MSGID_USER_BEGIN 3


#define OFFSET_TO_PREDEFINE_MSG_CONTENT 48

#define RTONBOOT_TRACE_LEVEL_ERROR 1
#define RTONBOOT_TRACE_LEVEL_WARN 2
#define RTONBOOT_TRACE_LEVEL_INFO 3
#define RTONBOOT_TRACE_LEVEL_DETAIL 4


struct pollpara_list_item {
	struct list_head pollpara_list;
	uint32_t pid;
	int * p_msg_created_received;
	wait_queue_head_t msg_created_wait;
	int msg_created_received;
	uint32_t val;
};

struct k7_msglist_item {
	struct list_head k7_msglist;
	uint32_t msgid;
	uint32_t offset_cur_list;
};

struct rtbridge_dev                              
{                   
	struct mutex send_lock;
	spinlock_t	send_slock;
	wait_queue_head_t send_queue;
	struct task_struct * send_thread;
	struct list_head msg_send_list_head;
};

struct rtbridge_file_priv
{
	struct rtbridge_dev * devp;
	struct fasync_struct * async_queue;
	struct pollpara_list_item * p_item;
};	

struct send_list_item {
	struct list_head send_list;
	struct rtonboot_msg_header msg_header;
	uint32_t is_content_alloc_inlist;
	uint8_t * p_content;
};

struct rtonboot_resp_header {
	uint32_t resp_status;
	uint32_t offset_content;
	uint32_t offset_cur_list;
	uint32_t offset_root_list_head;
	uint32_t dst_msgid;
};

struct rtonboot_rr_resp_header {
	uint32_t resp_status;
};

struct rtonboot_bulkpara {
	uint32_t offset;
	uint32_t len;
};


static inline void atomic_store_release_64(uint64_t *ptr, uint64_t val)
{
    asm volatile(
        "stlr %1, [%0]"
        :
        : "r" (ptr), "r" (val)
        : "memory"
    );
}

static inline uint64_t atomic_load_acquire_64(const uint64_t *ptr)
{
    uint64_t val;
    
    asm volatile(
        "ldar %0, [%1]"
        : "=r" (val)
        : "r" (ptr)
        : "memory"
    );
    
    return val;
}

static inline void atomic_store_release_32(uint32_t *ptr, uint32_t val)
{
    asm volatile(
        "stlr %w[val], [%[ptr]]"
        : 
        : [ptr] "r" (ptr), [val] "r" (val)
        : "memory"
    );
}

static inline uint32_t atomic_load_acquire_32(const uint32_t *ptr)
{
    uint32_t val;
    asm volatile(
        "ldar %w[val], [%[ptr]]"
        : [val] "=r" (val)
        : [ptr] "r" (ptr)
        : "memory"
    );
    return val;
}

void RTOnBootTraceDetail(char * fmt, ...);
	
void RTOnBootTraceInfo(char * fmt, ...);	

void RTOnBootTraceWarn(char * fmt, ...);	

void RTOnBootTraceError(char * fmt, ...);	

#endif

