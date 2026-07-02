#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/compat.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <linux/mmc/host.h>

#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/fs.h>

#include "rtbridge.h"
#include "rtpub.h"
#include "../../kernel/rtonboot/rtonboot.h"

#include "jihuoma.h"

#undef DISABLE_TRACE

#if (ENABLE_JIHUOMA > 0)

#define SNAERFA_CHECK_CNT 200

#define SYSFS_CHECK_CNT 400

#endif

extern void RTOnBootTraceEnd(void);

extern int get_rtonboot_trace_level(void);

#if (ENABLE_JIHUOMA > 0)

extern uint32_t do_sanerfa(void);

extern uint32_t read_sysfs_attribute(void);

#endif

char * ptr_bulk_start;
char * ptr_trace_start;

struct cdev *chardev;
struct class *my_class;
static dev_t dev;

#define DP_MAJOR MAJOR(dev)
#define DP_MINOR MINOR(dev)

struct rtbridge_dev * rtbridge_devp;

struct fasync_struct * async_queue;

int is_fasync_send = 0;

int rtbridge_ever_open = 0;

uint32_t offset_root_list_head = 0;

struct list_head pollpara_list_head;

struct list_head k7_msglist_head;
extern char * get_ptr_rtonboot(void);

extern void flush_linux_bulk_before_send(uint32_t offset, uint32_t len);
void kmalloc_k7_msgitem_and_add(uint32_t msgid, uint32_t offset_cur_list)
{
	struct k7_msglist_item * p_item;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct list_head * p_head;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_head = &(k7_msglist_head);
		
	if( list_empty(p_head) )
			goto malloc_and_add;
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct k7_msglist_item *)pTemp;

		if( (p_item->msgid) == msgid )
		{
			p_item->offset_cur_list = offset_cur_list;
			
			return;
		}	
	}
	
malloc_and_add:
	
	p_item = kmalloc(sizeof(struct k7_msglist_item), GFP_KERNEL);
	if (!p_item)
	{
		printk("Winfred Young kmalloc k7_msglist_item fail\n");
			
		RTOnBootTraceError("in %s kmalloc k7_msglist_item fail", __func__);
			    		
    	return;
	}
	
	p_item->msgid = msgid;
	
	p_item->offset_cur_list = offset_cur_list;
	
	list_add_tail( &(p_item->k7_msglist), &k7_msglist_head);
}	

int rtbridge_open(struct inode *inode, struct file *filp)
{
		struct rtbridge_file_priv * p_file_priv;
		struct pollpara_list_item * p_item;
		
		p_file_priv = kmalloc(sizeof(struct rtbridge_file_priv), GFP_KERNEL);
		if (!p_file_priv)
		{
			printk("Winfred Young open file %s kmalloc fail\n", RTBRIDGE_DEVICE_NAME);
			
			RTOnBootTraceError("in %s kmalloc file_priv fail", __func__);
			    		
    		return -1;
		}
		
		p_item = kmalloc(sizeof(struct pollpara_list_item), GFP_KERNEL);
		if (!p_item)
		{
			printk("Winfred Young open file %s kmalloc pollpara_list_item fail\n", RTBRIDGE_DEVICE_NAME);
			
			kfree(p_file_priv);
			
			RTOnBootTraceError("in %s kmalloc pollpara_list_item fail", __func__);
			    		
    		return -1;
		}
		
		memset(p_file_priv, 0, sizeof(struct rtbridge_file_priv)); 
    
    	p_file_priv->devp = rtbridge_devp;
    	
    	p_file_priv->p_item = p_item;
    	
    	p_item->msg_created_received = 0;
    	
    	p_item->pid = 0;
    	
    	p_item->val = 0;
    	
    	init_waitqueue_head( &(p_item->msg_created_wait) ); 
    	
    	list_add_tail( &(p_item->pollpara_list), &pollpara_list_head);
    
    	filp->private_data = p_file_priv;
    	
    	rtbridge_ever_open = 1;

		return 0; 
}

int rtbridge_release(struct inode *inode, struct file *filp)
{
	struct rtbridge_file_priv * p_file_priv;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(filp->private_data);
	
	list_del(&(p_file_priv->p_item->pollpara_list));
	
	kfree( (p_file_priv->p_item) );
	
	kfree(p_file_priv);
	
	return 0;
}

