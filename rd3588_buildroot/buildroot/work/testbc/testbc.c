#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtsdk.h"

extern int rtonboot_shutdown;

struct rtonboot_create_msg_desc testbc_msgs[] = {
	{
		.dst_msgid = MSGID_TESTBC,
		.max_content_size = 64,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 1,
		.is_kernel = 0,
	},	
    
    { 	
		.dst_msgid = MSGID_TESTBC_DUMMY,
		.max_content_size = 64,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content)
{

}

void user_set_end_callback(uint32_t dst_msgid, rtsdk_end_callack_func_t * p_cb, void ** p_priv)
{
}

void user_set_resp_callback(uint32_t dst_msgid, rtsdk_resp_callack_func_t * p_cb, void ** p_priv)
{
}

void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv)
{
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
	
	ret = RTSDKCreateMsgs(testbc_msgs, 2);
	if(ret)
	{
		printf("Winfred Young create testbc msgs fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	ret = RTSDKReceiveBroadcast();
	if(ret)
	{
		printf("Winfred Young RTSDKReceiveBroadcast fail\n");
		
		ret = -1;
		
		goto destroy_and_uninit;
	}
	
	while (!rtonboot_shutdown)
    {
		delay_us(2);
		
		 ++i;
		 --i;
    };
	
destroy_and_uninit:

     RTSDKDeleteAllMsg();
	
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
