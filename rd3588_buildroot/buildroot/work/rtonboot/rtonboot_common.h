#ifndef _RTONBOOT_COMMON_H_
#define _RTONBOOT_COMMON_H_

#include <sys/ioctl.h> 
#include <stdint.h>

#define RTBRIDGE_DEVICE_NAME "rtbridge"

#define RTBRIDGE_IOCTL	'r'

#define MSG_ORIGIN_LINUX 0
#define MSG_ORIGIN_RTOS 1

#define MSG_HAS_RESPONSE 0
#define MSG_NO_RESPONSE 1

#define RESP_STATUS_SUCCESS 0
#define RESP_STATUS_NOMEM 1
#define RESP_STATUS_MSG_DUPLICATE 2
#define RESP_STATUS_NO_FOUND 3

struct rtonboot_bufpara {
	uint32_t total_size;
	uint32_t bulk_offset;
};

struct rtonboot_create_msg_desc {
	uint32_t pid;
	uint32_t dst_msgid;
	uint32_t max_content_size;
	uint8_t msg_origin;
	uint8_t msg_has_response;
	uint8_t is_broadcast;
	uint8_t is_kernel;
};

struct rtonboot_set_pending_status {
	uint32_t status;
};

struct rtonboot_destroy_msg_desc {
	uint32_t pid;
	uint32_t dst_msgid;
	uint32_t offset_cur_list;
};

struct rtonboot_send_msg_desc {
	uint32_t pid;
	uint32_t dst_msgid;
	uint32_t offset_cur_list;
	uint32_t msglen;
	uint8_t * p_content;
};

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

struct rtonboot_rr_resp_desc {
	uint32_t offset_content;
	uint32_t offset_cur_list;
	uint32_t msglen;
	uint32_t dst_msgid;
	uint8_t * p_resp_content;
};

struct rtonboot_rr_resp_header {
	uint32_t resp_status;
};

struct rtonboot_broadcast_receive_desc {
	uint32_t is_receive;
	uint32_t pid;
};

struct rtonboot_broadcast_receive_header {
	uint32_t is_receive;
};

struct rtonboot_bulkpara {
	uint32_t offset;
	uint32_t len;
};

#define MSGID_USER_BEGIN 3

#define MSGID_EXECCMD_ON_RTOS (MSGID_USER_BEGIN + 1)
#define MSGID_SEND_EXECCMD_RESULT (MSGID_USER_BEGIN + 2)
#define MSGID_TESTRR (MSGID_USER_BEGIN + 3)
#define MSGID_TESTBC (MSGID_USER_BEGIN + 4)
#define MSGID_TESTBC_DUMMY (MSGID_USER_BEGIN + 5)

#define MSGID_RTPUB_USER_BEGIN 9

#define MSGID_TESTK3 MSGID_RTPUB_USER_BEGIN
#define MSGID_LINUX_BULK_READY_MSG (MSGID_RTPUB_USER_BEGIN + 1)
#define MSGID_RTOS_BULK_READY_MSG (MSGID_RTPUB_USER_BEGIN + 2)



#define RTBRIDGE_IOCTL_GET_RTONBOOT_BUFPARA _IOWR(RTBRIDGE_IOCTL, 0x80, struct rtonboot_bufpara)
#define RTBRIDGE_IOCTL_CREATE_MSG _IOW(RTBRIDGE_IOCTL, 0x81, struct rtonboot_create_msg_desc)
#define RTBRIDGE_IOCTL_SET_PENDING_STATUS _IOW(RTBRIDGE_IOCTL, 0x82, struct rtonboot_set_pending_status)
#define RTBRIDGE_IOCTL_DESTROY_MSG _IOW(RTBRIDGE_IOCTL, 0x83, struct rtonboot_destroy_msg_desc)
#define RTBRIDGE_IOCTL_SEND_MSG _IOW(RTBRIDGE_IOCTL, 0x84, struct rtonboot_send_msg_desc)
#define RTBRIDGE_IOCTL_SEND_RRMSG_RESP _IOW(RTBRIDGE_IOCTL, 0x85, struct rtonboot_rr_resp_desc)
#define RTBRIDGE_IOCTL_BROADCAST_RECEIVE _IOW(RTBRIDGE_IOCTL, 0x86, struct rtonboot_broadcast_receive_desc)

#define RTBULK_IOCTL	'u'

struct rtbulk_bufpara {
	uint32_t total_size;
};

#define RTBULK_IOCTL_GET_RTBULK_BUFPARA _IOWR(RTBULK_IOCTL, 0x87, struct rtbulk_bufpara)

#endif