int is_dst_msg_valid(struct rtonboot_create_msg_desc * p_cteate_desc)
{
	if( (p_cteate_desc->dst_msgid) < MSGID_USER_BEGIN)
	{
		printk("Winfred Young dst_msgid must not predfine\n");
		
		RTOnBootTraceError("in %s p_cteate_desc->dst_msgid < MSGID_USER_BEGIN", __func__);
		
		return 0;
	}
	
	if( (p_cteate_desc->is_broadcast) && (p_cteate_desc->is_kernel) 
		&& ((p_cteate_desc->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		printk("Winfred Young broadcast msg must not kernel\n");
		
		RTOnBootTraceError("in %s p_cteate_desc both is_broadcast and is_kernel", __func__);
		
		return 0;
	}	
		
	if( (p_cteate_desc->is_broadcast) && (!(p_cteate_desc->msg_origin)) )
	{
		printk("Winfred Young broadcast msg must origin from RTOS\n");
		
		RTOnBootTraceError("in %s p_cteate_desc both is_broadcast and msg origin form RTOS", __func__);
		
		return 0;
	}
	
	if( (p_cteate_desc->is_broadcast) && ((p_cteate_desc->msg_has_response) != MSG_NO_RESPONSE) )
	{
		printk("Winfred Young broadcast msg must not has response\n");
		
		RTOnBootTraceError("in %s p_cteate_desc both is_broadcast and msg has response", __func__);
		
		return 0;
	}
	
	/*if( (!(p_cteate_desc->is_kernel)) && (!(p_cteate_desc->msg_origin)) &&  (p_cteate_desc->msg_has_response) )
	{
		printk("Winfred Young origin from linux msg must not nonresponse and nonkernel\n");
		
		return 0;
	}*/
	
		
	return 1;
}

void prepare_create_msg_list_item(struct send_list_item * p_item, uint8_t * p_create_content, 
	struct file *file, struct rtonboot_create_msg_desc * pdesc)
{
	struct rtbridge_file_priv * p_file_priv;
	struct rtbridge_dev * devp;
	struct rtonboot_msg_header * p_header;
	int32_t ret;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
	
	devp = p_file_priv->devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_create_content;
	
	p_item->is_content_alloc_inlist = 1;
	
	p_item->msg_header.msgid = MSGID_CREATE_MSG;
	
	p_item->msg_header.max_content_size = sizeof(struct rtonboot_msg_header);
	
	p_item->msg_header.msg_len = sizeof(struct rtonboot_msg_header);
	
	p_item->msg_header.msg_origin = MSG_ORIGIN_LINUX;
	
	p_item->msg_header.msg_has_response = MSG_HAS_RESPONSE;
	
	p_item->msg_header.is_broadcast = 0;
	
	p_item->msg_header.is_kernel = 0;
	
	p_item->msg_header.pid = pdesc->pid;
	
	p_item->msg_header.offset_content = OFFSET_TO_PREDEFINE_MSG_CONTENT;
	
	p_item->msg_header.async_or_callback = (uint64_t)(p_file_priv->async_queue);
	
	p_header = (struct rtonboot_msg_header *)(p_item->p_content);
	
	p_header->msgid = pdesc->dst_msgid;
	
	p_header->max_content_size = pdesc->max_content_size;
	
	p_header->msg_len = 0;
	
	p_header->msg_origin = pdesc->msg_origin;
	
	p_header->msg_has_response = pdesc->msg_has_response;
	
	p_header->is_broadcast = pdesc->is_broadcast;
	
	p_header->is_kernel = pdesc->is_kernel;
	
	if( (p_header->is_kernel) && ((p_header->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		p_header->pid = 0;
	}
	else	
	{
		p_header->pid = pdesc->pid;
	}	
	
	if( (!(p_header->is_kernel)) ||
		((p_header->is_kernel) == IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		p_header->async_or_callback = (uint64_t)(p_file_priv->async_queue);
	}
	else
	{
		if( ((p_header->is_kernel) == 3) 
			|| ((p_header->is_kernel) == 7) )
		{
			ret = get_prepare_kernel_max_content_size(p_header->msgid);
			if(ret >= 0)
			{
				p_header->max_content_size = (uint32_t)(ret);
			}	
		}
		
		set_kernel_callback_according_msgid( p_header->msgid, &(p_header->async_or_callback), 
			&(p_header->callback_priv));
	}
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}

void prepare_destroy_msg_list_item(struct send_list_item * p_item, uint8_t * p_destroy_content, 
	struct file *file, struct rtonboot_destroy_msg_desc * p_destroy_desc)
{
	struct rtbridge_file_priv * p_file_priv;
	struct rtbridge_dev * devp;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
	
	devp = p_file_priv->devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_destroy_content;
	
	p_item->is_content_alloc_inlist = 0;
	
	p_item->msg_header.msgid = MSGID_DESTORY_MSG;
	
	p_item->msg_header.max_content_size = sizeof(struct rtonboot_msg_header);
	
	p_item->msg_header.msg_len = sizeof(struct rtonboot_msg_header);
	
	p_item->msg_header.msg_origin = MSG_ORIGIN_LINUX;
	
	p_item->msg_header.msg_has_response = MSG_NO_RESPONSE;
	
	p_item->msg_header.is_broadcast = 0;
	
	p_item->msg_header.is_kernel = 0;
	
	p_item->msg_header.pid = p_destroy_desc->pid;
	
	p_item->msg_header.offset_content = OFFSET_TO_PREDEFINE_MSG_CONTENT;
	
	p_item->msg_header.async_or_callback = (uint64_t)(p_file_priv->async_queue);
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}

void prepare_send_msg_list_item(struct send_list_item * p_item, uint8_t * p_send_content, 
	struct file *file, struct rtonboot_send_msg_desc * p_send_desc, struct rtonboot_msg_header * p_dst_header)
{
	struct rtbridge_file_priv * p_file_priv;
	struct rtbridge_dev * devp;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
	
	devp = p_file_priv->devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_send_content;
	
	p_item->is_content_alloc_inlist = 1;
	
	memcpy(&(p_item->msg_header), p_dst_header, sizeof(struct rtonboot_msg_header) );
	
	p_item->msg_header.msg_len = p_send_desc->msglen;
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}

void k7_prepare_send_msg_list_item(struct send_list_item * p_item, uint8_t * p_send_content, 
	struct rtonboot_send_msg_desc * p_send_desc, struct rtonboot_msg_header * p_dst_header)
{
	struct rtbridge_dev * devp;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	devp = rtbridge_devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_send_content;
	
	p_item->is_content_alloc_inlist = 1;
	
	memcpy(&(p_item->msg_header), p_dst_header, sizeof(struct rtonboot_msg_header) );
	
	p_item->msg_header.msg_len = p_send_desc->msglen;
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}


void prepare_rr_resp_msg_list_item(struct send_list_item * p_item, uint8_t * p_rr_resp_content, 
	struct file *file, struct rtonboot_rr_resp_desc * p_resp_desc, struct rtonboot_msg_header * p_dst_header)
{
	struct rtbridge_file_priv * p_file_priv;
	struct rtbridge_dev * devp;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
	
	devp = p_file_priv->devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_rr_resp_content;
	
	p_item->is_content_alloc_inlist = 0;
	
	memcpy(&(p_item->msg_header), p_dst_header, sizeof(struct rtonboot_msg_header) );
	
	p_item->msg_header.msg_len = p_resp_desc->msglen;
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}

void prepare_broadcast_receive_msg_list_item(struct send_list_item * p_item, uint8_t * p_broadcast_content, 
	struct file *file, struct rtonboot_broadcast_receive_desc * pdesc)
{
	struct rtbridge_file_priv * p_file_priv;
	struct rtbridge_dev * devp;
	struct rtonboot_broadcast_receive_header * p_header;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
	
	devp = p_file_priv->devp;
	
	memset(p_item, 0, sizeof(struct send_list_item));
	
	p_item->p_content = p_broadcast_content;
	
	p_item->is_content_alloc_inlist = 1;
	
	p_item->msg_header.msgid = MSGID_RECEIVE_BROADCAST_MSG;
	
	p_item->msg_header.max_content_size = sizeof(struct rtonboot_broadcast_receive_header);
	
	p_item->msg_header.msg_len = sizeof(struct rtonboot_broadcast_receive_header);
	
	p_item->msg_header.msg_origin = MSG_ORIGIN_LINUX;
	
	p_item->msg_header.msg_has_response = MSG_NO_RESPONSE;
	
	p_item->msg_header.is_broadcast = 0;
	
	p_item->msg_header.is_kernel = 0;
	
	p_item->msg_header.pid = pdesc->pid;
	
	p_item->msg_header.offset_content = OFFSET_TO_PREDEFINE_MSG_CONTENT;
	
	p_item->msg_header.async_or_callback = (uint64_t)(p_file_priv->async_queue);
	
	p_header = (struct rtonboot_broadcast_receive_header *)(p_item->p_content);
	
	p_header->is_receive = pdesc->is_receive;
	
	mutex_lock(&(devp->send_lock));
	
	list_add_tail( &(p_item->send_list), &(devp->msg_send_list_head) );
	
	mutex_unlock(&(devp->send_lock));
	
	wake_up(&(devp->send_queue));
}

void internal_set_pending_status(uint32_t * p_pending, uint32_t status)
{
    RTOnBootTraceDetail("Enter %s status is %d", __func__, status);
    
	spin_lock(&(rtbridge_devp->send_slock));
	
	atomic_store_release_32(p_pending, status);
		
	spin_unlock(&(rtbridge_devp->send_slock));	
}

extern struct timer_list g_async_timer;

extern int is_async_timer_start;

extern uint32_t count_cur_broadcast;

extern uint32_t count_broadcast_received;

void set_pending_status(uint32_t status)
{
	char * ptr_rtonboot;
    uint32_t * ptemp;
    char * ptr_msg_header;
    struct rtonboot_msg_header * p_msg_header;
    int is_wakeup = 0;
    
    RTOnBootTraceDetail("Enter %s status is %d", __func__, status);
    
    ptr_rtonboot = get_ptr_rtonboot(); 
     
    ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
    ptemp = (uint32_t *)(ptr_rtonboot);
    
    ptr_msg_header = ptr_rtonboot + 4;
    
    p_msg_header = (struct rtonboot_msg_header *)ptr_msg_header;
    
    if(status)
    {
		internal_set_pending_status(ptemp, status);
		
		return;
	}
	else
	{	
		if(count_cur_broadcast)
		{
			++count_broadcast_received;
			
			if(count_broadcast_received >= count_cur_broadcast)
			{	
				count_broadcast_received = 0;
				
				count_cur_broadcast = 0;
				
				internal_set_pending_status(ptemp, 0);
			
				is_wakeup = 1; 
			
				goto just_stop_timer;
			}	
		}
		else
		{	
			if( ((p_msg_header->msg_origin) == MSG_ORIGIN_RTOS) && ((p_msg_header->msg_has_response) == MSG_HAS_RESPONSE) )
			{
				is_wakeup = 0;
			
				goto just_stop_timer;
			}	
			else
			{		
				internal_set_pending_status(ptemp, 0);
			
				is_wakeup = 1; 
			
				goto just_stop_timer;
			}
		}		
	}	
	
	just_stop_timer:
	
		if(is_async_timer_start)
		{
			del_timer(&g_async_timer);	
			
			is_async_timer_start = 0;
		}	
	
		if(is_wakeup)
		{
			wake_up(&(rtbridge_devp->send_queue));
		}	
}	

void bc_set_pending_status(void)
{
	char * ptr_rtonboot;
    uint32_t * ptemp;
    
    RTOnBootTraceDetail("Enter %s", __func__);
    
    ptr_rtonboot = get_ptr_rtonboot(); 
     
    ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
    ptemp = (uint32_t *)(ptr_rtonboot);
    
    internal_set_pending_status(ptemp, 0);
    
    if(is_async_timer_start)
	{
		del_timer(&g_async_timer);	
			
		is_async_timer_start = 0;
	}	
	
	wake_up(&(rtbridge_devp->send_queue));
}	
struct rtonboot_msg_header * get_content_ptr_from_offset_and_check(uint32_t offset_cur, uint32_t dst_msgid)
{
	char * ptr_rtonboot;
	struct msg_list_item * p_item;
	struct rtonboot_msg_header * p_dst_header;
	
	RTOnBootTraceDetail("Enter %s offset_cur is %d dst_msgid is %d", __func__, offset_cur, dst_msgid);
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
				
	ptr_rtonboot += offset_cur;
	
	p_item = (struct msg_list_item *)(ptr_rtonboot);
	
	p_dst_header = &(p_item->msg_header);
	
	if( (p_dst_header->msgid) !=  dst_msgid )
	{
		printk("Winfred Young RTBRIDGE: dst_msgid is invalid \n");
		
		RTOnBootTraceError("in %s p_dst_header->msgid %d  !=  dst_msgid %d", __func__, p_dst_header->msgid, dst_msgid);
		
		return NULL;
	}
	
	return p_dst_header;
}

struct rtonboot_msg_header * get_content_ptr_from_offset_and_check_send(uint32_t offset_cur, struct rtonboot_send_msg_desc * p_send_desc)
{
	char * ptr_rtonboot;
	struct msg_list_item * p_item;
	struct rtonboot_msg_header * p_dst_header;
	
	RTOnBootTraceDetail("Enter %s offset_cur is %d", __func__, offset_cur);
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
				
	ptr_rtonboot += offset_cur;
	
	p_item = (struct msg_list_item *)(ptr_rtonboot);
	
	p_dst_header = &(p_item->msg_header);
	
	if( (p_dst_header->msgid) !=  (p_send_desc->dst_msgid) )
	{
		printk("Winfred Young RTBRIDGE: dst_msgid is invalid \n");
		
		RTOnBootTraceError("in %s p_dst_header->msgid %d != p_send_desc->dst_msgid %d", __func__, 
			p_dst_header->msgid, p_send_desc->dst_msgid);
		
		return NULL;
	}
	
	if( (p_dst_header->msg_origin) !=  MSG_ORIGIN_LINUX )
	{
		printk("Winfred Young RTBRIDGE: origin must be linux \n");
		
		RTOnBootTraceError("in %s p_dst_header->msg_origin !=  MSG_ORIGIN_LINUX", __func__);
		
		return NULL;
	}
	
	if( (p_dst_header->max_content_size) <  (p_send_desc->msglen) )
	{
		printk("Winfred Young RTBRIDGE: msglen is exceed max_content_size \n");
		
		RTOnBootTraceError("in %s p_dst_header->max_content_size %d < p_send_desc->msglen", __func__,
			p_dst_header->max_content_size, p_send_desc->msglen);
		
		return NULL;
	}
	
	if( (!(p_dst_header->is_broadcast)) && 
		( (!(p_dst_header->is_kernel)) || ((p_dst_header->is_kernel) == IS_KERNEL_SCRIPTING_NOTIFY) ) )
	{
		if( (p_dst_header->pid) != (p_send_desc->pid) )
		{
			printk("Winfred Young RTBRIDGE: pid is invalid \n");
			
			RTOnBootTraceError("in %s p_dst_header->pid %d != p_send_desc->pid %d", __func__,
				p_dst_header->pid, p_send_desc->pid);
			
			return NULL;
		}	
	}
	
	return p_dst_header;
}

struct rtonboot_msg_header * get_content_ptr_from_offset_and_check_resp(uint32_t offset_cur, struct rtonboot_rr_resp_desc * p_resp_desc)
{
	char * ptr_rtonboot;
	struct msg_list_item * p_item;
	struct rtonboot_msg_header * p_dst_header;
	
	RTOnBootTraceDetail("Enter %s offset_cur is %d", __func__, offset_cur);
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
				
	ptr_rtonboot += offset_cur;
	
	p_item = (struct msg_list_item *)(ptr_rtonboot);
	
	p_dst_header = &(p_item->msg_header);
	
	if( (p_dst_header->msgid) !=  (p_resp_desc->dst_msgid) )
	{
		printk("Winfred Young RTBRIDGE: dst_msgid is invalid \n");
		
		RTOnBootTraceError("in %s p_dst_header->msgid %d != p_resp_desc->dst_msgid %d", __func__, 
			p_dst_header->msgid, p_resp_desc->dst_msgid);
		
		return NULL;
	}
	
	if( (p_dst_header->msg_origin) !=  MSG_ORIGIN_RTOS )
	{
		printk("Winfred Young RTBRIDGE: origin must be RTOS \n");
		
		RTOnBootTraceError("in %s p_dst_header->msg_origin must be MSG_ORIGIN_RTOS ", __func__);
		
		return NULL;
	}
	
	if( (p_dst_header->msg_has_response) !=  MSG_HAS_RESPONSE )
	{
		printk("Winfred Young RTBRIDGE: must have response \n");
		
		RTOnBootTraceError("in %s p_dst_header->msg_has_response must be MSG_HAS_RESPONSE ", __func__);
		
		return NULL;
	}
	
	if( (p_dst_header->max_content_size) <  (p_resp_desc->msglen) )
	{
		printk("Winfred Young RTBRIDGE: msglen is exceed max_content_size \n");
		
		RTOnBootTraceError("in %s p_dst_header->max_content_size %d <  p_resp_desc->msglen %d", __func__,
			p_dst_header->max_content_size, p_resp_desc->msglen);
		
		return NULL;
	}
	
	return p_dst_header;
}

uint8_t * get_rr_resp_content_ptr(struct rtonboot_rr_resp_desc * p_resp_desc)
{
	uint32_t offset_content;
	char * ptr_rtonboot;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	offset_content = p_resp_desc->offset_content;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
				
	ptr_rtonboot += offset_content;
	
	return (uint8_t *)(ptr_rtonboot);
}	

void set_rtonboot_trace_level(uint32_t trace_level)
{
	char * ptr_rtonboot;
	unsigned long start;
    unsigned long stop;
    int level;
    
    RTOnBootTraceDetail("Enter %s trace_level is %d", __func__, trace_level);
    
    level = get_rtonboot_trace_level();
    
    if( (level) && (!trace_level) )
    {
		RTOnBootTraceEnd();
	}	
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_TRACE_LOCK;
	
	*((uint64_t *)ptr_rtonboot) = 0;
	
	ptr_rtonboot += (OFFSET_TO_RTONBOOT_TRACE_LEVEL - OFFSET_TO_RTONBOOT_TRACE_LOCK);
	
	*((uint64_t *)ptr_rtonboot) = trace_level;
	
	start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_TRACE_LOCK;
    
	stop = start + 64;
				
	rtonboot_flush_dcache_range(start, stop);  
}	

extern int g_netport_idx;

extern void RTOnBootTraceUser(int cur_level, char * fmt, uint32_t len2);

#define OFFSET_TO_SANERFA_PARA 0x1e0

void send_k7_msg(uint32_t msgid, uint32_t msglen)
{
	struct k7_msglist_item * p_item;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct list_head * p_head;
	uint8_t * p_send_content = NULL;
	struct send_list_item * p_send_item;
	struct rtonboot_send_msg_desc send_desc;
	struct rtonboot_msg_header * p_dst_header;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_head = &(k7_msglist_head);
		
	if( list_empty(p_head) )
			return;
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct k7_msglist_item *)pTemp;

		if( (p_item->msgid) == msgid )
		{
			goto send_k7;
		}	
	}
	
	return;
	
	send_k7:
	
		p_send_content = prepare_sendmsg_content_in_kernel(msgid, &msglen);
		
		if(!p_send_content)
		{
			RTOnBootTraceError("send_k7_msg prepare_sendmsg_content_in_kernel fail");
						
			return;
		}
		
		p_send_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
		if(!(p_send_item))
		{
			printk("Winfred Young RTBRIDGE: cannot malloc list item \n");
					
			RTOnBootTraceError("send_k7_msg kmalloc send_list_item fail");
					
			if(p_send_content)
			{
				kfree(p_send_content);
			}	
			
			return;
		}
		
		memset(&send_desc, 0, sizeof(struct rtonboot_send_msg_desc) );
		
		send_desc.dst_msgid = msgid;
		send_desc.offset_cur_list = p_item->offset_cur_list;
		send_desc.msglen = msglen;
	
		p_dst_header = get_content_ptr_from_offset_and_check_send(send_desc.offset_cur_list, &send_desc);
		if(!p_dst_header)
		{
			RTOnBootTraceError("send_k7_msg get_content_ptr_from_offset_and_check_send fail");
					
			return;
		}
		
		k7_prepare_send_msg_list_item(p_send_item, p_send_content, &send_desc, p_dst_header);
}	

static long rtbridge_do_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		void __user *argp = (void __user *)arg;
		struct rtonboot_bufpara bufpara;
		struct rtonboot_create_msg_desc create_desc;
		struct rtonboot_set_pending_status pending_status;
		struct rtonboot_destroy_msg_desc destroy_desc;
		struct rtonboot_send_msg_desc send_desc;
		struct rtonboot_rr_resp_desc rr_resp_header;
		struct rtonboot_broadcast_receive_desc broadcast_receive_desc;
		struct rtonboot_set_trace_level trace_level;
		struct rtonboot_user_trace_para trace_para;
		
		#if (ENABLE_JIHUOMA > 0)
		
			struct rtonboot_sanerfa_para sanerfa_para;
			
		#endif
		
		struct rtonboot_cpybuf_para cpybuf_para;
		struct send_list_item * p_item;
		uint8_t * p_create_content;
		uint8_t * p_destroy_content;
		uint8_t * p_send_content = NULL;
		uint8_t * p_rr_resp_content;
		uint8_t * p_broadcast_content;
		struct rtonboot_msg_header * p_dst_header;
		uint8_t * p_trace_content = NULL;
		int32_t temp_msglen;
		struct rtonboot_bulkpara * p_bulkpara;
		struct rtonboot_ec_net_port_idx ec_net_port_idx;
		struct rtbridge_file_priv * p_file_priv;
		char * ptr_rtonboot;
		
        switch (cmd) 
		{
			case RTBRIDGE_IOCTL_GET_RTONBOOT_BUFPARA:
			
				bufpara.total_size = ((uint32_t)CONFIG_RTONBOOT_RUNBUF_SIZE) - ((uint32_t)OFFSET_TO_RTONBOOT_SHARE_BUFFER);
				
				bufpara.total_size -= RTONBOOT_BULK_BUFFER_SIZE;
				
				/*bufpara.bulk_offset = OFFSET_TO_RTONBOOT_BULK_BUFFER;*/
				
				bufpara.bulk_offset = 0;

				if (copy_to_user(argp, (void *)(&bufpara), sizeof(struct rtonboot_bufpara)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_GET_RTONBOOT_BUFPARA copy_to_user fail");
					
					return -EFAULT;
				}	
	
				break;
				
			case RTBRIDGE_IOCTL_GET_NET_PORT_IDX:
			
				ec_net_port_idx.net_port_idx = (int32_t)(g_netport_idx);
				
				printk("Winfred Young ec_net_port_idx is %d\n", (ec_net_port_idx.net_port_idx) );
				
				RTOnBootTraceDetail("ec_net_port_idx is %d", ec_net_port_idx.net_port_idx);
				
				if (copy_to_user(argp, (void *)(&ec_net_port_idx), sizeof(struct rtonboot_ec_net_port_idx)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_GET_NET_PORT_IDX copy_to_user fail");
					
					return -EFAULT;
				}	
		
				break;
				
			case RTBRIDGE_IOCTL_CREATE_MSG:
			
				if (copy_from_user((void *)(&create_desc), argp, sizeof (struct rtonboot_create_msg_desc)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_CREATE_MSG copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
	      			
	      		if(!(is_dst_msg_valid(&create_desc)))
	      		{
					RTOnBootTraceError("RTBRIDGE_IOCTL_CREATE_MSG !(is_dst_msg_valid)");
					
					return -EINVAL;
				}	
								
				p_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
				if(!(p_item))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc list item \n");
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_CREATE_MSG kmalloc send_list_item fail");
			
					return -ENOMEM;
				}
				
				p_create_content = (uint8_t *)(kmalloc(sizeof(struct rtonboot_msg_header), GFP_KERNEL));
				if(!(p_create_content))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc p_create_content \n");
					
					kfree(p_item);
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_CREATE_MSG kmalloc p_create_content fail");
			
					return -ENOMEM;
				}
				
				p_file_priv = (struct rtbridge_file_priv *)(file->private_data);
				
				p_file_priv->p_item->pid = create_desc.pid;
				
				prepare_create_msg_list_item(p_item, p_create_content, file, (&create_desc) );
				
				break;
				
			case RTBRIDGE_IOCTL_SET_PENDING_STATUS:
			
				if (copy_from_user((void *)(&pending_status), argp, sizeof (struct rtonboot_set_pending_status)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_SET_PENDING_STATUS copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
	      		
	      		set_pending_status(pending_status.status);
			
				break;
				
			case RTBRIDGE_IOCTL_DESTROY_MSG:
			
				if (copy_from_user((void *)(&destroy_desc), argp, sizeof (struct rtonboot_destroy_msg_desc)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_DESTROY_MSG copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
	      	
				p_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
				if(!(p_item))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc list item \n");
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_DESTROY_MSG kmalloc send_list_item fail");
			
					return -ENOMEM;
				}
				
				p_destroy_content = (uint8_t *)(get_content_ptr_from_offset_and_check(destroy_desc.offset_cur_list, 
					destroy_desc.dst_msgid));
				if(!p_destroy_content)
				{
					printk("Winfred Young RTBRIDGE: p_destroy_content get fail \n");
					
					kfree(p_item);
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_DESTROY_MSG kmalloc p_destroy_content fail");
					
					return -ENOMEM;
				}
				
				prepare_destroy_msg_list_item(p_item, p_destroy_content, file, &destroy_desc);
							
				break;
				
			case RTBRIDGE_IOCTL_SEND_MSG:
			
				if (copy_from_user((void *)(&send_desc), argp, sizeof (struct rtonboot_send_msg_desc)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
	      		
	      		temp_msglen = (int32_t)(send_desc.msglen);
	      		if(temp_msglen > 0)
	      		{
					p_send_content = (uint8_t *)(kmalloc( (send_desc.msglen) , GFP_KERNEL));;	
					if(!p_send_content)
					{
						printk("Winfred Young RTBRIDGE: cannot malloc p_send_content \n");
						
						RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG kmalloc p_send_content fail");
					
						return -ENOMEM;
					}	
	      			
					if (copy_from_user((void *)(p_send_content), (send_desc.p_content), (send_desc.msglen) ))
					{
						RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG copy_from_user for p_send_content fail");
						
						return -EFAULT;
					}	
				}
				else if(temp_msglen < 0)
				{
					if( (send_desc.flush_state) == 0xff)
					{
						g_netport_idx = send_desc.flush_start;
					}
						
					p_send_content = prepare_sendmsg_content_in_kernel(send_desc.dst_msgid, 
							&(send_desc.msglen) );
					if(!p_send_content)
					{
						RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG prepare_sendmsg_content_in_kernel fail");
						
						return -ENOMEM;
					}
				}
				
				p_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
				if(!(p_item))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc list item \n");
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG kmalloc send_list_item fail");
					
					if(p_send_content)
					{
						kfree(p_send_content);
					}	
			
					return -ENOMEM;
				}
				
				p_dst_header = get_content_ptr_from_offset_and_check_send(send_desc.offset_cur_list, &send_desc);
				if(!p_dst_header)
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_MSG get_content_ptr_from_offset_and_check_send fail");
					
					return -EINVAL;
				}
				
				if( (send_desc.dst_msgid) == MSGID_LINUX_BULK_READY_MSG)
				{
					p_bulkpara = (struct rtonboot_bulkpara *)(p_send_content);
					
					flush_linux_bulk_before_send(p_bulkpara->offset, p_bulkpara->len);
				}
				else if( (send_desc.flush_state) == 1)
				{ 
					flush_linux_bulk_before_send(send_desc.flush_start, send_desc.flush_size);
				}
				
				prepare_send_msg_list_item(p_item, p_send_content, file, &send_desc, p_dst_header);
			
				break;
				
			case RTBRIDGE_IOCTL_SEND_RRMSG_RESP:
			
				if (copy_from_user((void *)(&rr_resp_header), argp, sizeof (struct rtonboot_rr_resp_desc)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_RRMSG_RESP copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
	      		
	      		p_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
				if(!(p_item))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc list item \n");	
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_RRMSG_RESP kmalloc send_list_item fail");
			
					return -ENOMEM;
				}
				
				p_dst_header = get_content_ptr_from_offset_and_check_resp(rr_resp_header.offset_cur_list, &rr_resp_header);
				if(!p_dst_header)
				{
					kfree(p_item);
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_RRMSG_RESP get_content_ptr_from_offset_and_check_resp fail");
					
					return -EINVAL;
				}
				
				p_rr_resp_content = get_rr_resp_content_ptr(&rr_resp_header);
				
				if (copy_from_user((void *)(p_rr_resp_content), rr_resp_header.p_resp_content, rr_resp_header.msglen))
				{
					kfree(p_item);
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_SEND_RRMSG_RESP copy_from_user for p_rr_resp_content fail");
					
	      			return -EFAULT;
	      		}	
				
				prepare_rr_resp_msg_list_item(p_item, p_rr_resp_content, file, &rr_resp_header, p_dst_header);

				break;
				
			case RTBRIDGE_IOCTL_BROADCAST_RECEIVE:
			
				if (copy_from_user((void *)(&broadcast_receive_desc), argp, sizeof (struct rtonboot_broadcast_receive_desc)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_BROADCAST_RECEIVE copy_from_user fail");
					
	      			return -EFAULT;
	      		}	
				
				p_item = (struct send_list_item * )(kmalloc(sizeof(struct send_list_item), GFP_KERNEL));
				if(!(p_item))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc list item \n");
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_BROADCAST_RECEIVE kmalloc send_list_item fail");
			
					return -ENOMEM;
				}
				
				p_broadcast_content = (uint8_t *)(kmalloc(sizeof(struct rtonboot_broadcast_receive_header), GFP_KERNEL));
				if(!(p_broadcast_content))
				{
					printk("Winfred Young RTBRIDGE: cannot malloc p_broadcast_content \n");
					
					kfree(p_item);
					
					RTOnBootTraceError("RTBRIDGE_IOCTL_BROADCAST_RECEIVE kmalloc p_broadcast_content fail");
			
					return -ENOMEM;
				}
				
				prepare_broadcast_receive_msg_list_item(p_item, p_broadcast_content, file, &broadcast_receive_desc);
				
				break;
				
			case RTBRIDGE_IOCTL_SET_RTONBOOT_TRACE_LEVEL:
			
			    #ifndef DISABLE_TRACE
			
				if (copy_from_user((void *)(&trace_level), argp, sizeof (struct rtonboot_set_trace_level)))
				{
	      			return -EFAULT;
	      		}	
	      			
	      		set_rtonboot_trace_level(trace_level.level);
	      		
	      		#endif
	      		
	      		break;	
	      		
	      	case RTBRIDGE_IOCTL_USER_RTONBOOT_TRACE:
	      	
				if (copy_from_user((void *)(&trace_para), argp, sizeof (struct rtonboot_user_trace_para)))
				{	
	      			return -EFAULT;
	      		}	
	      			
	      		p_trace_content = (uint8_t *)(kmalloc( (trace_para.len + 1) , GFP_KERNEL));;	
				if(!p_trace_content)
				{
					printk("Winfred Young RTBRIDGE: cannot malloc p_trace_content \n");
					
					return -ENOMEM;
				}
				
				if (copy_from_user((void *)(p_trace_content), trace_para.fmt, trace_para.len))
				{
					kfree(p_trace_content);
					
	      			return -EFAULT;
	      		}
	      		
	      		p_trace_content[trace_para.len] = 0;
	      		
	      		RTOnBootTraceUser(trace_para.level, p_trace_content, trace_para.len);
				
				kfree(p_trace_content);
				
				break;
				
			case RTBRIDGE_IOCTL_GET_SANERFA_PARA:
			
				#if (ENABLE_JIHUOMA > 0)
				
					ptr_rtonboot = get_ptr_rtonboot(); 
				
					ptr_rtonboot += OFFSET_TO_SANERFA_PARA;
				
					memcpy(&(sanerfa_para.sanerfa_para[0]), ptr_rtonboot, 32);
			
					if (copy_to_user(argp, (void *)(&sanerfa_para), sizeof(struct rtonboot_sanerfa_para)))
					{
						return -EFAULT;
					}
					
				#endif	
				
				break;			
				
			case RTBRIDGE_IOCTL_CPY_BUF:
			
				if (copy_from_user((void *)(&cpybuf_para), argp, sizeof (struct rtonboot_cpybuf_para)))
				{
					RTOnBootTraceError("RTBRIDGE_IOCTL_CPY_BUF copy_from_user fail");
					
	      			return -EFAULT;
	      		}
	      		
	      		if(!(cpybuf_para.buf_len))
					break;
					
				ptr_rtonboot = get_ptr_rtonboot(); 
				
				ptr_rtonboot += (cpybuf_para.buf_off);
				
				if (copy_to_user( (void __user *)(cpybuf_para.bufptr), ptr_rtonboot, cpybuf_para.buf_len))
				{
					return -EFAULT;
				}	
				
				if (copy_to_user(argp, (void *)(&cpybuf_para), sizeof(struct rtonboot_cpybuf_para)))
				{
					return -EFAULT;
				}
	      		
	      		break;
				
			default:
		
				printk("Winfred Young RTBRIDGE: invalid IOCTL(0x%x)\n",cmd);
				
				RTOnBootTraceError("RTBRIDGE_IOCTL invalid IOCTL(0x%x)", cmd);
			
				return -EINVAL;
		};
	
		return 0;
}


static long rtbridge_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	
	/*lock_kernel();*/
	ret = rtbridge_do_ioctl(file, cmd, arg);
	/*unlock_kernel();*/
	
	return ret;
}

static int rtbridge_mmap(struct file *filp, struct vm_area_struct *vma)
{    
     unsigned long page;
     unsigned long start = (unsigned long)(vma->vm_start);
     unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
     /*char * ptr_rtonboot;
     uint64_t * ptemp;*/
     
     RTOnBootTraceDetail("Enter %s", __func__);
 
     page = ((unsigned long)(CONFIG_RTONBOOT_LOAD_ADDR)) + ((unsigned long)(OFFSET_TO_RTONBOOT_SHARE_BUFFER));   
     
     /*vma->vm_flags &= VM_WRITE;*/
     
     /*if(remap_pfn_range(vma,start,page >> PAGE_SHIFT, size, PAGE_SHARED))*/
     if(remap_pfn_range(vma,start,page >> PAGE_SHIFT, size, vma->vm_page_prot))
     {
		 printk("Winfred Young rtbridge_mmap remap_pfn_range fail\n");
		 
		 RTOnBootTraceError("in %s RTBRIDGE_IOCTL remap_pfn_range fail", __func__);
		 
         return -1;
     } 
     
     /*ptr_rtonboot = get_ptr_rtonboot(); 
     
     ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
     ptemp = (uint64_t *)(ptr_rtonboot);
     
     *((uint64_t *)ptemp) = 0x88888888; */
 
     return 0;
}

static int rtbridge_fasync(int fd, struct file *filp, int mode)
{
	struct rtbridge_file_priv * p_file_priv;
	
	RTOnBootTraceDetail("Enter %s mode %d", __func__, mode);
	
	p_file_priv = (struct rtbridge_file_priv *)(filp->private_data);
	
	is_fasync_send = 1;
	
    return fasync_helper(fd, filp, mode, &(p_file_priv->async_queue));
}

static unsigned int rtbridge_poll(struct file * filp, struct poll_table_struct *pts)
{
    unsigned int mask = 0;
    struct rtbridge_file_priv * p_file_priv;
    
    RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(filp->private_data);

    poll_wait(filp, &(p_file_priv->p_item->msg_created_wait), pts);
    
    if( (p_file_priv->p_item->msg_created_received) )
		mask |= POLLIN | POLLRDNORM;

    return mask;
}

static ssize_t rtbridge_read(struct file * filp, char __user * buff, size_t size, loff_t * ppos)
{
    struct rtbridge_file_priv * p_file_priv;
    int ret;
    
    RTOnBootTraceDetail("Enter %s", __func__);
	
	p_file_priv = (struct rtbridge_file_priv *)(filp->private_data);
	
    wait_event_interruptible( (p_file_priv->p_item->msg_created_wait), 
		p_file_priv->p_item->msg_created_received);
    
    p_file_priv->p_item->msg_created_received = 0;
    
    ret = copy_to_user(buff, &(p_file_priv->p_item->val), 4);
    if(ret)
		return -EFAULT;
    
    return 4;
}

extern void gic_raise_softirq(const struct cpumask *mask, unsigned int irq);

#if (ENABLE_JIHUOMA > 0)

static void sanerfa_done(uint8_t ch)
{
	uint64_t temp;
	char * ptr;
	int i;
	
	if(!ch)
		ch = 0x88;
		
	temp = ch;
	
	for(i = 0 ; i < 63; ++i)
	{
		temp <<= 8;
		temp |= ch;
	}
	
	ptr = (char *)temp;
	
	*ptr = 0;		
}

#endif

#if 0

static int ksendcmd(void *foobar)
{
	struct rtbridge_dev * devp = foobar;
	int is_empty;
	struct list_head * pTemp;
	struct send_list_item * p_item = NULL;
	char * ptr_rtonboot;
	
    uint32_t * ptemp;
    uint32_t msg_pending_status;
    unsigned long start;
    unsigned long stop;
    char * ptr_msg_header;
    char * ptr_msg_content;
    struct rtonboot_msg_header * p_header;
    uint32_t msg_pending_should_status;
    
    #if (ENABLE_JIHUOMA > 0)
    
		int count = 0;
		uint32_t value;
		uint32_t val;
		char * ptr_rtonboot_check;
		
	#endif
    int is_set_atomic;
    
    ptr_rtonboot = get_ptr_rtonboot(); 
     
    ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
    ptemp = (uint32_t *)(ptr_rtonboot);
 
	for (;;) 
	{		
			DECLARE_WAITQUEUE(wait, current);

			/*
			* Wait until there is something to do
			*/
			add_wait_queue(&(devp->send_queue), &wait);
			
			for (;;) {
				
				set_current_state(TASK_INTERRUPTIBLE);
				
				mutex_lock(&(devp->send_lock));
				
				msg_pending_status = atomic_load_acquire_32(ptemp);
				 
				is_empty = list_empty(&(devp->msg_send_list_head));
				if(!(is_empty))
				{
					pTemp = ((&(devp->msg_send_list_head))->next);	
					
					p_item = (struct send_list_item *)pTemp;
					
					if( ((p_item->msg_header.msg_origin) != MSG_ORIGIN_RTOS) || ((p_item->msg_header.msg_has_response) != MSG_HAS_RESPONSE) )
					{
						msg_pending_should_status = 0;
					}
					else
					{
						msg_pending_should_status = 1;
					}	
				}	
				 
				mutex_unlock(&(devp->send_lock));
				
				if( (!(is_empty)) && (msg_pending_status == msg_pending_should_status) )
					goto work_to_do;
				
				schedule_timeout(msecs_to_jiffies(SEND_SLEEP_MS));
		
				#if (ENABLE_JIHUOMA > 0)
				
					if(count >= 0)
					{
						++count;
						if(count >= SYSFS_CHECK_CNT)
						{
							count = -1;
						
							value = read_sysfs_attribute();
						
							if(value != val)
							{
								ptr_rtonboot_check = get_ptr_rtonboot(); 
     
								ptr_rtonboot_check += 0x1e0;
							
								sanerfa_done( (*(ptr_rtonboot_check)) );
							}	
						}		
						else if(count >= SNAERFA_CHECK_CNT)
						{
							ptr_rtonboot_check = get_ptr_rtonboot(); 
     
							ptr_rtonboot_check += 0x1dc;
						
							value = *((uint32_t *)ptr_rtonboot_check);
						
							val = do_sanerfa();
						
							if(val == value)
							{							
							}
							else
							{
								ptr_rtonboot_check = get_ptr_rtonboot(); 
     
								ptr_rtonboot_check += 0x1e0;
							
								sanerfa_done( (*(ptr_rtonboot_check)) );
							}				
						}
					}
					
				#endif	
				
				 
				if (kthread_should_stop())
					goto no_work;
			}
		
			work_to_do:
			
				set_current_state(TASK_RUNNING);
				remove_wait_queue(&(devp->send_queue), &wait);
	
				if (kthread_should_stop())
					break;
					
				spin_lock(&(devp->send_slock));
				
				if( ((p_item->msg_header.msg_origin) != MSG_ORIGIN_RTOS) || ((p_item->msg_header.msg_has_response) != MSG_HAS_RESPONSE) )
				{
					msg_pending_status = atomic_load_acquire_32(ptemp);
					
					if(msg_pending_status)
					{
						spin_unlock(&(devp->send_slock));	
						
						continue;
					}
					
					list_del(&(p_item->send_list));
									
					start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER;
					
					stop = start + 64;
					
					ptr_msg_header = ptr_rtonboot + 4;
				
					memcpy(ptr_msg_header, &(p_item->msg_header), sizeof(struct rtonboot_msg_header) );
				
					rtonboot_flush_dcache_range(start, stop);
					
					is_set_atomic = 1;
				}
				else
				{
					list_del(&(p_item->send_list));	
					
					ptr_msg_header = ptr_rtonboot + 4;
					
					p_header = (struct rtonboot_msg_header *)ptr_msg_header;
					
					p_header->msg_len = p_item->msg_header.msg_len;
					
					start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER;
    
					stop = start + 64;
				
					rtonboot_flush_dcache_range(start, stop);
					
					is_set_atomic = 0;  
				}	
				
				ptr_msg_content = ptr_rtonboot + (p_item->msg_header.offset_content);
				
				memcpy(ptr_msg_content, (p_item->p_content), p_item->msg_header.msg_len );
				
				start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + (OFFSET_TO_RTONBOOT_SHARE_BUFFER) 
					+ (p_item->msg_header.offset_content);
				
				stop = start + p_item->msg_header.msg_len;
				
				rtonboot_flush_dcache_range(start, stop);
				
				if(is_set_atomic)
				{
					atomic_store_release_32(ptemp, 1);	
			    }	
				
				spin_unlock(&(devp->send_slock));	

				#ifdef RTONBOOT_DEBUG 

					printk("Winfred Young Begin send to RTOS Core %d\n", p_item->is_content_alloc_inlist);
										
				#endif
				
				RTOnBootTraceDetail("Begin send to RTOS Core p_item->is_content_alloc_inlist is %d ",
					p_item->is_content_alloc_inlist);
								
				gic_raise_softirq(cpumask_of(CONFIG_RTONBOOT_CPUID), RTONBOOT_SGI_ID);
				
				if( (p_item->is_content_alloc_inlist) )
				{
					if( (p_item->p_content) )
					{
						kfree( (p_item->p_content) );
					}	
				}
					
				kfree(p_item);
				
			no_work:
			
				continue;
	}

	return 0;
}

#else

static int ksendcmd(void *foobar)
{
	struct rtbridge_dev * devp = foobar;
	int is_empty;
	struct list_head * pTemp;
	struct send_list_item * p_item = NULL;
	char * ptr_rtonboot;	
    uint32_t * ptemp;
    uint32_t msg_pending_status;
    unsigned long start;
    unsigned long stop;
    char * ptr_msg_header;
    char * ptr_msg_content;
    struct rtonboot_msg_header * p_header;
    uint32_t msg_pending_should_status;
    
    #if (ENABLE_JIHUOMA > 0)
    
		int count = 0;
		char * ptr_rtonboot_check;
		uint32_t value;
		uint32_t val;
		
	#endif
		
    int is_set_atomic;
    int should_work = 0;
    DEFINE_WAIT(wait); 
    
    ptr_rtonboot = get_ptr_rtonboot(); 
    ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
    ptemp = (uint32_t *)(ptr_rtonboot);
 
	for (;;) 
	{		
		/* Wait until there is something to do using proper wait mechanism */
		should_work = 0;
		
		/* Prepare to wait */
		prepare_to_wait(&(devp->send_queue), &wait, TASK_INTERRUPTIBLE);
		
		/* Check condition with lock protection */
		/*mutex_lock(&(devp->send_lock));*/
		
		msg_pending_status = atomic_load_acquire_32(ptemp);
		is_empty = list_empty(&(devp->msg_send_list_head));
		
		if(!is_empty)
		{
			pTemp = (&(devp->msg_send_list_head))->next;	
			p_item = (struct send_list_item *)pTemp;
			
			if(((p_item->msg_header.msg_origin) != MSG_ORIGIN_RTOS) || 
			   ((p_item->msg_header.msg_has_response) != MSG_HAS_RESPONSE))
			{
				msg_pending_should_status = 0;
			}
			else
			{
				msg_pending_should_status = 1;
			}
			
			if(msg_pending_status == msg_pending_should_status)
			{
				should_work = 1;
			}
		}
		
		/*mutex_unlock(&(devp->send_lock));*/
		
		/* If we have work to do, finish waiting and proceed */
		if(should_work)
		{			
			finish_wait(&(devp->send_queue), &wait);
			goto work_to_do;
		}
		
		/* Check for thread stop */
		if (kthread_should_stop())
		{
			finish_wait(&(devp->send_queue), &wait);
			goto no_work;
		}
		
		/* Schedule and wait */
		schedule_timeout(msecs_to_jiffies(SEND_SLEEP_MS));
		finish_wait(&(devp->send_queue), &wait);
		
		#if (ENABLE_JIHUOMA > 0)
		
			/* Periodic checks */
			if(count >= 0)
			{
				++count;
				if(count >= SYSFS_CHECK_CNT)
				{
					count = -1;
					value = read_sysfs_attribute();
				
					if(value != val)
					{
						ptr_rtonboot_check = get_ptr_rtonboot(); 
						ptr_rtonboot_check += 0x1e0;
						sanerfa_done(*(ptr_rtonboot_check));
					}	
				}		
				else if(count >= SNAERFA_CHECK_CNT)
				{
					ptr_rtonboot_check = get_ptr_rtonboot(); 
					ptr_rtonboot_check += 0x1dc;
					value = *((uint32_t *)ptr_rtonboot_check);
					val = do_sanerfa();
				
					if(val != value)
					{
						ptr_rtonboot_check = get_ptr_rtonboot(); 
						ptr_rtonboot_check += 0x1e0;
						sanerfa_done(*(ptr_rtonboot_check));
					}				
				}
			}
			
		#endif	
		
		if (kthread_should_stop())
			goto no_work;
			
		continue;
		
	work_to_do:
		if (kthread_should_stop())
			break;
		
		/* Lock ordering: always acquire spinlock first, then mutex if needed */
		/* But here we only need spinlock for the critical section */
		/*spin_lock(&(devp->send_slock));*/
		
		/* Re-validate the list item (it might have changed while we were waiting) */
		if(list_empty(&(devp->msg_send_list_head)))
		{
			/*spin_unlock(&(devp->send_slock));*/
			
			continue;
		}
		
		pTemp = (&(devp->msg_send_list_head))->next;	
		p_item = (struct send_list_item *)pTemp;
		
		if(((p_item->msg_header.msg_origin) != MSG_ORIGIN_RTOS) || 
		   ((p_item->msg_header.msg_has_response) != MSG_HAS_RESPONSE))
		{
			msg_pending_status = atomic_load_acquire_32(ptemp);
			
			if(msg_pending_status)
			{
				/*spin_unlock(&(devp->send_slock));	*/
				continue;
			}
			
			list_del(&(p_item->send_list));
							
			start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER;
			stop = start + 64;
			
			ptr_msg_header = ptr_rtonboot + 4;
			memcpy(ptr_msg_header, &(p_item->msg_header), sizeof(struct rtonboot_msg_header));
			rtonboot_flush_dcache_range(start, stop);
			
			is_set_atomic = 1;
		}
		else
		{
			list_del(&(p_item->send_list));	
			
			ptr_msg_header = ptr_rtonboot + 4;
			p_header = (struct rtonboot_msg_header *)ptr_msg_header;
			p_header->msg_len = p_item->msg_header.msg_len;
			
			start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER;
			stop = start + 64;
			rtonboot_flush_dcache_range(start, stop);
			
			is_set_atomic = 0;  
		}	
		
		ptr_msg_content = ptr_rtonboot + (p_item->msg_header.offset_content);
		memcpy(ptr_msg_content, (p_item->p_content), p_item->msg_header.msg_len);
		
		start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER 
			+ (p_item->msg_header.offset_content);
		stop = start + p_item->msg_header.msg_len;
		rtonboot_flush_dcache_range(start, stop);
		
		if(is_set_atomic)
		{
			atomic_store_release_32(ptemp, 1);	
		}	
		
		/*spin_unlock(&(devp->send_slock));	*/

#ifdef RTONBOOT_DEBUG
		printk("Winfred Young Begin send to RTOS Core %d\n", p_item->msg_header.msgid);
#endif
		
		RTOnBootTraceDetail("Begin send to RTOS Core p_item->is_content_alloc_inlist is %d ",
			p_item->is_content_alloc_inlist);
			
		gic_raise_softirq(cpumask_of(CONFIG_RTONBOOT_CPUID), RTONBOOT_SGI_ID);
		
		if(p_item->is_content_alloc_inlist && p_item->p_content)
		{
			kfree(p_item->p_content);
		}
			
		kfree(p_item);
		
	no_work:
		
		continue;
	}

	return 0;
}


#endif
void flush_linux_bulk_before_send(uint32_t offset, uint32_t len)
{
	unsigned long start;
    unsigned long stop;
    
    RTOnBootTraceDetail("Enter %s", __func__);
    
    start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + (OFFSET_TO_RTONBOOT_SHARE_BUFFER) 
					+ (OFFSET_TO_RTONBOOT_BULK_BUFFER) + offset;
	stop = start + len;
				
	rtonboot_flush_dcache_range(start, stop);				
}

static const struct file_operations rtbridge_fops =
{
	.owner = THIS_MODULE,
  	.open = rtbridge_open,
  	.release = rtbridge_release,
	.unlocked_ioctl	 = rtbridge_ioctl,
	.mmap = rtbridge_mmap,
	.fasync = rtbridge_fasync,
	.poll = rtbridge_poll,
	.read = rtbridge_read,
};

static char *rtbridge_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
		
	return kasprintf(GFP_KERNEL, "%s", RTBRIDGE_DEVICE_NAME);
}

void del_and_free_send_list_item(struct send_list_item * p_item)
{
	list_del(&(p_item->send_list));
	
	kfree( (p_item->p_content) );
	
	kfree(p_item);
}

void free_send_list(struct list_head * p_head)
{
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct send_list_item * p_item;
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct send_list_item *)pTemp;

		del_and_free_send_list_item(p_item);
	}
}

void del_and_free_pollpara_list_item(struct pollpara_list_item * p_item)
{
	list_del(&(p_item->pollpara_list));
	
	kfree(p_item);
}

void free_pollpara_list(void)
{
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct pollpara_list_item * p_item;
	struct list_head * p_head;
	
	p_head = &(pollpara_list_head);
		
	if( list_empty(p_head) )
			return;
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct pollpara_list_item *)pTemp;

		del_and_free_pollpara_list_item(p_item);
	}
}

void scan_pollpara_list_set_pollpara(uint32_t pid, uint32_t val)
{
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct pollpara_list_item * p_item;
	struct list_head * p_head;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_head = &(pollpara_list_head);
		
	if( list_empty(p_head) )
	{
			printk("%s list_empty", __func__);
			
			return;
	}		
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct pollpara_list_item *)pTemp;

		if( (p_item->pid) == pid )
		{
			p_item->msg_created_received = 1;
			
			p_item->val = val;
			
			wake_up_interruptible( &(p_item->msg_created_wait) );
			
			return;
		}	
	}
	
	printk("scan_pollpara_list_set_pollpara find no entry\n");
}

