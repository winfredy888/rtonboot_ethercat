#ifndef _RTONBOOT_IOCTL_STRUCT_H_
#define _RTONBOOT_IOCTL_STRUCT_H_

#ifdef __cplusplus 
extern "C" {
#endif

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
	uint8_t flush_state;
	uint32_t flush_start;
	uint32_t flush_size;
	uint8_t * p_content;
};

struct rtonboot_rr_resp_desc {
	uint32_t offset_content;
	uint32_t offset_cur_list;
	uint32_t msglen;
	uint32_t dst_msgid;
	uint8_t * p_resp_content;
};

struct rtonboot_broadcast_receive_desc {
	uint32_t is_receive;
	uint32_t pid;
};

struct rtonboot_ec_net_port_idx {
	int32_t net_port_idx;
};

struct rtonboot_set_trace_level {
	uint32_t level;
};

struct rtonboot_user_trace_para {
	uint32_t level;
	char * fmt;
	uint32_t len;
};

struct rtonboot_sanerfa_para {
	uint8_t sanerfa_para[32];
};

struct rtonboot_cpybuf_para {
	uint32_t buf_off;
	uint32_t buf_len;
	char * bufptr;
};

struct rtbulk_bufpara {
	uint32_t total_size;
};

#ifdef __cplusplus 
}
#endif

#endif
