#ifndef __ARCH_ARM64_SRC_RK3568_RK3568_BOOT_H
#define __ARCH_ARM64_SRC_RK3568_RK3568_BOOT_H

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch/chip/chip.h>
#include "arm64_internal.h"
#include "arm64_arch.h"

#include "list.h"

#include "rtospub.h"

/*#define RTONBOOT_DEBUG 1*/

#undef RTONBOOT_DEBUG

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#define MSGID_CREATE_MSG 0
#define MSGID_DESTORY_MSG 1
#define MSGID_RECEIVE_BROADCAST_MSG 2

#define MSGID_USER_BEGIN 3

#define MSGID_EXECCMD_ON_RTOS (MSGID_USER_BEGIN + 1)
#define MSGID_SEND_EXECCMD_RESULT (MSGID_USER_BEGIN + 2)
#define MSGID_TESTRR (MSGID_USER_BEGIN + 3)
#define MSGID_TESTBC (MSGID_USER_BEGIN + 4)
#define MSGID_TESTBC_DUMMY (MSGID_USER_BEGIN + 5)

#define MSG_ORIGIN_LINUX 0
#define MSG_ORIGIN_RTOS 1

#define MSG_HAS_RESPONSE 0
#define MSG_NO_RESPONSE 1

#define RESP_STATUS_SUCCESS 0
#define RESP_STATUS_NOMEM 1
#define RESP_STATUS_MSG_DUPLICATE 2
#define RESP_STATUS_NO_FOUND 3

#define PID_BROADCAST_MSG 0xffffffff
#define PID_KERNEL_MSG 0xfffffffe 

#define OFFSET_TO_RTONBOOT_BULK_BUFFER (4L * 1024L * 1024L)

struct rtonboot_msg_header {
	uint32_t msgid;
	uint32_t max_content_size;
	uint32_t msg_len;
	uint8_t msg_origin;
	uint8_t msg_has_response;
	uint8_t is_broadcast;
	uint8_t is_kernel;
	uint32_t pid;
	uint32_t offset_content;
	uint64_t async_or_callback;
	uint64_t callback_priv;
	uint32_t offset_cur_listem;
};

struct rtonboot_resp_header {
	uint32_t resp_status;
	uint32_t offset_content;
	uint32_t offset_cur_list;
	uint32_t offset_root_list_head;
};

struct root_list_item {
	struct list_head root_list;
	struct list_head msg_list_head;
	uint32_t pid;
	uint32_t is_receive;
};

struct msg_list_item {
	struct list_head msg_list;
	struct rtonboot_msg_header msg_header;
	uint64_t end_callback;
	uint64_t end_callback_priv;
	uint64_t resp_callback;
	uint64_t resp_callback_priv;
};

struct rtos_send_list_item {
	struct list_head rtos_send_list;
	struct rtonboot_msg_header * p_msg_header;
	uint8_t * p_content;
};

struct prepare_send_list_item {
	struct list_head prepare_send_list;
	uint32_t msgid;
	uint32_t offset_content;
	uint32_t offset_cur_list;
	prepare_send_callack_func_t prepare_callback;
	void * prepare_priv;
};

struct send_result_msg_content {
	uint32_t msgid;
};

struct testrr_msg_content {
	uint32_t msgid;
};

struct testbc_msg_content {
	uint32_t msgid;
};

struct testk3_msg_content {
	uint32_t msgid;
};

struct rtonboot_broadcast_receive_header {
	uint32_t is_receive;
};

void rk3568_board_initialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */


#endif