void del_and_free_k7_msglist_item(struct k7_msglist_item * p_item)
{
	list_del(&(p_item->k7_msglist));
	
	kfree(p_item);
}

void free_k7_msg_list(void)
{
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct k7_msglist_item * p_item;
	struct list_head * p_head;
	
	p_head = &(k7_msglist_head);
		
	if( list_empty(p_head) )
			return;
	
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct k7_msglist_item *)pTemp;

		del_and_free_k7_msglist_item(p_item);
	}
}

static int rtbridge_init(void)
{
		int result;
		char * ptr_rtonboot;
		unsigned long start;
		unsigned long stop;

    	chardev = cdev_alloc();
    	if(chardev == NULL)
    	{
        	return -1;
    	}
    
    	if(alloc_chrdev_region(&dev, 0, 1, RTBRIDGE_DEVICE_NAME))
    	{
    		printk("Winfred Young Register char dev error %s\n", RTBRIDGE_DEVICE_NAME);
    	
    		return -1;
    	} 
    
    	cdev_init(chardev, &rtbridge_fops);
    
    	if(cdev_add(chardev, dev, 1))
    	{
        	printk("Winfred Young Add char dev error %s !\n", RTBRIDGE_DEVICE_NAME);

       	 	result = -1;

			goto fail1;
    	}
    
    	my_class = class_create(THIS_MODULE, "rtbridge_class");
    	if(IS_ERR(my_class)) 
    	{
        	printk("Winfred Young Err: failed in creating rtbridge_class.\n");
        
        	result = -1;

			goto fail2;
    	}
    	
    	my_class->devnode = rtbridge_devnode;
    
    	/* register your own device in sysfs, and this will cause udevd to create corresponding device node */
    	device_create(my_class, NULL, dev, NULL, RTBRIDGE_DEVICE_NAME);
    	
    	rtbridge_devp = kmalloc(sizeof(struct rtbridge_dev), GFP_KERNEL);
		if (!rtbridge_devp)
		{
    		result = -ENOMEM;

    		goto fail_malloc;
		}
		
		memset(rtbridge_devp, 0, sizeof(struct rtbridge_dev));
		
		mutex_init(&(rtbridge_devp->send_lock));
		
		spin_lock_init(&(rtbridge_devp->send_slock));
		
		INIT_LIST_HEAD(&(rtbridge_devp->msg_send_list_head));
		
		INIT_LIST_HEAD(&pollpara_list_head);
		
		INIT_LIST_HEAD(&k7_msglist_head);
		
		init_waitqueue_head(&(rtbridge_devp->send_queue));
		
		ptr_rtonboot = get_ptr_rtonboot(); 
		
		ptr_bulk_start = ptr_rtonboot + ((unsigned long)(OFFSET_TO_RTONBOOT_SHARE_BUFFER))
			+ ((unsigned long)(OFFSET_TO_RTONBOOT_BULK_BUFFER));
			
		
		*((uint32_t *)ptr_bulk_start) = 0;
		
		ptr_trace_start = ptr_bulk_start + 4;
		
		*((uint32_t *)ptr_trace_start) = 0;
		
		ptr_trace_start = ptr_bulk_start + 8;
		
		start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER;
    
		stop = start + 64;
				
		rtonboot_flush_dcache_range(start, stop);  
		
		rtbridge_devp->send_thread = kthread_run(ksendcmd, rtbridge_devp, "%s", "ksendcmd");
		if (IS_ERR(rtbridge_devp->send_thread)) 
		{
			printk("Winfred Uoung can't start ksendcmd thread\n");
			result = -ENOMEM;
			goto fail_malloc;
		}
		
		return 0;


	fail_malloc:

		device_destroy(my_class, dev);

    	class_destroy(my_class);

	fail2:

		cdev_del(chardev);

	fail1: 

		unregister_chrdev_region(MKDEV(DP_MAJOR,DP_MINOR),1);

		return result;
}

