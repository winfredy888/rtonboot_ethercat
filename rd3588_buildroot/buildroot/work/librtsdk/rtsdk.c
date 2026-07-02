#include  <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h> 
#include <signal.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>

#include "rtsdk_priv.h"
#include "rtsdk.h"

int is_creating_msg = 0;

int use_bulk = 0;

int fd_bridge = -1;
int fd_bulk = -1;
int rtonboot_shutdown = 0;

unsigned char * p_map = NULL;
unsigned char * p_bulk = NULL;

struct rtonboot_bufpara bufpara;
struct rtbulk_bufpara bulk_para;

user_sig_callack_func_t user_sig_callack = NULL;

sem_t g_create_msg_sem;

struct list_head rtsdk_msg_list_head;

pthread_mutex_t g_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t g_readmsg_thread;

int g_continue_readmsg = 1;

pthread_rwlock_t g_prepare_rwlock;
pthread_rwlock_t g_resp_rwlock;
pthread_rwlock_t g_end_rwlock;


extern void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content);

void rtsdk_call_user_bc_callback(uint32_t msgid, struct rtonboot_msg_header * p_header)
{
	uint8_t * p_content;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	p_content = (uint8_t *)(p_map + (p_header->offset_content) );
			
	user_broadcast_callback(msgid, p_header->msg_len, p_content);
}

int rtsdk_call_end_callback(uint32_t msgid, struct rtonboot_msg_header * p_header)
{
	struct rtsdk_msg_list_item * p_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	uint8_t * p_content;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	p_head = &(rtsdk_msg_list_head);
		
	if( list_empty(p_head) )
	{
		printf("Winfred Young list empty call end fail\n");
		
		RTSDKRtonbootTraceError("in %s list empty call end fail", __func__);
		
		return ERROR_NUM_LIST_EMPTY_CALL_END_FAIL;
	}		
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_item = (struct rtsdk_msg_list_item *)pTemp;
		
		if( (p_item->msgid) == msgid)
			goto find_on_call_end;
	}
	
	printf("Winfred Young can not find on list call end fail\n");
	
	RTSDKRtonbootTraceError("in %s can not find on list call end fail", __func__);
	
	return ERROR_NUM_NOT_FOUND_CALL_END_FAIL;
	
	find_on_call_end:
		
		if((p_item->rtsdk_end_callack))
		{
			p_content = (uint8_t *)(p_map + (p_item->offset_content) );
	
			pthread_rwlock_wrlock(&g_end_rwlock);
			
			(p_item->rtsdk_end_callack)(msgid, p_header->msg_len,
				p_content, p_item->rtsdk_end_callack_priv);
				
			pthread_rwlock_unlock(&g_end_rwlock);	
		}
		
		return ERROR_NUM_OK;
}

int rtsdk_call_resp_callback(uint32_t msgid, struct rtonboot_msg_header * p_header)
{
	struct rtsdk_msg_list_item * p_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct rtonboot_rr_resp_desc rr_resp_desc;
	int ret;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	p_head = &(rtsdk_msg_list_head);
		
	if( list_empty(p_head) )
	{
		printf("Winfred Young list empty call resp fail\n");
		
		RTSDKRtonbootTraceError("in %s list empty call resp fail", __func__);
		
		return ERROR_NUM_LIST_EMPTY_CALL_RESP_FAIL;
	}		
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_item = (struct rtsdk_msg_list_item *)pTemp;
		
		if( (p_item->msgid) == msgid)
			goto find_on_call_resp;
	}
	
	printf("Winfred Young can not find on list call resp fail\n");
	
	RTSDKRtonbootTraceError("in %s can not find on list call resp fail", __func__);
	
	return ERROR_NUM_NOT_FOUND_CALL_RESP_FAIL;
	
	find_on_call_resp:
	
		RTSDKRtonbootTraceDetail("in %s rr_resp_desc.offset_content %d offset_cur_list %d", 
			__func__, p_item->offset_content, p_item->offset_cur_list);
	
		rr_resp_desc.offset_content = p_item->offset_content;
		rr_resp_desc.offset_cur_list = p_item->offset_cur_list;
		rr_resp_desc.dst_msgid = msgid;
		
		if((p_item->rtsdk_resp_callack))
		{	
			pthread_rwlock_rdlock(&g_resp_rwlock);
			
			(p_item->rtsdk_resp_callack)(msgid, &(rr_resp_desc.msglen), 
				&(rr_resp_desc.p_resp_content), p_item->max_content_size,
				p_item->rtsdk_resp_callack_priv);
				
			pthread_rwlock_unlock(&g_resp_rwlock);	
		}
		else
		{
			rr_resp_desc.msglen = 0;
			rr_resp_desc.p_resp_content = NULL;
		}
		
		ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_SEND_RRMSG_RESP, &rr_resp_desc);
		if(ret < 0)
		{
			printf("Winfred Young bridge rr resp iovtl fail\n");
			
			RTSDKRtonbootTraceError("in %s RTBRIDGE_IOCTL_SEND_RRMSG_RESP fail ret %d", 
				__func__, ret);
			
			return ERROR_NUM_RR_RESP_IOCTL_FAIL;
		}	
				
		return ERROR_NUM_OK;
}

