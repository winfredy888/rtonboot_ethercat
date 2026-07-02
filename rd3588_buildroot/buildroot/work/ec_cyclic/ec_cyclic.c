#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtsdk.h"

extern int rtonboot_shutdown;

int is_ec_ready = 0;

struct rtonboot_ec_ready_status ec_ready_status;

struct rtonboot_start_ec_cyclic start_ec_cyclic;

struct rtonboot_stop_ec_cyclic stop_ec_cyclic;

struct rtonboot_create_msg_desc ec_cyclic_msgs[] = {
	{
		.dst_msgid = MSGID_EC_MASTER_READY,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},	
    
    { 	
		.dst_msgid = MSGID_START_EC_CYCLIC,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
	
	{ 	
		.dst_msgid = MSGID_STOP_EC_CYCLIC,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
	
	{ 	
		.dst_msgid = MSGID_EC_CYCLIC_PERF,
		.max_content_size = 52,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}
};

void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content)
{

}

void end_callack_ec_perf(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_ec_cyclic_perf * p_perf;
	
	p_perf = (struct rtonboot_ec_cyclic_perf *)(p_content);
	
	printf("period     %10u ... %10u\n",
		(p_perf->period_min_ns), (p_perf->period_max_ns) );
		
    printf("exec       %10u ... %10u\n",
         (p_perf->exec_min_ns), (p_perf->exec_max_ns) );
         
    printf("latency    %10u ... %10u\n",
         (p_perf->latency_min_ns), (p_perf->latency_max_ns) );
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
			
		case MSGID_EC_CYCLIC_PERF:
		
			*p_cb = &end_callack_ec_perf;
			
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

void prepare_callack_start_ec_cyclic(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	start_ec_cyclic.is_start = 1;
	start_ec_cyclic.status = 0;
	
	*(pp_content) = (uint8_t *)(&start_ec_cyclic);
}

void prepare_callack_stop_ec_cyclic(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 4;
	
	stop_ec_cyclic.is_stop = 1;
	stop_ec_cyclic.status = 0;
	
	*(pp_content) = (uint8_t *)(&stop_ec_cyclic);
}

void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_EC_MASTER_READY:
		
			*p_cb = &prepare_callack_ec_master_ready;
			
			*p_priv = NULL;
		
			break;
		
		case MSGID_START_EC_CYCLIC:
		
			*p_cb = &prepare_callack_start_ec_cyclic;
			
			*p_priv = NULL;
		
			break;
			
		case MSGID_STOP_EC_CYCLIC:
		
			*p_cb = &prepare_callack_stop_ec_cyclic;
			
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
	
	ret = RTSDKCreateMsgs(ec_cyclic_msgs, 4);
	if(ret)
	{
		printf("Winfred Young create ec_cyclic_msgs msgs fail\n");
		
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
	
	/*ret = RTSDKSendMsg(MSGID_EC_MASTER_READY);
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
	}*/
	
	ret = RTSDKSendMsg(MSGID_START_EC_CYCLIC);
	if(ret)
	{
		printf("Winfred Young send start ec_cyclic msg fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	while (!rtonboot_shutdown)
    {
		my_delay_us(2);
		
		 ++i;
		 --i;
    };
    
    ret = RTSDKSendMsg(MSGID_STOP_EC_CYCLIC);
	if(ret)
	{
		printf("Winfred Young send stop ec_cyclic msg fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	my_delay_us(1000);
	
destroy_and_uninit:

     RTSDKDeleteAllMsg();
	
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
