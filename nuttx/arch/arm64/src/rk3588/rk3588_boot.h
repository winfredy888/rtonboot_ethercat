#ifndef __ARCH_ARM64_SRC_RK3588_RK3588_BOOT_H
#define __ARCH_ARM64_SRC_RK3588_RK3588_BOOT_H

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

#include "rt_common_internal.h"

#include "offset_common.h"

/*#define RTONBOOT_DEBUG 1*/

#undef RTONBOOT_DEBUG

#define ALLOW_CREATE_DUPLICATE 1

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

#define PID_BROADCAST_MSG 0xffffffff
#define PID_KERNEL_MSG 0xfffffffe 

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

void rk3588_board_initialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */


#endif
