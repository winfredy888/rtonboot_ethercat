#ifndef _RTSDK_H_
#define _RTSDK_H_

#include <sys/ioctl.h> 
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#define INCLUDE_NONPUB 1

#include "msgid.h"
#include "error_num.h"

#include "list.h"

#define MTL_MAX_TX_QUEUES	8
#define MTL_MAX_RX_QUEUES	8
#define AXI_BLEN	7

#include "msg_common_struct.h"

#include "rtonboot_ioctl_struct.h"

#ifdef __cplusplus 
extern "C" {
#endif
/*#define RTONBOOT_DEBUG 1*/

#undef RTONBOOT_DEBUG

#define CREATE_MSG_TIMEOUT_MS 2000L

#define CREATE_MSG_SLEEP_US 100L

#define RTONBOOT_TRACE_LEVEL_ERROR 1
#define RTONBOOT_TRACE_LEVEL_WARN 2
#define RTONBOOT_TRACE_LEVEL_INFO 3
#define RTONBOOT_TRACE_LEVEL_DETAIL 4
extern pthread_rwlock_t g_prepare_rwlock;
extern pthread_rwlock_t g_resp_rwlock;
extern pthread_rwlock_t g_end_rwlock;

#define WITH_PREPARE_LOCK(block) do {          \
    pthread_rwlock_wrlock(&(g_prepare_rwlock));             \
    block                                     \
    pthread_rwlock_unlock(&(g_prepare_rwlock));           \
} while(0)

#define WITH_RESP_LOCK(block) do {          \
    pthread_rwlock_wrlock(&(g_resp_rwlock));             \
    block                                     \
    pthread_rwlock_unlock(&(g_resp_rwlock));           \
} while(0)

#define WITH_END_LOCK(block) do {          \
    pthread_rwlock_rdlock(&(g_end_rwlock));             \
    block                                     \
    pthread_rwlock_unlock(&(g_end_rwlock));           \
} while(0)

typedef void (*user_sig_callack_func_t)(int signo);

typedef void (*rtsdk_resp_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t ** p_content, 
	uint32_t max_content_size, void * priv);
	
typedef void (*rtsdk_end_callack_func_t)(uint32_t msgid, uint32_t msglen, uint8_t * p_content, 
	void * priv);
	
typedef void (*rtsdk_prepare_send_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv);

struct rtsdk_msg_list_item {
	struct list_head msg_list;
	uint32_t msgid;
	uint32_t max_content_size;
	uint32_t offset_content;
	uint32_t offset_cur_list;
	uint8_t is_kernel;
	rtsdk_prepare_send_callack_func_t rtsdk_prepare_send_callack;
	void * rtsdk_prepare_send_callack_priv;
	rtsdk_end_callack_func_t rtsdk_end_callack;
	void * rtsdk_end_callack_priv;
	rtsdk_resp_callack_func_t rtsdk_resp_callack;
	void * rtsdk_resp_callack_priv;
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
int RTSDKBeginInit(void);

unsigned char * RTSDKGetDebugBufferPtr(void);

void RTSDKUnInit(void);

int RTSDKCreateMsgs(struct rtonboot_create_msg_desc * p_create_desc, int total_count);

void RTSDKDeleteSingleMsg(uint32_t dst_msgid);

void RTSDKDeleteAllMsg(void);

int RTSDKSendMsg(uint32_t msgid);

int RTSDKSendMsgAndFlush(uint32_t msgid, uint8_t flush_state,
	uint32_t flush_start, uint32_t flush_size);

void RTSDKSetUserSignalFunction(user_sig_callack_func_t sig_callack);

int RTSDKUseBulk(void);

int RTSDKCreateBulkMsgs(uint32_t linux_ready_bulk_msgid, uint32_t rtos_ready_bulk_msgid);

int RTSDKReceiveBroadcast(void);

int RTSDKUnReceiveBroadcast(void);

int RTSDKCheckECNetPortIdx(void);

int RTSDKGetSanerfaPara(uint8_t * pSanErFA);

int RTSDKCpyKernelBuffer(uint32_t buf_off, uint32_t buf_len,
		char * bufptr);

int RTSDKSetRtonbootTraceLevel(int level);

int RTSDKRtonbootUserTrace(int level, char * fmt, va_list args);

void RTSDKRtonbootTraceDetail(char * fmt, ...);

void RTSDKRtonbootTraceInfo(char * fmt, ...);

void RTOnBootTraceWarn(char * fmt, ...)	;

void RTOnBootTraceError(char * fmt, ...);

void delay_us(long micro_seconds);

#ifdef __cplusplus 
}
#endif

#endif
