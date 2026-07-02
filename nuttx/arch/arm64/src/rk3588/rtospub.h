#ifndef _RTOSPUB_H_
#define _RTOSPUB_H_

#include <stdint.h>
#include <pthread.h>

#include "../../../../drivers/ethercat/stmmac/patch.h"

#include "../../../../drivers/ethercat/stmmac/linux_stmmac.h"	

#include "../../../../drivers/ethercat/stmmac/stmmac.h"	

#include "../../../../drivers/ethercat/stmmac/rkpriv.h"

#define EC_KERNEL_EXEC_TASK_PRIORITY 147
#define EC_KERNEL_EXEC_TASK_STACK_SIZE 32768

#define EC_KERNEL_EXEC_TASK_NAME "ec_kernel_exec"

#undef INCLUDE_NONPUB

#include "msgid.h"

#include "msg_common_struct.h"

struct rtonboot_bulkpara {
	uint32_t offset;
	uint32_t len;
};

typedef void (*resp_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv);
	
typedef void (*end_callack_func_t)(uint32_t msgid, uint32_t msglen, uint8_t * p_content, 
	void * priv);
	
typedef void (*prepare_send_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv);

void rtospub_set_resp_callback(uint32_t dst_msgid, uint64_t * p_resp_cb, uint64_t * p_resp_cb_priv);

void rtospub_set_end_callback(uint32_t dst_msgid, uint64_t * p_end_cb, uint64_t * p_end_cb_priv);

void rtospub_set_prepare_callback(uint32_t dst_msgid, prepare_send_callack_func_t * p_cb, void ** p_cb_priv);

extern int prepare_send_and_wake_up(uint32_t msgid);

void prepare_rtos_bulk_and_wake_up(void);

#endif
