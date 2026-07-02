#ifndef _RTONBOOT_IOCTL_H_
#define _RTONBOOT_IOCTL_H_

#ifdef __cplusplus 
extern "C" {
#endif

#include "rtonboot_ioctl_struct.h"

#define RTBRIDGE_IOCTL	'r'

#define RTBRIDGE_IOCTL_GET_RTONBOOT_BUFPARA _IOWR(RTBRIDGE_IOCTL, 0x80, struct rtonboot_bufpara)
#define RTBRIDGE_IOCTL_CREATE_MSG _IOW(RTBRIDGE_IOCTL, 0x81, struct rtonboot_create_msg_desc)
#define RTBRIDGE_IOCTL_SET_PENDING_STATUS _IOW(RTBRIDGE_IOCTL, 0x82, struct rtonboot_set_pending_status)
#define RTBRIDGE_IOCTL_DESTROY_MSG _IOW(RTBRIDGE_IOCTL, 0x83, struct rtonboot_destroy_msg_desc)
#define RTBRIDGE_IOCTL_SEND_MSG _IOW(RTBRIDGE_IOCTL, 0x84, struct rtonboot_send_msg_desc)
#define RTBRIDGE_IOCTL_SEND_RRMSG_RESP _IOW(RTBRIDGE_IOCTL, 0x85, struct rtonboot_rr_resp_desc)
#define RTBRIDGE_IOCTL_BROADCAST_RECEIVE _IOW(RTBRIDGE_IOCTL, 0x86, struct rtonboot_broadcast_receive_desc)

#define RTBRIDGE_IOCTL_GET_NET_PORT_IDX  _IOWR(RTBRIDGE_IOCTL, 0x88, struct rtonboot_ec_net_port_idx)

#define RTBRIDGE_IOCTL_SET_RTONBOOT_TRACE_LEVEL _IOW(RTBRIDGE_IOCTL, 0x89, struct rtonboot_set_trace_level)

#define RTBRIDGE_IOCTL_USER_RTONBOOT_TRACE _IOW(RTBRIDGE_IOCTL, 0x8a, struct rtonboot_user_trace_para)

#define RTBRIDGE_IOCTL_GET_SANERFA_PARA  _IOWR(RTBRIDGE_IOCTL, 0x8b, struct rtonboot_sanerfa_para)

#define RTBRIDGE_IOCTL_CPY_BUF  _IOWR(RTBRIDGE_IOCTL, 0x8c, struct rtonboot_cpybuf_para)

#define RTBULK_IOCTL	'u'

#define RTBULK_IOCTL_GET_RTBULK_BUFPARA _IOWR(RTBULK_IOCTL, 0x87, struct rtbulk_bufpara)

#define RTCNC_IOCTL	'c'

#define RTCNC_IOCTL_GET_RTCNC_BUFPARA _IOWR(RTCNC_IOCTL, 0x93, struct rtcnc_bufpara)

#ifdef __cplusplus 
}
#endif

#endif