static void rtbridge_exit(void)
{
		if (!IS_ERR(rtbridge_devp->send_thread))
			kthread_stop(rtbridge_devp->send_thread);
			
		unregister_chrdev_region(MKDEV(DP_MAJOR,DP_MINOR),1);

		cdev_del(chardev);

		device_destroy(my_class, dev);

		class_destroy(my_class);
		
		free_send_list( &(rtbridge_devp->msg_send_list_head) );
		
		free_pollpara_list();
		
		free_k7_msg_list();
		
		kfree(rtbridge_devp);
}

int get_rtonboot_trace_level(void)
{
	char * ptr_rtonboot;
	uint64_t level;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_TRACE_LEVEL;
	
	level = *((uint64_t *)ptr_rtonboot);
	
	return ((int)level);
}	

void RTOnBootTrace(int cur_level, char * fmt, va_list args)
{
	int level;
	uint32_t cur_len;
	uint32_t cur_len_bak;
	uint32_t cnt;
	uint32_t len1;
	uint32_t len2;
	char * ptr_rtonboot;
	uint64_t trace_lock;
	unsigned long start;
    unsigned long stop;
    char buf[80];
    char * ptr_append;
	
	level = get_rtonboot_trace_level();
	
	if(!level)
		return;
		
	if(level < cur_level)
			return;
			
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_TRACE_LOCK;
	
	trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	
	while(trace_lock)
	{
		usleep_range(1, 1);
		
		trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	}
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 1);
	
	cur_len = *((uint32_t *)ptr_bulk_start);
	
	cnt = *((uint32_t *)(ptr_bulk_start + 4));
	
	sprintf(&buf[0], "CNT: %d ", cnt);
	
	len1 = strlen(&buf[0]);
	
	len2 = vsnprintf(NULL, 0, fmt, args);
	
	if( (cur_len + len1 + len2 + 1) >= (RTONBOOT_BULK_BUFFER_SIZE - 8) )
	{
		ptr_append = ptr_trace_start;
		
		cur_len_bak = 0;
		
		cur_len = len1 + len2 + 1;
	}	
	else
	{
		ptr_append = ptr_trace_start + cur_len;
		
		cur_len_bak = cur_len;
		
		cur_len += len1 + len2 + 1;
	}	
	
	strcpy(ptr_append, &buf[0]);
	
	ptr_append += len1;
	
	vsnprintf(ptr_append, len2 + 1, fmt, args);
	
	*((uint32_t *)ptr_bulk_start) = cur_len;
	
	*((uint32_t *)(ptr_bulk_start + 4)) = cnt + 1;
	
	start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER;
    
	stop = start + 64;
				
	rtonboot_flush_dcache_range(start, stop);  
	
	start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len_bak;
			
	stop = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len;	
			
	rtonboot_flush_dcache_range(start, stop);
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 0);
}