void handle_notify_common_process(struct rtonboot_msg_header * p_header)
{
	struct rtonboot_set_pending_status pending_status;
	int ret;
	
	if( (p_header->msgid) == (MSGID_CREATE_MSG) )
	{
		is_creating_msg = 0;
		
		RTSDKRtonbootTraceDetail("in %s is_creating_msg = 0", 
				__func__);
		
		goto zero_pending_status;
	}
	else
	{
		if(p_header->is_broadcast)
		{
			rtsdk_call_user_bc_callback(p_header->msgid, p_header);

			goto zero_pending_status;
		}
		else if( ( (p_header->msg_origin == MSG_ORIGIN_RTOS) && (p_header->msg_has_response == MSG_NO_RESPONSE) ) ||
			( (p_header->msg_origin == MSG_ORIGIN_LINUX) && (p_header->msg_has_response == MSG_HAS_RESPONSE) ) )
		{	
			rtsdk_call_end_callback(p_header->msgid, p_header);
			
			goto zero_pending_status;
		}
		else if( (p_header->msg_origin == MSG_ORIGIN_RTOS) && (p_header->msg_has_response == MSG_HAS_RESPONSE) )
		{
			rtsdk_call_resp_callback(p_header->msgid, p_header);
			
			return;
		}
		else
		{
			return;
		}		
	}		
	
	zero_pending_status:
	
		pending_status.status = 0;
	
		ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_SET_PENDING_STATUS, &pending_status);
		if(ret < 0)
		{
			printf("Winfred Young bridge SET_PENDING_STATUS fail\n");
			
			RTSDKRtonbootTraceError("in %s RTBRIDGE_IOCTL_SET_PENDING_STATUS fail ret %d", 
				__func__, ret);
		}
		else
		{
			#ifdef RTONBOOT_DEBUG
			
				printf("Winfred Young set pending status success\n");
				
			#endif
			
			RTSDKRtonbootTraceDetail("in %s set pending status success", 
				__func__);
		}	
}	

void handle_sigio(void)
{	
	unsigned char * ptr_msg_header;
	struct rtonboot_msg_header * p_header;
	
	ptr_msg_header = p_map + 4;
	p_header = (struct rtonboot_msg_header *)ptr_msg_header;
	
	handle_notify_common_process(p_header);	
}

void catch_signal(int signo)
{
	if(signo == SIGIO)
	{
		#ifdef RTONBOOT_DEBUG
		
			printf("Winfred Young catch sigio\n");
			
		#endif	
		
		RTSDKRtonbootTraceDetail("in %s catch sigio", 
				__func__);
		
		handle_sigio();
	}
	else if( (signo == SIGTERM) ||
		(signo == SIGINT) ||
		(signo == SIGHUP) )
	{
		#ifdef RTONBOOT_DEBUG
		
			printf("Winfred Young catch shudown sig\n");
			
		#endif	
		
		RTSDKRtonbootTraceDetail("in %s catch shudown sig", 
				__func__);
		
		rtonboot_shutdown = 1;
	}
	else
	{
		RTSDKRtonbootTraceDetail("in %s call user_sig_callack", 
				__func__);
		
		if(user_sig_callack)
		{
			user_sig_callack(signo);
		}	
	}	
}

