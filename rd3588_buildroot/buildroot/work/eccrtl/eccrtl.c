#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtsdk.h"

extern int rtonboot_shutdown;

struct rtonboot_create_msg_desc eccrtl_msgs[] = {
	{
		.dst_msgid = MSGID_START_EC,
		.max_content_size = 0,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 3,
	},	
    
    { 	
		.dst_msgid = MSGID_CLEANUP_AFTER_EC,
		.max_content_size = 32,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content)
{

}

void end_callack_cleanup_after_ec(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
}


void user_set_end_callback(uint32_t dst_msgid, rtsdk_end_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_CLEANUP_AFTER_EC:
		
			*p_cb = &end_callack_cleanup_after_ec;
			
			*p_priv = NULL;
		
			break;
	};
}

void user_set_resp_callback(uint32_t dst_msgid, rtsdk_resp_callack_func_t * p_cb, void ** p_priv)
{
}

void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv)
{
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
	
	usleep(10000000);
	
	ret = RTSDKBeginInit();
    if(ret)
    {
		 ret = -1;
		 
		 goto just_uninit; 
	}
	
    ret = RTSDKCheckECNetPortIdx();
    if(ret)
    {
		printf("Winfred Young invalid net port idx do not start ec msgs fail\n");
		
		ret = -1;
		
		goto just_uninit;
	}	
	
	ret = RTSDKCreateMsgs(eccrtl_msgs, 2);
	if(ret)
	{
		printf("Winfred Young create eccrtl_msgs msgs fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	ret = RTSDKSendMsg(MSGID_START_EC);
	if(ret)
	{
			printf("Winfred Young send start_ec msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
	}
	
	while (!rtonboot_shutdown)
    {
		my_delay_us(10);
		
		 ++i;
		 --i;
    };
	
destroy_and_uninit:

     RTSDKDeleteAllMsg();
	
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
