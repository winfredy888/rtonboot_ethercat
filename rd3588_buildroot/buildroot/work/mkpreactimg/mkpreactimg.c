#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "rtsdk.h"

extern int rtonboot_shutdown;

extern unsigned char * p_map;

#define FILENAME_PREACT "/data/preact.img"

#define PREACT_MAGIC "RTOnBoot PreACT Image Header"

#define FILESIZE_PREACT 2048

#define USER_SHARED_BUF_FIXVAR_SIZE 1024
#define OFFSET_TO_RTONBOOT_SHARE_BUFFER 0xc00000

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

void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

char buf[100];
 
int main(int argc , char *argv[])
{
	int ret = 0;
	int i = 0;
	int fd_preact;
	uint8_t * ptr_ciper_serial;
	uint8_t sanerfa_para[32];
	int is_sanerfa_all_zero;
	uint32_t pos; 
	uint32_t len; 
	
	ret = RTSDKBeginInit();
    if(ret)
    {
		 ret = -1;
		 
		 goto just_uninit; 
	}
		
	/*while (!shutdown)
    {
		delay_us(2);
		
		 ++i;
		 --i;
    };*/
    
    fd_preact = open(FILENAME_PREACT, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd_preact < 0) 
	{
		printf("Winfred Young cannot open pre act img file\n");
		
		goto just_uninit; 
	}
	
	close(fd_preact);
	
	ptr_ciper_serial = p_map + USER_SHARED_BUF_FIXVAR_SIZE - 16;
			
	/*print_hex(ptr_ciper_serial, 16);*/
	
	ret = RTSDKGetSanerfaPara(&sanerfa_para[0]);
	if(ret)
	{
		printf("Winfred Young ger para fail\n");
		
		ret = -1;
		
		goto just_uninit;
	}
	
	is_sanerfa_all_zero = 1;
	
	for(i = 0; i < 32; ++i)
	{
		if(sanerfa_para[i])
		{
			is_sanerfa_all_zero = 0;
			
			break;
		}	
	}	
	
	fd_preact = open(FILENAME_PREACT, O_RDWR);
	if (fd_preact < 0) 
	{
		printf("Winfred Young cannot open pre act img file for rw\n");
		
		goto just_uninit; 
	}
	
	pos = 0;
	
	srand(time(NULL));
	
	lseek(fd_preact, pos, SEEK_SET);
	
	len = strlen(PREACT_MAGIC);
	
	strcpy(&buf[0], PREACT_MAGIC);
	
	buf[len] = 0;
	
	for(i = len + 1; i < 100; ++i)
	{
		buf[i] = rand() % 256;
	}
	
	write(fd_preact, buf, 100);
	
	pos = 100;
	
	lseek(fd_preact, pos, SEEK_SET);
	
	for(i = 0; i < 36; ++i)
	{
		buf[i] = rand() % 256;
	}
	
	for(i = 36; i < 52; ++i)
	{
		buf[i] = ptr_ciper_serial[i - 36];
	}
	
	if(is_sanerfa_all_zero)
	{
		for(i = 52; i < 84; ++i)
		{
			buf[i] = rand() % 256;
		}
	}	
	else
	{
		for(i = 52; i < 84; ++i)
		{
			buf[i] = sanerfa_para[i - 52];
		}
	}	
	
	for(i = 84; i < 100; ++i)
	{
		buf[i] = rand() % 256;
	}
	
	write(fd_preact, buf, 100);
	
	pos += 100;
	
	for(;;)
	{
		lseek(fd_preact, pos, SEEK_SET);
		
		if( (pos + 100) > FILESIZE_PREACT)
		{
			len = FILESIZE_PREACT - pos;
		}
		else
		{
			len = 100;
		}
		
		for(i = 0; i < len; ++i)
		{
			buf[i] = rand() % 256;
		}
		
		write(fd_preact, buf, len);
		
		pos += len;
		
		if(pos >= FILESIZE_PREACT)
			break;
	}	
	
	close(fd_preact);
		
just_uninit:     

     RTSDKUnInit();
     
     return ret;
}