void * readmsgThread(void *arg)
{
	struct timeval timeout;
	int ret;
	fd_set readfds;
	uint32_t ms;
	uint32_t sec;
	uint32_t usec;
	uint32_t val;
	unsigned char * ptr_msg_header;
	struct rtonboot_msg_header * p_header;
	
	ptr_msg_header = p_map + 4;
	p_header = (struct rtonboot_msg_header *)ptr_msg_header;
	
	ms = CREATE_MSG_TIMEOUT_MS;
	if(ms < ASYNC_TIMER_EXPIRE_MS)
		ms = ASYNC_TIMER_EXPIRE_MS;
	
	sec = ms / 1000;
	usec = ms % 1000;
	usec *= 1000;
	
    while(g_continue_readmsg)
    {
		FD_ZERO(&readfds);
		FD_SET(fd_bridge, &readfds); 
			
		timeout.tv_sec = sec;
		timeout.tv_usec = usec;
		
		ret = select(fd_bridge + 1, &readfds, NULL, NULL, &timeout);
		if (ret == -1) 
		{
			RTSDKRtonbootTraceError("readmsgThread select fail");
			
			continue;
		} 
		else if (ret) 
		{
			if (FD_ISSET(fd_bridge, &readfds)) 
			{
				read(fd_bridge, &val, 4);		 	 
				
				if( val != (p_header->msgid) )
				{
					RTSDKRtonbootTraceError("readmsgThread read check error val is %d msgid is %d", val, p_header->msgid);
				}
				else
				{
					handle_notify_common_process(p_header);
				}		
			}	
		} 
		else 
		{
			continue;
		}	
	};
	
	return NULL;	
}

int RTSDKBeginInit(void)
{
	int ret;
	int flags;
	
	if (SIG_ERR == signal(SIGIO, catch_signal))
	{
        printf("Winfred Young signal failed\n");
        
        return ERROR_NUM_SIGNAL_FAIL;
    }
    
    signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);
    signal(SIGHUP, catch_signal);
    
    fd_bridge = open("/dev/rtbridge", O_RDWR);
    if(fd_bridge < 0)
    {
         printf("Winfred Young open bridge deivice fail\n");
         
         return ERROR_NUM_OPEN_BRIDGE_FAIL;
    }
     
     ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_GET_RTONBOOT_BUFPARA, &bufpara);
     if(ret < 0)
     {
		 printf("Winfred Young bridge get bufpara fail\n");
		 
		 close(fd_bridge);
		 
		 fd_bridge = -1;
         
         return ERROR_NUM_GET_BUF_PARA_FAIL;
     }
     
     #ifdef RTONBOOT_DEBUG
     
		printf("Winfred Young get bufpara total_size %lx offset_bulk is %lx\n", bufpara.total_size, bufpara.bulk_offset);
		
	 #endif
 
     p_map = (unsigned char *)mmap(0, bufpara.total_size, PROT_READ /*| PROT_WRITE*/, MAP_SHARED, fd_bridge, 0);
     if(p_map == MAP_FAILED)
     {
         printf("Winfred Young mmap fail\n");
         
         close(fd_bridge);
         
         fd_bridge = -1;
         
         return ERROR_NUM_BRIDGE_MMAP_FAIL;
     }
     
     fcntl(fd_bridge, F_SETOWN, getpid());
	 flags = fcntl(fd_bridge, F_GETFL);
	 fcntl(fd_bridge, F_SETFL, flags | O_ASYNC);
	 
	 INIT_LIST_HEAD(&rtsdk_msg_list_head);
	 
	 ret = pthread_create(&g_readmsg_thread, NULL, readmsgThread, NULL);
	 if (ret != 0) 
	 {
		 printf("Winfred Young create readmsgThread fail\n");
        
        munmap(p_map, bufpara.total_size);
			
		p_map = NULL;
     
		close(fd_bridge);
		
		fd_bridge = -1;
        
        return ERROR_CREATE_READMSG_THREAD_FAIL;
	 }
	 
	 g_continue_readmsg = 1;
	 
	 pthread_rwlock_init(&g_prepare_rwlock, NULL);
	 
	 pthread_rwlock_init(&g_resp_rwlock, NULL);
	 
	 pthread_rwlock_init(&g_end_rwlock, NULL);
          	
     return ERROR_NUM_OK;
}

#define DEBUG_BUFFER_OFFSET 0x120000L

unsigned char * RTSDKGetDebugBufferPtr(void)
{
	return (p_bulk);
}	

void rtsdk_init_item_and_add_to_list(struct rtsdk_msg_list_item * p_item, struct rtonboot_msg_header * p_header,
	struct rtonboot_resp_header * p_resp, struct rtonboot_create_msg_desc * p_desc)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	memset(p_item, 0, sizeof(struct rtsdk_msg_list_item));
	
	p_item->msgid = p_desc->dst_msgid;
	
	p_item->max_content_size = p_desc->max_content_size;
	
	p_item->offset_content = p_resp->offset_content;
	
	p_item->offset_cur_list = p_resp->offset_cur_list;
	
	p_item->is_kernel = p_desc->is_kernel;
	
	pthread_mutex_lock(&g_list_mutex);
	
	list_add_tail(&(p_item->msg_list), &(rtsdk_msg_list_head));
	
	pthread_mutex_unlock(&g_list_mutex);
}