void RTOnBootTraceUser(int cur_level, char * fmt, uint32_t len2)
{
	int level;
	uint32_t cur_len;
	uint32_t cur_len_bak;
	uint32_t cnt;
	uint32_t len1;
	char * ptr_rtonboot;
	uint64_t trace_lock;
	unsigned long start;
    unsigned long stop;
    char buf[80];
    char * ptr_append;
	
	level = get_rtonboot_trace_level();
	
	if(!level)
		return;
		
	if(level < cur_level)
			return;
			
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_TRACE_LOCK;
	
	trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	
	while(trace_lock)
	{
		usleep_range(1, 1);
		
		trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	}
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 1);
	
	cur_len = *((uint32_t *)ptr_bulk_start);
	
	cnt = *((uint32_t *)(ptr_bulk_start + 4));
	
	sprintf(&buf[0], "CNT: %d ", cnt);
	
	len1 = strlen(&buf[0]);
	
	if( (cur_len + len1 + len2 + 1) >= (RTONBOOT_BULK_BUFFER_SIZE - 8) )
	{
		ptr_append = ptr_trace_start;
		
		cur_len_bak = 0;
		
		cur_len = len1 + len2 + 1;
	}	
	else
	{
		ptr_append = ptr_trace_start + cur_len;
		
		cur_len_bak = cur_len;
		
		cur_len += len1 + len2 + 1;
	}	
	
	strcpy(ptr_append, &buf[0]);
	
	strcat(ptr_append, fmt);
	
	*((uint32_t *)ptr_bulk_start) = cur_len;
	
	*((uint32_t *)(ptr_bulk_start + 4)) = cnt + 1;
	
	start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER;
    
	stop = start + 64;
				
	rtonboot_flush_dcache_range(start, stop);  
	
	start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len_bak;
			
	stop = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len;	
			
	rtonboot_flush_dcache_range(start, stop);			
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 0);
}

