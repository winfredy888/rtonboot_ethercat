#ifndef _RT_COMMON_INTERNAL_H_
#define _RT_COMMON_INTERNAL_H_

#ifdef __cplusplus 
extern "C" {
#endif

#define MSG_ORIGIN_LINUX 0
#define MSG_ORIGIN_RTOS 1

#define MSG_HAS_RESPONSE 0
#define MSG_NO_RESPONSE 1

#define RESP_STATUS_SUCCESS 0
#define RESP_STATUS_NOMEM 1
#define RESP_STATUS_MSG_DUPLICATE 2
#define RESP_STATUS_NO_FOUND 3

#define IS_KERNEL_SCRIPTING_NOTIFY 8

#define CREATE_MSG_TIMEOUT_MS 2000L
#define ASYNC_TIMER_EXPIRE_MS 1000L

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

struct rtonboot_broadcast_receive_header {
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

struct root_list_item {
	struct list_head root_list;
	struct list_head msg_list_head;
	uint32_t pid;
	uint32_t is_receive;
};


#ifdef __cplusplus 
}
#endif

#endif