extern void user_set_end_callback(uint32_t dst_msgid, rtsdk_end_callack_func_t * p_cb, void ** p_priv);

void rtsdk_set_end_callback(uint32_t dst_msgid, struct rtsdk_msg_list_item * p_item)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	user_set_end_callback(dst_msgid, &(p_item->rtsdk_end_callack), &(p_item->rtsdk_end_callack_priv) );
}

extern void user_set_resp_callback(uint32_t dst_msgid, rtsdk_resp_callack_func_t * p_cb, void ** p_priv);

void rtsdk_set_resp_callback(uint32_t dst_msgid, struct rtsdk_msg_list_item * p_item)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	user_set_resp_callback(dst_msgid, &(p_item->rtsdk_resp_callack), &(p_item->rtsdk_resp_callack_priv) );
}

extern void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv);

void rtsdk_set_prepare_callback(uint32_t dst_msgid, struct rtsdk_msg_list_item * p_item)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	user_set_prepare_callback(dst_msgid, &(p_item->rtsdk_prepare_send_callack), 
		&(p_item->rtsdk_prepare_send_callack_priv) );
}	

void rtsdk_set_callbacks(struct rtsdk_msg_list_item * p_item, struct rtonboot_create_msg_desc * p_desc)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	if(p_desc->is_broadcast)
		return;
		
	if( ( (p_desc->msg_origin == MSG_ORIGIN_RTOS) && (p_desc->msg_has_response == MSG_NO_RESPONSE) ) ||
		( (p_desc->msg_origin == MSG_ORIGIN_LINUX) && (p_desc->msg_has_response == MSG_HAS_RESPONSE) ) )
	{	
		rtsdk_set_end_callback(p_desc->dst_msgid, p_item);
	}
	else if( (p_desc->msg_origin == MSG_ORIGIN_RTOS) && (p_desc->msg_has_response == MSG_HAS_RESPONSE) )
	{
		rtsdk_set_resp_callback(p_desc->dst_msgid, p_item);
	}
	
	if( (p_desc->msg_origin == MSG_ORIGIN_LINUX) ) 
	{
		rtsdk_set_prepare_callback(p_desc->dst_msgid, p_item);
	}	
}

void delay_us(long micro_seconds)
{
    struct timeval tv;
    
    tv.tv_sec = micro_seconds / 1000000L;
    tv.tv_usec = micro_seconds % 1000000L;
 
    select(0, NULL, NULL, NULL, &tv);
}