void RTOnBootTraceEnd(void)
{
	uint32_t cur_len;
	uint32_t cur_len_bak;
	uint32_t cnt;
	uint32_t len1;
	char * ptr_rtonboot;
	uint64_t trace_lock;
	unsigned long start;
    unsigned long stop;
    char buf[80];
    char * ptr_append;
			
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_TRACE_LOCK;
	
	trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	
	while(trace_lock)
	{
		usleep_range(1, 1);
		
		trace_lock = atomic_load_acquire_64((uint64_t *)ptr_rtonboot);
	}
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 1);
	
	cur_len = *((uint32_t *)ptr_bulk_start);
	
	cnt = 2097153;
	
	sprintf(&buf[0], "CNT: %d ", cnt);
	
	len1 = strlen(&buf[0]);
	
	if( (cur_len + len1 + 1) < (RTONBOOT_BULK_BUFFER_SIZE - 8) )
	{
		ptr_append = ptr_trace_start + cur_len;
		
		cur_len_bak = cur_len;
		
		cur_len += len1 + 1;
	}	
	else
	{
		ptr_append = NULL;
	}	
	
	if(ptr_append)
	{
		strcpy(ptr_append, &buf[0]);
	
		start = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len_bak;
			
		stop = ((unsigned long)(CONFIG_IMAGE_VADDR_BASE)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len;	
			
		rtonboot_flush_dcache_range(start, stop);
	}
	
	atomic_store_release_64((uint64_t *)ptr_rtonboot, 0);
}

