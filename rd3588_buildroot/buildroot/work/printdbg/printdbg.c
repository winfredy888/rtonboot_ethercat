#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtsdk.h"

#define RTONBOOT_BULK_BUFFER_SIZE (2L * 1024L * 1024L)

extern int rtonboot_shutdown;

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

unsigned char * dbgptr;

void ShowNormalDebug(void)
{
	char * ptr;
	int count_debug_msg;
	int i = 0;
	int len;
	
	count_debug_msg = *((uint32_t *)dbgptr);
	
	ptr = (char *)(dbgptr + 4);
	
	for(i = 0; i < count_debug_msg; ++i)
	{
		printf("%s", ptr);
		
		len = strlen(ptr);
		
		ptr += len + 1;
	}	
}	

#define RTONBOOT_BULK_BUFFER_SIZE (2L * 1024L * 1024L)

void ShowRTOnBootTrace(void)
{
	char * ptr;
	char * ptr1;
	char * ptr2;
	char * ptr_begin;
	char * ptr_end;
	int min_loc;
	int max_loc;
	int len;
	int total_len;
	int buf_len;
	int min_cnt;
	char temp;
	int cnt;
	
	ptr_begin = (char *)(dbgptr + 8);
	ptr = ptr_begin;
	ptr_end = (char *)(dbgptr + RTONBOOT_BULK_BUFFER_SIZE);
	
	min_loc = 0;
	max_loc = 0;
	total_len = 0;
	min_cnt = 0;
	buf_len = RTONBOOT_BULK_BUFFER_SIZE - 8;
	
	while( (total_len < buf_len) )
	{
		if( (ptr + 4) >= ptr_end )
			break;
			
		temp = ptr[4];
		ptr[4] = 0;
		
		if(!strcmp(ptr, "CNT:"))
		{
			ptr[4] = temp;
			
			ptr1 = ptr + 5;
			ptr2 = ptr1;
			
			if( ptr2 >= ptr_end )
					break;
			
			while(isdigit(*ptr2))
			{
				++ptr2;
				
				if( ptr2 >= ptr_end )
					goto end_loop;
			};
			
			temp = *(ptr2);
			
			*(ptr2) = 0;
			
			cnt = atoi(ptr1);
			
			*(ptr2) = temp;
			
			if(cnt >= 2097153)
			{
				max_loc = ptr - ptr_begin;
			}
			else if( (ptr == ptr_begin) && (min_cnt == 0) )
			{
				min_cnt = cnt;
			}
			else if( (cnt) && (cnt < min_cnt) )
			{
				min_cnt = cnt;
				
				min_loc = ptr - ptr_begin;
			}
		}
		else
		{
			ptr[4] = temp;
		}
		
		len = strlen(ptr);
		
		total_len += len + 1;
		
		ptr += len + 1;		
	}	
	
end_loop:
	
	total_len = min_loc;
	ptr = ptr_begin + min_loc;
	
	while( (total_len < buf_len) )
	{
		printf("%s\n", ptr);
		
		len = strlen(ptr);
		
		total_len += len + 1;
		
		ptr += len + 1;
		
		if( (!min_loc) && (max_loc) && (total_len >= max_loc) )
		{
			break;
		}		
	};
	
	if(min_loc)
	{
		total_len = 0;
		ptr = ptr_begin;
		
		while( (total_len < min_loc) )
		{
			printf("%s\n", ptr);
		
			len = strlen(ptr);
		
			total_len += len + 1;
		
			ptr += len + 1;		
		};
	}		
}	
 
int main(int argc , char *argv[])
{	
	int ret;
	int is_rtonboot_trace = 0;
		
	if( (argc > 1) && (!strcmp(argv[1], "-t")) )
	{
		is_rtonboot_trace = 1;
	}	
	
	ret = RTSDKBeginInit();	
    if(ret)
    {
		 ret = -1;
		 
		 goto just_uninit; 
	}
	
	ret = RTSDKUseBulk();	
	if(ret)
	{
		printf("Winfred Young RTSDKUseBulk fail\n");
		
		ret = -1;
		
		goto just_uninit;
	}
	
	dbgptr = RTSDKGetDebugBufferPtr();
	
	if(is_rtonboot_trace)
	{
		ShowRTOnBootTrace();
	}
	else
	{
		ShowNormalDebug();
	}	
	
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