#define CREATE_RETRY_COUNT 5
int RTSDKCreateMsgs(struct rtonboot_create_msg_desc * p_create_desc, int total_count)
{
	int i;
	struct rtonboot_create_msg_desc * p_desc;
	int ret;
	unsigned char * ptr_msg_header;
	struct rtonboot_msg_header * p_header;
	struct rtonboot_resp_header * p_resp;
	struct rtsdk_msg_list_item * p_item;
	int count;
	int count_check;
	int count_check_limit;
	struct rtonboot_set_pending_status pending_status;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	count_check_limit = CREATE_MSG_TIMEOUT_MS * 100;
	
	ptr_msg_header = p_map + 4;
	p_header = (struct rtonboot_msg_header *)ptr_msg_header;
	
	for(i = 0; i < total_count; ++i)
	{
		p_desc = &(p_create_desc[i]);
		
		p_desc->pid = getpid();
		
		count = 0;
		
		create_retry:
		
		is_creating_msg = 1;
				
		RTSDKRtonbootTraceDetail("call RTBRIDGE_IOCTL_CREATE_MSG dst_msgid %d", p_desc->dst_msgid);
		
		ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_CREATE_MSG, p_desc);
		if(ret < 0)
		{
			printf("Winfred Young bridge create msg ioctl fail\n");
			
			RTSDKRtonbootTraceError("call create msg ioctl fail ret %d", ret);
		 
			return ERROR_NUM_CREATE_MSG_IOCTL_FAIL;
		}
		
		count_check = 0; 
		while(is_creating_msg)
		{
			usleep(10);
			
			++count_check;
			if(count_check > count_check_limit)
				goto create_timeout;
		};	
		
		goto check_resp;
		
		create_timeout:
		
			printf("create msg timeout\n");
				
			RTSDKRtonbootTraceError("create msg timeout");
				
			++count;
			if(count <= CREATE_RETRY_COUNT)
			{
				pending_status.status = 0;
	
				ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_SET_PENDING_STATUS, &pending_status);
				if(ret < 0)
				{
					printf("Winfred Young bridge SET_PENDING_STATUS fail\n");
			
					RTSDKRtonbootTraceError("in %s RTBRIDGE_IOCTL_SET_PENDING_STATUS fail ret %d", 
							__func__, ret);
							
					return ERROR_NUM_CREATE_MSG_TIMEOUT_FAIL;
				}
				else
				{
					#ifdef RTONBOOT_DEBUG
			
						printf("Winfred Young set pending status success\n");
				
					#endif
			
					RTSDKRtonbootTraceDetail("in %s set pending status success", 
							__func__);
				}
					
				goto create_retry;
			}	
				
			return ERROR_NUM_CREATE_MSG_TIMEOUT_FAIL;
			
		check_resp:
		
			p_resp = (struct rtonboot_resp_header *)(p_map + (p_header->offset_content) );			
			if(!(p_resp->resp_status))
			{
				p_item = (struct rtsdk_msg_list_item *)(malloc(sizeof(struct rtsdk_msg_list_item)));
				if(!p_item)
				{
					printf("Winfred Young bridge create msg malloc item fail\n");
				
					RTSDKRtonbootTraceError("call create msg malloc item fail");
				
					return ERROR_NUM_CREATE_MSG_MALLOC_ITEM_FAIL;
				}
			
				rtsdk_init_item_and_add_to_list(p_item, p_header, p_resp, p_desc);
			
				rtsdk_set_callbacks(p_item, p_desc);
				
				goto create_success;
			}
			else
			{
				printf("Winfred Young bridge create msg rep status fail resp_status %d\n", p_resp->resp_status);
			
				RTSDKRtonbootTraceError("create msg rep status fail resp_status %d", p_resp->resp_status);
				
				++count;
				if(count <= CREATE_RETRY_COUNT)
				{					
					goto create_retry;
				}
			
				return ERROR_NUM_CREATE_MSG_REP_STATUS_FAIL;
			}
		
		create_success:
		
			usleep(100);
	}
	
	return ERROR_NUM_OK;
}

void rtsdk_internal_delete_single_msg(struct rtsdk_msg_list_item * p_item)
{
	struct rtonboot_destroy_msg_desc destroy_msg_desc;
	int ret;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	destroy_msg_desc.pid = getpid();
    destroy_msg_desc.dst_msgid = p_item->msgid;
    destroy_msg_desc.offset_cur_list = p_item->offset_cur_list;
		 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_DESTROY_MSG, &destroy_msg_desc);
	if(ret < 0)
	{
		printf("Winfred Young delte msg ioctl fail\n");
		
		RTSDKRtonbootTraceError("delete_single_msg ioctl fail");
	}
	
	pthread_mutex_lock(&g_list_mutex);
	
	list_del(&(p_item->msg_list));
	
	pthread_mutex_unlock(&g_list_mutex);
	
	free(p_item);
}	

