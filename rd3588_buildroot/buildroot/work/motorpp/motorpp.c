#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtsdk.h"

#define ADSB3 1

#undef ADSA2

extern int rtonboot_shutdown;

int is_ec_ready = 0;

struct rtonboot_ec_ready_status ec_ready_status;

struct rtonboot_start_motorpp start_motorpp;

struct rtonboot_stop_motorpp stop_motorpp;

struct rtonboot_motorpp_update_pos motorpp_update_pos;

int32_t curpos = 100000;

struct rtonboot_create_msg_desc motorpp_msgs[] = {
	{
		.dst_msgid = MSGID_EC_MASTER_READY,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},	
    
    { 	
		.dst_msgid = MSGID_START_MOTORPP,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
	
	{ 	
		.dst_msgid = MSGID_STOP_MOTORPP,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
	
	{ 	
		.dst_msgid = MSGID_MOTORPP_UPDATE_POS,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
};

void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content)
{

}

void end_callack_ec_master_ready(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_ec_ready_status * p_status;
	
	p_status = (struct rtonboot_ec_ready_status *)(p_content);
	
	is_ec_ready = p_status->is_ready;
}

void user_set_end_callback(uint32_t dst_msgid, rtsdk_end_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_EC_MASTER_READY:
		
			*p_cb = &end_callack_ec_master_ready;
			
			*p_priv = NULL;
		
			break;
	};
}

void user_set_resp_callback(uint32_t dst_msgid, rtsdk_resp_callack_func_t * p_cb, void ** p_priv)
{
}

void prepare_callack_ec_master_ready(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	ec_ready_status.is_ready = 0;
	ec_ready_status.status = 0;
	
	*(pp_content) = (uint8_t *)(&ec_ready_status);
}

void prepare_callack_start_motorpp(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	start_motorpp.is_start = 1;
	start_motorpp.status = 0;
	
	*(pp_content) = (uint8_t *)(&start_motorpp);
}

void prepare_callack_stop_motorpp(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	stop_motorpp.is_stop = 1;
	stop_motorpp.status = 0;
	
	*(pp_content) = (uint8_t *)(&stop_motorpp);
}

void prepare_callack_motorpp_update_pos(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	motorpp_update_pos.newpos = curpos;
	
	*(pp_content) = (uint8_t *)(&motorpp_update_pos);
}

void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_EC_MASTER_READY:
		
			*p_cb = &prepare_callack_ec_master_ready;
			
			*p_priv = NULL;
		
			break;
		
		case MSGID_START_MOTORPP:
		
			*p_cb = &prepare_callack_start_motorpp;
			
			*p_priv = NULL;
		
			break;
			
		case MSGID_STOP_MOTORPP:
		
			*p_cb = &prepare_callack_stop_motorpp;
			
			*p_priv = NULL;
		
			break;	
			
		case MSGID_MOTORPP_UPDATE_POS:
		
			*p_cb = &prepare_callack_motorpp_update_pos;
			
			*p_priv = NULL;
		
			break;
	};
}

void my_delay_us(long micro_seconds)
{
    struct timeval tv;
    
    tv.tv_sec = micro_seconds / 1000000L;
    tv.tv_usec = micro_seconds % 1000000L;
 
    select(0, NULL, NULL, NULL, &tv);
}

void my_delay_sec(long seconds)
{
    struct timeval tv;
    
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
 
    select(0, NULL, NULL, NULL, &tv);
}
 
int main(int argc , char *argv[])
{
	int ret = 0;
	int i = 0;
		
	ret = RTSDKBeginInit();
    if(ret)
    {
		 ret = -1;
		 
		 goto just_uninit; 
	}
	
	/*ret = RTSDKSetRtonbootTraceLevel(RTONBOOT_TRACE_LEVEL_DETAIL);
	if(ret)
	{
		printf("Winfred Young RTSDKSetRtonbootTraceLevel fail\n");
		
		ret = -1;
		 
		goto just_uninit;
	}*/
	
	ret = RTSDKCreateMsgs(motorpp_msgs, 4);
	if(ret)
	{
		printf("Winfred Young create motorpp_msgs msgs fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	/*ret = RTSDKReceiveBroadcast();
	if(ret)
	{
		printf("Winfred Young RTSDKReceiveBroadcast fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}*/
	
	ret = RTSDKSendMsg(MSGID_EC_MASTER_READY);
	if(ret)
	{
		printf("Winfred Young send start ec master ready msg fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	my_delay_us(10000);
	
	if(!is_ec_ready)
	{
		printf("EtherCAT master in nuttx is not ready, just quit\n");
		
		ret = 0;
		
		goto destroy_and_uninit;
	}
	
	printf("EtherCAT begin send MSGID_START_MOTORPP\n");
	
	ret = RTSDKSendMsg(MSGID_START_MOTORPP);
	if(ret)
	{
		printf("Winfred Young send start motorpp msg fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	my_delay_sec(8);
	
	while (!rtonboot_shutdown)
    {
		my_delay_sec(1);
		
		RTSDKSendMsg(MSGID_MOTORPP_UPDATE_POS);
		
		#if defined (ADSB3)
	
			curpos += 1000000000;
		
		#elif defined (ADSA2)
	
			curpos += 100000;
		
		#else
	
			#error "Unsopport Servo type"
	
		#endif
    };
    
    ret = RTSDKSendMsg(MSGID_STOP_MOTORPP);
	if(ret)
	{
		printf("Winfred Young send stop motorpp msg fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	my_delay_us(1000);
	
destroy_and_uninit:

     RTSDKDeleteAllMsg();
     
     /*RTSDKSetRtonbootTraceLevel(0);*/
	
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
