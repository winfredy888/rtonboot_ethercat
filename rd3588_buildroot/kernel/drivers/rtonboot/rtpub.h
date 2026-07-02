#ifndef _RTPUB_H_
#define _RTPUB_H_

#include <linux/stmmac.h>

#include "../net/ethernet/stmicro/stmmac/stmmac_platform.h"

#define INCLUDE_NONPUB 1

#include "msgid.h"

#include "msg_common_struct.h"


typedef void (*kcallack_func_t)(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv);

extern int32_t get_prepare_kernel_max_content_size(uint32_t msgid);

extern uint8_t * prepare_sendmsg_content_in_kernel(uint32_t msgid, uint32_t * p_msglen);

extern void set_kernel_callback_according_msgid(uint32_t msgid, uint64_t * p_callback, uint64_t * p_priv);

#endif