int InternalRTSDKSendMsg(uint32_t msgid, uint8_t flush_state,
	uint32_t flush_start, uint32_t flush_size)
{
	struct rtsdk_msg_list_item * p_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct rtonboot_send_msg_desc send_msg_desc;
	uint8_t * p_content;
	int ret;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	p_head = &(rtsdk_msg_list_head);
		
	if( list_empty(p_head) )
	{
		printf("Winfred Young list empty send fail\n");
		
		RTSDKRtonbootTraceError("in %s list empty send fail", __func__);
		
		return ERROR_NUM_LIST_EMPTY_SEND_FAIL;
	}		
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_item = (struct rtsdk_msg_list_item *)pTemp;
		
		if( (p_item->msgid) == msgid)
			goto find_on_send;
	}
	
	printf("Winfred Young can not find on list send fail\n");
	
	RTSDKRtonbootTraceError("in %s can not find on list send fail", __func__);
	
	return ERROR_NUM_NOT_FOUND_SEND_FAIL;
	
	find_on_send:
	
		send_msg_desc.pid = getpid();
		send_msg_desc.dst_msgid = msgid;
		send_msg_desc.offset_cur_list = p_item->offset_cur_list;
		
		send_msg_desc.flush_state = flush_state;
		send_msg_desc.flush_start = flush_start;
		send_msg_desc.flush_size = flush_size;
		
		if( (p_item->is_kernel == 3) ||
			(p_item->is_kernel == 7) )
		{
			send_msg_desc.msglen = (uint32_t)((int32_t)-1);
			send_msg_desc.p_content = NULL;
		}
		else if((p_item->rtsdk_prepare_send_callack))
		{	
			pthread_rwlock_rdlock(&g_prepare_rwlock);
					
			(p_item->rtsdk_prepare_send_callack)(msgid, &(send_msg_desc.msglen),
				&(send_msg_desc.p_content), p_item->max_content_size,
				p_item->rtsdk_prepare_send_callack_priv);
				
			pthread_rwlock_unlock(&g_prepare_rwlock);
		}
		else
		{
			send_msg_desc.msglen = 0;
			send_msg_desc.p_content = NULL;
		}	

		ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_SEND_MSG, &send_msg_desc);
		if(ret < 0)
		{
			printf("Winfred Young send msg ioctl fail\n");
			
			RTSDKRtonbootTraceError("RTBRIDGE_IOCTL_SEND_MSG ioctl fail ret %d", ret);
			
			return ERROR_NUM_IOCTL_SEND_FAIL;
		}
		
		return ERROR_NUM_OK;
}

int RTSDKSendMsg(uint32_t msgid)
{
	return InternalRTSDKSendMsg(msgid, 0, 0, 0);
}

int RTSDKSendMsgAndFlush(uint32_t msgid, uint8_t flush_state,
	uint32_t flush_start, uint32_t flush_size)
{
	return InternalRTSDKSendMsg(msgid, flush_state,
		flush_start, flush_size);
}

void RTSDKDeleteSingleMsg(uint32_t dst_msgid)
{
	struct rtsdk_msg_list_item * p_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	
	RTSDKRtonbootTraceDetail("Enter %s dst_msgid %d", __func__, dst_msgid);
	
	p_head = &(rtsdk_msg_list_head);
		
	if( list_empty(p_head) )
	{
		printf("Winfred Young list empty delete fail\n");

		RTSDKRtonbootTraceError("in %s list empty delete fail", __func__);
		
		return;
	}		
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_item = (struct rtsdk_msg_list_item *)pTemp;
		
		if( (p_item->msgid) == dst_msgid)
			goto find_on_list;
	}
	
	printf("Winfred Young can not find on list delete fail\n");\
	
	RTSDKRtonbootTraceError("in %s can not find on list delete fail", __func__);
	
	return;
	
	find_on_list:
	
		rtsdk_internal_delete_single_msg(p_item);
}

void RTSDKDeleteAllMsg(void)
{
	struct rtsdk_msg_list_item * p_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	p_head = &(rtsdk_msg_list_head);
		
	if( list_empty(p_head) )
	{	
		return;
	}		
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_item = (struct rtsdk_msg_list_item *)pTemp;
		
		rtsdk_internal_delete_single_msg(p_item);
		
		usleep(100);
	}
}

void RTSDKUnInit(void)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	g_continue_readmsg = 0;
	
	sleep(1);
	
	if(fd_bridge > 0)
	{
		if( (p_map) && (p_map != MAP_FAILED) )
		{
			munmap(p_map, bufpara.total_size);
			
			p_map = NULL;
		}	
     
		close(fd_bridge);
		
		fd_bridge = -1;
	}
	
	if( (use_bulk) && (fd_bulk > 0) )
	{
		if( (p_bulk) && (p_bulk != MAP_FAILED) )
		{
			munmap(p_bulk, bulk_para.total_size);
			
			p_bulk = NULL;
		}	
     
		close(fd_bulk);
		
		fd_bulk = -1;
		
		use_bulk = 0;
	}
	
	pthread_rwlock_destroy(&g_prepare_rwlock);
	 
	pthread_rwlock_destroy(&g_resp_rwlock);
	 
	pthread_rwlock_destroy(&g_end_rwlock);
}	

void RTSDKSetUserSignalFunction(user_sig_callack_func_t sig_callback)
{
	user_sig_callack = sig_callback;
}