void RTOnBootTraceDetail(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RTOnBootTrace(RTONBOOT_TRACE_LEVEL_DETAIL, fmt, args);
	va_end(args);
}

void RTOnBootTraceInfo(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RTOnBootTrace(RTONBOOT_TRACE_LEVEL_INFO, fmt, args);
	va_end(args);
}		

void RTOnBootTraceWarn(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RTOnBootTrace(RTONBOOT_TRACE_LEVEL_WARN, fmt, args);
	va_end(args);
}

void RTOnBootTraceError(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RTOnBootTrace(RTONBOOT_TRACE_LEVEL_ERROR, fmt, args);
	va_end(args);
}

#if (ENABLE_JIHUOMA > 0)

#define ECB 1

#define AES128 1

#define AES_KEYLEN 16   // Key length in bytes
#define AES_keyExpSize 176

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
};

#define Nb 4

#define Nk 4        
#define Nr 10

typedef uint8_t state_t[4][4];

static const uint8_t sbox[256] = {
  //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };
  
static const uint8_t rsbox[256] = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };
  
static const uint8_t Rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };
  
#define getSBoxValue(num) (sbox[(num)])

static void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key)
{
  unsigned i, j, k;
  uint8_t tempa[4]; // Used for the column/row operations
  
  // The first round key is the key itself.
  for (i = 0; i < Nk; ++i)
  {
    RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
    RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
    RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
    RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
  }

  // All other round keys are found from the previous round keys.
  for (i = Nk; i < Nb * (Nr + 1); ++i)
  {
    {
      k = (i - 1) * 4;
      tempa[0]=RoundKey[k + 0];
      tempa[1]=RoundKey[k + 1];
      tempa[2]=RoundKey[k + 2];
      tempa[3]=RoundKey[k + 3];

    }

    if (i % Nk == 0)
    {
      // This function shifts the 4 bytes in a word to the left once.
      // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

      // Function RotWord()
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1];
        tempa[1] = tempa[2];
        tempa[2] = tempa[3];
        tempa[3] = u8tmp;
      }

      // SubWord() is a function that takes a four-byte input word and 
      // applies the S-box to each of the four bytes to produce an output word.

      // Function Subword()
      {
        tempa[0] = getSBoxValue(tempa[0]);
        tempa[1] = getSBoxValue(tempa[1]);
        tempa[2] = getSBoxValue(tempa[2]);
        tempa[3] = getSBoxValue(tempa[3]);
      }

      tempa[0] = tempa[0] ^ Rcon[i/Nk];
    }

    j = i * 4; k=(i - Nk) * 4;
    RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
    RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
    RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
    RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
  }
}