int RTSDKUseBulk(void)	
{
	int ret;
	unsigned long long * ptemp;
	if(use_bulk)
	{
		return ERROR_NUM_OK;
	}		
		
	fd_bulk = open("/dev/rtbulk", O_RDWR);
    if(fd_bulk < 0)
    {
         printf("Winfred Young open bulk fail\n");
         
         return ERROR_NUM_OPEN_BULK_FAIL;
     }
     
     ret = ioctl(fd_bulk, RTBULK_IOCTL_GET_RTBULK_BUFPARA, &bulk_para);
     if(ret < 0)
     {
		 printf("Winfred Young bulk ioctl fail\n");
		 
		 close(fd_bulk);
		 
		 fd_bulk = -1;
     
		 return ERROR_NUM_GET_BULK_PARA_FAIL;
     }
     
     #ifdef RTONBOOT_DEBUG
     
		printf("Winfred Young get bulkpara total_size %lx \n", bulk_para.total_size);
		
	 #endif	
 
     p_bulk = (unsigned char *)mmap(0, bulk_para.total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bulk, 0);
     if(p_bulk == MAP_FAILED)
     {
         printf("Winfred Young bulk mmap fail\n");
         
         close(fd_bulk);
		 
		 fd_bulk = -1;
		 
		 return ERROR_NUM_BULK_MMAP_FAIL;
     }
     
     ptemp = (unsigned long long *)(p_bulk);
     
     (*(ptemp)) = 0x8888;
     
     use_bulk = 1;
     
     return ERROR_NUM_OK;
}

struct rtonboot_create_msg_desc bulk_msgs[] = {	
	{
		.max_content_size = 32,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
	
	{
		.max_content_size = 32,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},
};

int RTSDKCreateBulkMsgs(uint32_t linux_ready_bulk_msgid, uint32_t rtos_ready_bulk_msgid)
{
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	if(!use_bulk)
	{
		printf("Winfred Young bulk not inited\n");
		
		RTSDKRtonbootTraceError("in %s bulk mmap fail", __func__);
		
		return ERROR_NUM_BULK_NOT_INITED_FAIL;
	}	
	
	bulk_msgs[0].pid = getpid();
	bulk_msgs[0].dst_msgid = linux_ready_bulk_msgid;
	
	bulk_msgs[1].pid = getpid();
	bulk_msgs[1].dst_msgid = rtos_ready_bulk_msgid;
	
	int ret = RTSDKCreateMsgs(bulk_msgs, 2);
	if(ret)
	{
		printf("Winfred Young create bulk msgs fail\n");
		
		RTSDKRtonbootTraceError("in %s create bulk msgs fail", __func__);
		
		return ERROR_NUM_BULK_CREATE_MSGS_FAIL;
	}
	
	return ERROR_NUM_OK;
}

int RTSDKReceiveBroadcast(void)
{
	struct rtonboot_broadcast_receive_desc broadcast_receive_desc;
	int ret;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	broadcast_receive_desc.pid = getpid();
	broadcast_receive_desc.is_receive = 1;
	 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_BROADCAST_RECEIVE, &broadcast_receive_desc);
	if(ret < 0)
	{
		printf("Winfred Young bridge receive broadcast ioctl fail\n");
		
		RTSDKRtonbootTraceError("in %s receive broadcast ioctl fail ret %d", __func__, ret);
		 
		return ERROR_NUM_RECEIVE_BROADCAST_IOCTL_FAIL;
	}
	
	return ERROR_NUM_OK;
}

int RTSDKUnReceiveBroadcast(void)
{
	struct rtonboot_broadcast_receive_desc broadcast_receive_desc;
	int ret;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	
	broadcast_receive_desc.pid = getpid();
	broadcast_receive_desc.is_receive = 0;
	 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_BROADCAST_RECEIVE, &broadcast_receive_desc);
	if(ret < 0)
	{
		printf("Winfred Young bridge unreceive broadcast ioctl fail\n");
		
		RTSDKRtonbootTraceError("in %s unreceive broadcast ioctl fail ret %d", __func__, ret);
		 
		return ERROR_NUM_UN_RECEIVE_BROADCAST_IOCTL_FAIL;
	}
	
	return ERROR_NUM_OK;
}

int RTSDKCheckECNetPortIdx(void)
{
	struct rtonboot_ec_net_port_idx ec_net_port_idx;
	int ret;
	int32_t net_port_idx;
	
	RTSDKRtonbootTraceDetail("Enter %s", __func__);
	 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_GET_NET_PORT_IDX, &ec_net_port_idx);
	if(ret < 0)
	{
		printf("Winfred Young bridge get ec_net_port_idx ioctl fail\n");
		
		RTSDKRtonbootTraceError("in %s get ec_net_port_idx ioctl fail ret %d", __func__, ret);
		 
		return ERROR_NUM_GET_NET_PORT_IDX_IOCTL_FAIL;
	}
	
	net_port_idx = ec_net_port_idx.net_port_idx;
	
	if( (net_port_idx < 0) || (net_port_idx > 1) )
	{
		RTSDKRtonbootTraceError("in %s invalid ec_net_port_idx ioctl", __func__);
		
		return ERROR_NUM_INVALID_NET_PORT_IDX_FAIL;
	}	
	
	return ERROR_NUM_OK;
}

int RTSDKGetSanerfaPara(uint8_t * pSanErFA)
{
	int ret;
	struct rtonboot_sanerfa_para sanerfa_para;
	 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_GET_SANERFA_PARA, &sanerfa_para);
	if(ret < 0)
	{
		printf("Winfred Young bridge get sanerfa para ioctl fail\n");
		 
		return ERROR_NUM_SANERFA_IOCTL_FAIL;
	}
	
	memcpy(pSanErFA, &(sanerfa_para.sanerfa_para[0]), 32);
	
	return ERROR_NUM_OK;
}

int RTSDKCpyKernelBuffer(uint32_t buf_off, uint32_t buf_len,
		char * bufptr)
{
	struct rtonboot_cpybuf_para cpybuf_para;
	int ret;
	
	cpybuf_para.buf_off = buf_off;
	cpybuf_para.buf_len = buf_len;
	cpybuf_para.bufptr = bufptr;
	
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_CPY_BUF, &cpybuf_para);
	if(ret < 0)
	{
		printf("Winfred Young bridge RTBRIDGE_IOCTL_CPY_BUF fail\n");
		
		RTSDKRtonbootTraceError("in %s RTBRIDGE_IOCTL_CPY_BUF fail ret %d", __func__, ret);
		 
		return ERROR_CPYBUF_IOCTL_FAIL;
	}
	
	return ERROR_NUM_OK;
}

int RTSDKSetRtonbootTraceLevel(int level)
{
	struct rtonboot_set_trace_level trace_level;
	int ret;
	
	trace_level.level = level;
	
	if(level > RTONBOOT_TRACE_LEVEL_DETAIL)
		trace_level.level = RTONBOOT_TRACE_LEVEL_DETAIL;
		
	 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_SET_RTONBOOT_TRACE_LEVEL, &trace_level);
	if(ret < 0)
	{
		printf("Winfred Young bridge set trace level ioctl fail\n");
		
		RTSDKRtonbootTraceError("in %s set trace level ioctl fail ret %d", __func__, ret);
		 
		return ERROR_NUM_SET_TRACE_LEVEL_FAIL;
	}
	
	return ERROR_NUM_OK;
}

char TraceBuffer[129];

int RTSDKRtonbootUserTrace(int level, char * fmt, va_list args)
{
	struct rtonboot_user_trace_para trace_para;
	int ret;
	uint32_t len;
	
	if(fd_bridge < 0)
		return ERROR_NUM_OK;
	
	len = vsnprintf(&TraceBuffer[0], 129, fmt, args);
	TraceBuffer[128] = 0;
	
	trace_para.level = level;
	trace_para.fmt = &TraceBuffer[0];
	trace_para.len = len;
		 
	ret = ioctl(fd_bridge, RTBRIDGE_IOCTL_USER_RTONBOOT_TRACE, &trace_para);
	if(ret < 0)
	{
		printf("Winfred Young bridge RTBRIDGE_IOCTL_USER_RTONBOOT_TRACE ioctl fail\n");
		 
		return ERROR_NUM_USER_TRACE_IOCTL_FAIL;
	}
	
	return ERROR_NUM_OK;
}

void RTSDKRtonbootTraceDetail(char * fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	(void)RTSDKRtonbootUserTrace(RTONBOOT_TRACE_LEVEL_DETAIL, fmt, args);
	va_end(args);
}

void RTSDKRtonbootTraceInfo(char * fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	(void)RTSDKRtonbootUserTrace(RTONBOOT_TRACE_LEVEL_INFO, fmt, args);
	va_end(args);
}

void RTSDKRtonbootTraceWarn(char * fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	(void)RTSDKRtonbootUserTrace(RTONBOOT_TRACE_LEVEL_WARN, fmt, args);
	va_end(args);
}

void RTSDKRtonbootTraceError(char * fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	(void)RTSDKRtonbootUserTrace(RTONBOOT_TRACE_LEVEL_ERROR, fmt, args);
	va_end(args);
}