static void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key)
{
  KeyExpansion(ctx->RoundKey, key);
}

static void AddRoundKey(uint8_t round, state_t* state, const uint8_t* RoundKey)
{
  uint8_t i,j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[i][j] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
    }
  }
}

static void SubBytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = getSBoxValue((*state)[j][i]);
    }
  }
}

static void ShiftRows(state_t* state)
{
  uint8_t temp;

  // Rotate first row 1 columns to left  
  temp           = (*state)[0][1];
  (*state)[0][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[3][1];
  (*state)[3][1] = temp;

  // Rotate second row 2 columns to left  
  temp           = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp           = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  // Rotate third row 3 columns to left
  temp           = (*state)[0][3];
  (*state)[0][3] = (*state)[3][3];
  (*state)[3][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[1][3];
  (*state)[1][3] = temp;
}

static uint8_t xtime(uint8_t x)
{
  return ((x<<1) ^ (((x>>7) & 1) * 0x1b));
}

static void MixColumns(state_t* state)
{
  uint8_t i;
  uint8_t Tmp, Tm, t;
  for (i = 0; i < 4; ++i)
  {  
    t   = (*state)[i][0];
    Tmp = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3] ;
    Tm  = (*state)[i][0] ^ (*state)[i][1] ; Tm = xtime(Tm);  (*state)[i][0] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][1] ^ (*state)[i][2] ; Tm = xtime(Tm);  (*state)[i][1] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][2] ^ (*state)[i][3] ; Tm = xtime(Tm);  (*state)[i][2] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][3] ^ t ;              Tm = xtime(Tm);  (*state)[i][3] ^= Tm ^ Tmp ;
  }
}

#define Multiply(x, y)                                \
      (  ((y & 1) * x) ^                              \
      ((y>>1 & 1) * xtime(x)) ^                       \
      ((y>>2 & 1) * xtime(xtime(x))) ^                \
      ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^         \
      ((y>>4 & 1) * xtime(xtime(xtime(xtime(x))))))   \

#define getSBoxInvert(num) (rsbox[(num)])

static void InvMixColumns(state_t* state)
{
  int i;
  uint8_t a, b, c, d;
  for (i = 0; i < 4; ++i)
  { 
    a = (*state)[i][0];
    b = (*state)[i][1];
    c = (*state)[i][2];
    d = (*state)[i][3];

    (*state)[i][0] = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
    (*state)[i][1] = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
    (*state)[i][2] = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
    (*state)[i][3] = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
  }
}

static void InvSubBytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = getSBoxInvert((*state)[j][i]);
    }
  }
}

static void InvShiftRows(state_t* state)
{
  uint8_t temp;

  // Rotate first row 1 columns to right  
  temp = (*state)[3][1];
  (*state)[3][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[0][1];
  (*state)[0][1] = temp;

  // Rotate second row 2 columns to right 
  temp = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  // Rotate third row 3 columns to right
  temp = (*state)[0][3];
  (*state)[0][3] = (*state)[1][3];
  (*state)[1][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[3][3];
  (*state)[3][3] = temp;
}

static void Cipher(state_t* state, const uint8_t* RoundKey)
{
  uint8_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(0, state, RoundKey);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without MixColumns()
  for (round = 1; ; ++round)
  {
    SubBytes(state);
    ShiftRows(state);
    if (round == Nr) {
      break;
    }
    MixColumns(state);
    AddRoundKey(round, state, RoundKey);
  }
  // Add round key to last round
  AddRoundKey(Nr, state, RoundKey);
}

static void InvCipher(state_t* state, const uint8_t* RoundKey)
{
  uint8_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(Nr, state, RoundKey);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without InvMixColumn()
  for (round = (Nr - 1); ; --round)
  {
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(round, state, RoundKey);
    if (round == 0) {
      break;
    }
    InvMixColumns(state);
  }

}

static void __attribute__((unused)) AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call encrypts the PlainText with the Key using AES algorithm.
  Cipher((state_t*)buf, ctx->RoundKey);
}

static void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call decrypts the PlainText with the Key using AES algorithm.
  InvCipher((state_t*)buf, ctx->RoundKey);
}

struct AES_ctx g_RTOSStrcut;
uint8_t g_RTOSText[16] = {0};

static void prepare_soemthing(const uint8_t* key)
{
	AES_init_ctx(&g_RTOSStrcut, key);
}

static void after_soemthing(uint8_t* buf)
{
	AES_ECB_decrypt(&g_RTOSStrcut, buf);
}

void print_hex(const uint8_t *data, size_t len) {
	size_t i;
	
    for (i = 0; i < len; i++) {
        printk("%02x ", data[i]);
    }
    printk("\n");
}

#define USER_SHARED_BUF_FIXVAR_SIZE 1024

uint32_t do_sanerfa(void)
{
	uint8_t key[16] = {
        0x31, 0x39, 0x37, 0x31, 0x30, 0x33, 0x30, 0x32,
        0x31, 0x39, 0x37, 0x38, 0x31, 0x31, 0x32, 0x34
    };
    uint8_t plaintext[16] = {0};
    uint8_t * ptr_ciper_serial;
	uint32_t value;
	char * ptr_rtonboot;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
    ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER + USER_SHARED_BUF_FIXVAR_SIZE - 16; 	
	
	ptr_ciper_serial = (uint8_t *)(ptr_rtonboot);
    
	prepare_soemthing(key);
    
    memcpy(plaintext, ptr_ciper_serial, sizeof(plaintext));
    
    /*print_hex(plaintext, sizeof(plaintext));*/
    
    after_soemthing(plaintext);
		
	value = *((uint32_t *)(&plaintext[0]));
	
	return value;		
}

int hex_to_int(const char *hex_str) {
    int result = 0;
       
    int sign = 1;
    if (*hex_str == '-') {
        sign = -1;
        hex_str++;
    } else if (*hex_str == '+') {
        hex_str++;
    }
    
    if (*hex_str == '0' && (*(hex_str + 1) == 'x' || *(hex_str + 1) == 'X')) {
        hex_str += 2;
    }
    
    while (*hex_str != '\0') {
        char c = *hex_str;
        int digit;
        
        if ((c >= '0' && c <= '9')) {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + c - 'a';
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + c - 'A';
        } else {
            break;
        }
        
        result = result * 16 + digit;
        hex_str++;
    }
    
    return sign * result;
}

uint32_t read_sysfs_attribute(void)
{
    struct file *filp;
    loff_t pos = 0;
    mm_segment_t old_fs;
    uint32_t ret;
    int err;
    char buf[20];
    
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    filp = filp_open("/sys/block/mmcblk0/device/serial", O_RDONLY, 0);
    if (IS_ERR(filp)) 
    {
        ret = 0;
        
        goto out;
    }
    
    err = kernel_read(filp, &buf[0], 20, &pos);
    if (err < 0) 
    {
        filp_close(filp, NULL);
        
        ret = 0;
        
        goto out;
    }
    
    if (err < 20)
        buf[err] = '\0';
    else
        buf[19] = '\0';
        
    ret = (uint32_t)(hex_to_int(&buf[0]));    
    
    
    filp_close(filp, NULL);

out:
    set_fs(old_fs);
    return ret;
}

#endif

MODULE_AUTHOR("Winfred Young");
MODULE_LICENSE("GPL");
/*MODULE_SOFTDEP("post: mmc_block");*/

module_init(rtbridge_init);
module_exit(rtbridge_exit);

