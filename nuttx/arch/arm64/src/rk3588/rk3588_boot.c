#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/kthread.h>
#include <nuttx/spinlock.h>
#include <nuttx/kmalloc.h>
#include <nuttx/sched.h>
#include <nuttx/signal.h>

#include <nuttx/cache.h>
#ifdef CONFIG_PAGING
#  include <nuttx/page.h>
#endif

#include <arch/chip/chip.h>

#include "rtonboot_trace.h"

#include <stdatomic.h>
#include <pthread.h>

#ifdef CONFIG_SMP
#include "arm64_smp.h"
#endif

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "arm64_mmu.h"
#include "rk3588_boot.h"
#include "rk3588_serial.h"

#include "heap_4.h"

#include "rtospub.h"

extern int pipe_printf(const char *fmt, ...);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct arm_mmu_region g_mmu_regions[] =
{
  MMU_REGION_FLAT_ENTRY("DEVICE_REGION",
                        CONFIG_DEVICEIO_BASEADDR, CONFIG_DEVICEIO_SIZE,
                        MT_DEVICE_NGNRNE | MT_RW | MT_SECURE),

  MMU_REGION_FLAT_ENTRY("DRAM0_S0",
                        CONFIG_RAMBANK1_ADDR, CONFIG_RAMBANK1_SIZE,
                        MT_NORMAL | MT_RW | MT_SECURE),
};

const struct arm_mmu_config g_mmu_config =
{
  .num_regions = nitems(g_mmu_regions),
  .mmu_regions = g_mmu_regions,
};


void arm64_el_init(void)
{
  /* TODO: RK3568 set init sys clock */
}

void enable_cache_el1(void)
{
  #if 0
  	
  uint64_t value;

  value = read_sysreg(sctlr_el1);
  write_sysreg((value /*| SCTLR_M_BIT*/ /*| SCTLR_C_BIT */ | SCTLR_I_BIT /*| SCTLR_SA_BIT*/), 
	sctlr_el1);

  /* Ensure the MMU enable takes effect immediately */

  ARM64_ISB();
  
  #endif
}

void arm64_chip_boot(void)
{
  /* MAP IO and DRAM, enable MMU. */

  #if 0
  
  arm64_mmu_init(true);

  #else
  
  enable_cache_el1();
  
  #endif

#if defined(CONFIG_SMP) || defined(CONFIG_ARCH_HAVE_PSCI)
	/*  arm64_psci_init("smc");*/
#endif

  /* Perform board-specific device initialization. This would include
   * configuration of board specific resources such as GPIOs, LEDs, etc.
   */

  rk3588_board_initialize();

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization if we are going to use the serial
   * driver.
   */

  arm64_earlyserialinit();
  
#endif
}

#if defined(CONFIG_NET) && !defined(CONFIG_NETDEV_LATEINIT)
void arm64_netinitialize(void)
{
  /* TODO: Support net initialize */
}
#endif

extern void __asm_flush_dcache_range(unsigned long start, unsigned long stop);

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void flush_dcache_range(unsigned long start, unsigned long stop)
{
	__asm_flush_dcache_range(start, stop);
}

extern void __asm_invalidate_dcache_range(unsigned long start, unsigned long stop);

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	__asm_invalidate_dcache_range(start, stop);
}

void prepare_send_result_and_wake_up(void);

void create_and_init_rtos_send_thread(void);

extern int rtos_printf(const char *fmt, ...);

extern int linux_printf(const char *fmt, ...);

extern int arm64_gic_raise_rtonboot_sgi(void);

int is_main_first_kick_received = 0;

struct work_s rtonboot_commu_bhalf;

struct work_s rtonboot_slave_send_bhalf;

mutex_t g_commu_bottomhalf_lock = NXMUTEX_INITIALIZER; 

int is_root_list_alloc = 0;

struct list_head * p_root_list_head = NULL;

struct list_head rtos_prepare_send_list_head;

mutex_t g_rtos_prepare_send_lock = NXMUTEX_INITIALIZER; 

char * g_exec_cmd_str;
extern sem_t g_cmd_ready_sem;

uint32_t offset_root_list_head = 0;

int prepare_root_list_item_and_add(uint32_t pid, struct root_list_item ** pp_item)
{
	struct root_list_item * p_item;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_item = (struct root_list_item *)pvPortMalloc( sizeof(struct root_list_item) );
	if(!p_item)
	{
		linux_printf("Winfred Young Cannot Malloc root list item");
		
		RtonbootTraceError("Cannot Malloc root list item");
			
		return RESP_STATUS_NOMEM;
	}
	
	INIT_LIST_HEAD( &(p_item->msg_list_head) );
	
	p_item->pid = pid;
	
	p_item->is_receive = 0;
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	list_add_tail(&(p_item->root_list), p_root_list_head);
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);
	
	*(pp_item) = p_item;
	
	return RESP_STATUS_SUCCESS;
}

int prepare_msg_list_item_and_add(uint32_t pid, uint8_t * p_start, struct rtonboot_resp_header * p_resp_header,
	struct rtonboot_msg_header * p_dst_header, struct root_list_item * p_root_item)
{
	struct msg_list_item * p_item;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_item = (struct msg_list_item *)pvPortMalloc( sizeof(struct msg_list_item) );
	if(!p_item)
	{
		linux_printf("Winfred Young Cannot Malloc msg list item");
		
		RtonbootTraceError("Cannot Malloc msg list item");
			
		return RESP_STATUS_NOMEM;
	}
	
	p_item->msg_header.pid = pid;
	
	p_item->msg_header.msgid = p_dst_header->msgid;
	
	p_item->msg_header.max_content_size = p_dst_header->max_content_size;
	
	p_item->msg_header.msg_len = p_dst_header->msg_len;
	
	p_item->msg_header.msg_origin = p_dst_header->msg_origin;
	
	p_item->msg_header.msg_has_response = p_dst_header->msg_has_response;
	
	p_item->msg_header.is_broadcast = p_dst_header->is_broadcast;
	
	p_item->msg_header.is_kernel = p_dst_header->is_kernel;
	
	p_item->msg_header.offset_content = p_resp_header->offset_content;
	
	p_item->msg_header.async_or_callback = p_dst_header->async_or_callback;
	
	p_item->msg_header.callback_priv = p_dst_header->callback_priv;
	
	p_item->end_callback = 0;
	p_item->end_callback_priv = 0;
	p_item->resp_callback = 0;
	p_item->resp_callback_priv = 0;
	
	p_resp_header->offset_cur_list = ((uint64_t)(p_item)) - ((uint64_t)(p_start));
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	list_add_tail(&(p_item->msg_list), &(p_root_item->msg_list_head));
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);
	
	return RESP_STATUS_SUCCESS;
}

#ifdef ALLOW_CREATE_DUPLICATE

void update_msg_list_item(uint8_t * p_start, struct msg_list_item * p_item, struct rtonboot_resp_header * p_resp_header, struct rtonboot_msg_header * p_dst_header)
{
	p_item->msg_header.async_or_callback = p_dst_header->async_or_callback;
	
	p_item->msg_header.callback_priv = p_dst_header->callback_priv;
	
	p_resp_header->offset_cur_list = ((uint64_t)(p_item)) - ((uint64_t)(p_start));
	
	p_resp_header->offset_content = p_item->msg_header.offset_content;
}	

#endif
int scan_root_list_find_this_pid_entry_or_add(uint8_t * p_start, struct rtonboot_resp_header * p_resp_header,
	struct rtonboot_msg_header * p_dst_header)
{
	uint32_t pid;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct root_list_item * p_item;
	uint32_t resp_status;
	struct list_head * p_head;
	struct msg_list_item * p_msg_item;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if( (p_dst_header->is_broadcast) )
	{
		pid = PID_BROADCAST_MSG;
	}
	else if( (p_dst_header->is_kernel) &&
		((p_dst_header->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		pid = PID_KERNEL_MSG;
	}
	else
	{
		pid = p_dst_header->pid;
	}
	
	if(list_empty(p_root_list_head))
	{
		goto add_to_root;
	}
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;

		if(pid == (p_item->pid) )
		{
			nxmutex_unlock(&g_commu_bottomhalf_lock);
			
			goto found_on_root;
		}	
	}
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);
	
	goto add_to_root;
	
	found_on_root:
	
		goto add_to_msg_list;
	
	add_to_root:
	
		resp_status = prepare_root_list_item_and_add(pid, &p_item);
		if(resp_status != RESP_STATUS_SUCCESS)
			return resp_status;
			
	add_to_msg_list:
	
		p_head = &(p_item->msg_list_head);
		
		if( list_empty(p_head) )
			goto just_add;
		
		nxmutex_lock(&g_commu_bottomhalf_lock);
			
		for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
		{
			pTempBak = pTemp->next;
			
			p_msg_item = (struct msg_list_item *)pTemp;

			if( (p_msg_item->msg_header.msgid) == (p_dst_header->msgid) )
			{
				nxmutex_unlock(&g_commu_bottomhalf_lock);
			
				goto found_on_msg_list;
			}	
		}
		
		nxmutex_unlock(&g_commu_bottomhalf_lock);
		
		goto just_add;
			
	found_on_msg_list:
	
	    #ifndef ALLOW_CREATE_DUPLICATE
	
			p_resp_header->offset_cur_list = ((uint64_t)(p_msg_item)) - ((uint64_t)(p_start));
		
			RtonbootTraceError("in %s RESP_STATUS_MSG_DUPLICATE", __func__);
	
			return RESP_STATUS_MSG_DUPLICATE;
			
		#else
		
			update_msg_list_item(p_start, p_msg_item, p_resp_header, p_dst_header);
			
			return RESP_STATUS_MSG_DUPLICATE;
		
		#endif	
				
	just_add:
	
		resp_status = prepare_msg_list_item_and_add(pid, p_start, p_resp_header,
			p_dst_header, p_item);
		if(resp_status != RESP_STATUS_SUCCESS)
			return resp_status;
		
		return RESP_STATUS_SUCCESS;
}	

int handle_create_msg_list(uint8_t * p_start, struct rtonboot_resp_header * p_resp_header,
	struct rtonboot_msg_header * p_dst_header)
{
	uint32_t resp_status;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if(!is_root_list_alloc)
	{
		p_root_list_head = (struct list_head *)pvPortMalloc( sizeof(struct list_head) );
		
		if(!p_root_list_head)
		{
			linux_printf("Winfred Young Cannot Malloc p_root_list_head");
			
			RtonbootTraceError("in %s Cannot Malloc p_root_list_head", __func__);
			
			return RESP_STATUS_NOMEM;
		}
		
		offset_root_list_head = ((uint64_t)(p_root_list_head)) - ((uint64_t)(p_start));
		
		p_resp_header->offset_root_list_head = offset_root_list_head;
		
		INIT_LIST_HEAD(p_root_list_head);
		
		is_root_list_alloc = 1;
	}
	else
	{
		p_resp_header->offset_root_list_head = offset_root_list_head;
	}	
	
	resp_status = scan_root_list_find_this_pid_entry_or_add(p_start, p_resp_header, p_dst_header);
	if(resp_status != RESP_STATUS_SUCCESS)
		return resp_status;
	
	return RESP_STATUS_SUCCESS;
}

void scan_msglist_calculate_dirty_list_range(uint8_t ** pp_start, uint8_t ** pp_end, struct root_list_item * p_item)
{
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	uint8_t * p_start;
	uint8_t * p_end;
	uint8_t * ptemp;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_start = *(pp_start);
	p_end = *(pp_end);
	
	p_head = &(p_item->msg_list_head);
		
	if( list_empty(p_head) )
			return;
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
			
		ptemp = (uint8_t * )pTemp;

		if(ptemp < p_start)
		{
			p_start = ptemp;
		}
		else if(ptemp > p_end)
		{
			p_end = ptemp;
		}
	}
	
	*(pp_start) = p_start;
	*(pp_end) = p_end;
}

void calculate_dirty_list_range(uint8_t ** pp_start, uint8_t ** pp_end)
{
	uint8_t * p_start;
	uint8_t * p_end;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct root_list_item * p_item;
	uint8_t * ptemp;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_start = (uint8_t * )p_root_list_head;
	p_end = (uint8_t * )p_root_list_head;
	
	if(list_empty(p_root_list_head))
	{
		goto set_return;
	}
	
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;
		
		ptemp = (uint8_t *)p_item;
		
		if(ptemp < p_start)
		{
			p_start = ptemp;
		}
		else if(ptemp > p_end)
		{
			p_end = ptemp;
		}
		
		scan_msglist_calculate_dirty_list_range(&p_start, &p_end, p_item);
	}
	
	set_return:
	
		*(pp_start) = p_start;
		*(pp_end) = p_end;
}

void flush_dirty_list(void)
{
	uint8_t * p_start;
	uint8_t * p_end;
	unsigned long long start;
    unsigned long long stop;
    
    RtonbootTraceDetail("Enter %s", __func__);
	
	calculate_dirty_list_range(&p_start, &p_end);
	
	if(p_end < p_start)
		return;
	else if(p_end == p_start)
	{
		start = (unsigned long long)(p_start);
				
		stop = start + 64;
	}		
	else
	{
		start = (unsigned long long)(p_start);
				
		stop = (unsigned long long)(p_end);
		
		stop += 64;
	}
	
	flush_dcache_range(start, stop);
}

void exec_cmd_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	g_exec_cmd_str = (char *)p_content;
			
	if(g_exec_cmd_str)
	{	
		sem_post(&g_cmd_ready_sem);
	}
}

void testrr_end_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#ifdef RTONBOOT_DEBUG
		
		linux_printf("Winfred Young MSGID_TESTRR response received %x\n", *(p_content) );
			
	#endif	
	
	prepare_send_result_and_wake_up();
}	

void set_end_callback(uint32_t dst_msgid, struct msg_list_item * p_item)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	if( dst_msgid == MSGID_EXECCMD_ON_RTOS )
    {
		p_item->end_callback = (uint64_t)(&exec_cmd_callback);
		
		p_item->end_callback_priv = 0;	
	}
	else if( dst_msgid == MSGID_TESTRR )
    {
		p_item->end_callback = (uint64_t)(&testrr_end_callback);
		
		p_item->end_callback_priv = 0;
	}
	else
	{
		rtospub_set_end_callback(dst_msgid, &(p_item->end_callback), &(p_item->end_callback_priv) );
	}	
}

void testk3_callback(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct testk3_msg_content * p_testk3_content;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	*(p_msglen) = sizeof(struct testk3_msg_content);
		
	p_testk3_content = (struct testk3_msg_content *)(p_content);
		
	p_testk3_content->msgid = msgid;
}	

void set_resp_callback(uint32_t dst_msgid, struct msg_list_item * p_item)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	if(dst_msgid == MSGID_TESTK3)
	{
		p_item->resp_callback = (uint64_t)(&testk3_callback);
		
		p_item->resp_callback_priv = 0;
	}
	else
	{
		rtospub_set_resp_callback(dst_msgid, &(p_item->resp_callback), &(p_item->resp_callback_priv) );
	}
}

extern uint32_t g_non_tty_debug;

extern uint32_t g_non_tty_cmd_result_curpos;

extern uint32_t g_non_tty_cmd_result_bufoff;

void prepare_callback_send_execcmd_result(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct send_result_msg_content * p_result_content;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_result_content = (struct send_result_msg_content *)(p_content);
		
	p_result_content->msgid = msgid;
	p_result_content->non_tty_debug = g_non_tty_debug;
	p_result_content->non_tty_cmd_result_bufoff = g_non_tty_cmd_result_bufoff;
	p_result_content->non_tty_cmd_result_buflen = g_non_tty_cmd_result_curpos;
	
	g_non_tty_debug = 0;
		
	*(p_msglen) = sizeof(struct send_result_msg_content);
}

void prepare_callback_testrr(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct testrr_msg_content * p_testrr_content;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_testrr_content = (struct testrr_msg_content *)(p_content);
		
	p_testrr_content->msgid = msgid;
		
	*(p_msglen) = sizeof(struct testrr_msg_content);
}		

void prepare_callback_testbc(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct testbc_msg_content * p_testbc_content;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_testbc_content = (struct testbc_msg_content *)(p_content);
		
	p_testbc_content->msgid = msgid;
		
	*(p_msglen) = sizeof(struct testbc_msg_content);
}	

void set_prepare_callback(uint32_t dst_msgid, prepare_send_callack_func_t * p_cb, void ** p_cb_priv)
{
	RtonbootTraceDetail("Enter %s dst_msgid %d", __func__, dst_msgid);
	
	if(dst_msgid == MSGID_SEND_EXECCMD_RESULT)
	{
		*(p_cb) = &prepare_callback_send_execcmd_result;
		*(p_cb_priv) = NULL;
	}
	else if(dst_msgid == MSGID_TESTRR)
	{
		*(p_cb) = &prepare_callback_testrr;
		*(p_cb_priv) = NULL;
	}
	else if(dst_msgid == MSGID_TESTBC)
	{
		*(p_cb) = &prepare_callback_testbc;
		*(p_cb_priv) = NULL;
    }		
	else
	{
		rtospub_set_prepare_callback(dst_msgid, p_cb, p_cb_priv);
	}	
}	

void rtos_create_msg_hook(uint32_t dst_msgid, uint32_t pid, struct rtonboot_resp_header * p_resp_header, 
	int is_rtos_origin)
{
	struct prepare_send_list_item * p_prepare_item;
	
	RtonbootTraceDetail("Enter %s dst_msgid %d", __func__, dst_msgid);
	
	if(!is_rtos_origin)
		return;
	
	p_prepare_item = (struct prepare_send_list_item *)(kmm_malloc(sizeof(struct prepare_send_list_item)));
	if(!p_prepare_item)
	{
		linux_printf("Winfred Young Cannot Malloc prepare_send_list_item\n");
		
		RtonbootTraceError("in %s Cannot Malloc prepare_send_list_item", __func__);
		
		return;
	}	
	
	memset(p_prepare_item, 0, sizeof(struct prepare_send_list_item) );
	
	p_prepare_item->msgid = dst_msgid;
	
	p_prepare_item->offset_content = p_resp_header->offset_content;
	
	p_prepare_item->offset_cur_list = p_resp_header->offset_cur_list;
	
	set_prepare_callback(dst_msgid, &(p_prepare_item->prepare_callback), &(p_prepare_item->prepare_priv));
	
	nxmutex_lock(&g_rtos_prepare_send_lock);
	
	list_add_tail(&(p_prepare_item->prepare_send_list), &rtos_prepare_send_list_head);
	
	nxmutex_unlock(&g_rtos_prepare_send_lock);
}	

void call_rtos_create_hook(uint32_t dst_msgid, uint32_t pid, struct rtonboot_resp_header * p_resp_header,
	struct rtonboot_msg_header * p_dst_header, struct msg_list_item * p_item, int is_rtos_origin)
{
	RtonbootTraceDetail("Enter %s dst_msgid %d", __func__, dst_msgid);
	
	if( ( (p_dst_header->msg_origin == MSG_ORIGIN_LINUX) && (p_dst_header->msg_has_response == MSG_NO_RESPONSE) ) ||
		( (p_dst_header->msg_origin == MSG_ORIGIN_RTOS) && (p_dst_header->msg_has_response == MSG_HAS_RESPONSE) ) )
	{	
		set_end_callback(dst_msgid, p_item);
	}
	else if( (p_dst_header->msg_origin == MSG_ORIGIN_LINUX) && (p_dst_header->msg_has_response == MSG_HAS_RESPONSE) )
	{
		set_resp_callback(dst_msgid, p_item);
	}
	
	rtos_create_msg_hook(dst_msgid, pid, p_resp_header, is_rtos_origin);		
}

void call_rtos_delete_hook(uint32_t dst_msgid, uint32_t pid, int is_rtos_origin)
{	
	struct prepare_send_list_item * p_prepare_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	
	RtonbootTraceDetail("Enter %s dst_msgid %d", __func__, dst_msgid);
	
	if(!is_rtos_origin)
		return;
	
	p_head = &(rtos_prepare_send_list_head);
		
	if( list_empty(p_head) )
			return;
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_prepare_item = (struct prepare_send_list_item *)pTemp;
		
		if( (p_prepare_item->msgid) ==  dst_msgid)
			goto find_on_prepare;
	}
	
	return;
	
	find_on_prepare:
	
		nxmutex_lock(&g_rtos_prepare_send_lock);
	
		list_del(&(p_prepare_item->prepare_send_list));
	
		nxmutex_unlock(&g_rtos_prepare_send_lock);
	
		kmm_free(p_prepare_item);
}	

void handle_create_msg(struct rtonboot_msg_header * p_header, uint8_t * p_content, uint8_t * p_start)
{
	struct rtonboot_msg_header * p_dst_header;
	uint8_t * p_dst_content = NULL;
	struct rtonboot_resp_header resp_header;
	uint8_t * ptr_msg_content;
	uint8_t * ptr_msg_item;
	struct msg_list_item * p_item;
	unsigned long start;
    unsigned long stop;
    int is_rtos_origin;
    
    RtonbootTraceDetail("Enter %s", __func__);
	
	memset(&resp_header, 0, sizeof(struct rtonboot_resp_header));
	
	resp_header.resp_status = RESP_STATUS_SUCCESS;
	
	p_dst_header = (struct rtonboot_msg_header *)p_content;
	
	if( (p_dst_header->max_content_size) )
	{
		p_dst_content = (uint8_t *)pvPortMalloc( p_dst_header->max_content_size );
		
		if(!p_dst_content)
		{
			linux_printf("Winfred Young handle_create_msg malloc p_dst_content fail\n");
			
			resp_header.resp_status = RESP_STATUS_NOMEM;
			
			RtonbootTraceError("in %s malloc p_dst_content fail", __func__);
			
			goto flush_response_and_trigger;
		}
		
		resp_header.offset_content = ((uint64_t)(p_dst_content)) - ((uint64_t)(p_start));
	}
	else
	{
		resp_header.offset_content = 0;
	}	
	
	resp_header.resp_status = handle_create_msg_list(p_start, &resp_header, p_dst_header);
	
	if( ( (resp_header.resp_status) == RESP_STATUS_MSG_DUPLICATE ) && (resp_header.offset_content) )
	{
		if(p_dst_content)
		{
			vPortFree( p_dst_content );
		}	
		
		#ifdef ALLOW_CREATE_DUPLICATE
		
			goto update_final;
		
		#endif
		
		goto flush_response_and_trigger;	
	}	
	else if( (resp_header.resp_status) != RESP_STATUS_SUCCESS )
			goto flush_response_and_trigger;
	
	update_final:
			
	resp_header.dst_msgid = p_dst_header->msgid;
	
	if( (p_dst_header->is_kernel) == 7)
	{
		resp_header.dst_msgid |= 0x80000000;
	}			
	
	#ifdef ALLOW_CREATE_DUPLICATE
		
		if( (resp_header.resp_status) == RESP_STATUS_MSG_DUPLICATE )
		{
			resp_header.resp_status = RESP_STATUS_SUCCESS;
			
			goto flush_response_and_trigger;
		}	
		
	#endif			
	
	ptr_msg_item = p_start + (resp_header.offset_cur_list);	
	p_item = (struct msg_list_item *)(ptr_msg_item);
	p_item->msg_header.offset_cur_listem = resp_header.offset_cur_list; 		
			
	if(	(p_dst_header->msg_origin) == MSG_ORIGIN_RTOS )
	{
		is_rtos_origin = 1; 
	}	 
	else
	{
		is_rtos_origin = 0; 
	}	
	
	call_rtos_create_hook(p_dst_header->msgid, p_header->pid, &resp_header, p_dst_header, p_item, is_rtos_origin);	
	
	flush_response_and_trigger:
	
		ptr_msg_content = p_start + (p_header->offset_content);
		
		memcpy(ptr_msg_content, &resp_header, sizeof(struct rtonboot_resp_header) );
		
		start = ((unsigned long)CONFIG_RAM_START) + (OFFSET_TO_RTONBOOT_SHARE_BUFFER)  + (p_header->offset_content);
				
		stop = start + 64;
		
		flush_dcache_range(start, stop);
		
		flush_dirty_list();

		#ifdef RTONBOOT_DEBUG
		
			linux_printf("Winfred Young RTOS flush_response_and_trigger \n");
			
		#endif	
		
		RtonbootTraceDetail("in %s malloc RTOS flush_response_and_trigger", __func__);
			
		arm64_gic_raise_rtonboot_sgi();
	
		return;
}

uint32_t scan_list_find_this_pid_entry_and_delete(struct rtonboot_msg_header * p_dst_header, uint8_t * p_start)
{
	uint32_t pid;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct root_list_item * p_item;
	struct list_head * p_head;
	struct msg_list_item * p_msg_item;
	uint8_t * p_dst_content = NULL;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if( (p_dst_header->is_broadcast) )
	{
		pid = PID_BROADCAST_MSG;
	}
	else if( (p_dst_header->is_kernel) &&
		((p_dst_header->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		pid = PID_KERNEL_MSG;
	}
	else
	{
		pid = p_dst_header->pid;
	}
	
	if(list_empty(p_root_list_head))
	{
		linux_printf("Winfred Young root list empty cannot delete \n");
		
		RtonbootTraceError("in %s root list empty cannot delete", __func__);
		
		return RESP_STATUS_NO_FOUND;
	}
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;

		if(pid == (p_item->pid) )
		{
			nxmutex_unlock(&g_commu_bottomhalf_lock);
			
			goto found_on_root;
		}	
	}
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);
	
	linux_printf("Winfred Young root list cannot find this pid entry \n");
	
	RtonbootTraceError("in %s root list cannot find this pid entry pid %d", __func__, pid);
	
	return RESP_STATUS_NO_FOUND;
	
	found_on_root:
	
		p_head = &(p_item->msg_list_head);
		
		if( list_empty(p_head) )
			goto no_found_on_msg_list;
		
		nxmutex_lock(&g_commu_bottomhalf_lock);
			
		for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
		{
			pTempBak = pTemp->next;
			
			p_msg_item = (struct msg_list_item *)pTemp;

			if( (p_msg_item->msg_header.msgid) == (p_dst_header->msgid) )
			{
				nxmutex_unlock(&g_commu_bottomhalf_lock);
			
				goto found_on_msg_list;
			}	
		}
		
		nxmutex_unlock(&g_commu_bottomhalf_lock);
		
	no_found_on_msg_list:	
	
		linux_printf("Winfred Young no_found_on_msg_list \n");
	
		return RESP_STATUS_NO_FOUND;
	
	found_on_msg_list:
	
		nxmutex_lock(&g_commu_bottomhalf_lock);
		
		list_del(&(p_msg_item->msg_list));
		
		p_dst_content = p_start + (p_msg_item->msg_header.offset_content);
		
		if(p_dst_content)
		{
			vPortFree( p_dst_content );
		}
		
		vPortFree( p_msg_item );
		
		if( list_empty(p_head) )
		{
			list_del(&(p_item->root_list));
			
			vPortFree( p_item );
		}	
		
		nxmutex_unlock(&g_commu_bottomhalf_lock);
		
		return RESP_STATUS_SUCCESS;
}

void print_no_msg(int is_broadcast, int is_kernel, uint32_t pid)
{
	if(is_broadcast)
	{
		pipe_printf("There are not any broadcast msg \n");
	}
	else if( (is_kernel) && 
		(is_kernel != IS_KERNEL_SCRIPTING_NOTIFY) )
	{
		pipe_printf("There are not any kernel msg \n");
	}
	else
	{
		pipe_printf("There are not any msg for pid %d\n", pid);
	}
}

void print_single_msg(struct rtonboot_msg_header * p_msg_header)
{
	char type_str[5];
	
	if( (p_msg_header->msg_origin) == MSG_ORIGIN_LINUX )
	{
		if( (p_msg_header->msg_has_response) == MSG_HAS_RESPONSE )
		{
			strcpy(&type_str[0], "LR");
		}	
		else
		{
			strcpy(&type_str[0], "LNR");
		}
	}
	else
	{
		if( (p_msg_header->msg_has_response) == MSG_HAS_RESPONSE )
		{
			strcpy(&type_str[0], "RR");
		}	
		else
		{
			strcpy(&type_str[0], "RNR");
		}
	}	
	
	pipe_printf("msg id: %d type: %s max_content_size: %d pid: %d offset_content: %d is_kernel: %d\n", 
		p_msg_header->msgid, &type_str[0], p_msg_header->max_content_size, p_msg_header->pid, p_msg_header->offset_content,
		p_msg_header->is_kernel);
}
	
void scan_list_find_this_pid_entry_and_list(int spid)
{
	uint32_t pid;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct root_list_item * p_item;
	struct list_head * p_head;
	struct msg_list_item * p_msg_item;
	int is_broadcast = 0;
	int is_kernel = 0;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if( (spid) == -1 )
	{
		pid = PID_BROADCAST_MSG;
		
		is_broadcast = 1;
	}
	else if( (spid) == -2 )
	{
		pid = PID_KERNEL_MSG;
		
		is_kernel = 1;
	}
	else
	{
		pid = (uint32_t)spid;
	}
	
	if(list_empty(p_root_list_head))
	{
		pipe_printf("There are not any msg\n");
		
		RtonbootTraceError("in %s There are not any msg", __func__);
		
		return;
	}
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;

		if(pid == (p_item->pid) )
		{
			nxmutex_unlock(&g_commu_bottomhalf_lock);
			
			goto found_on_root;
		}	
	}
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);
	
	print_no_msg(is_broadcast, is_kernel, pid);
	
	return;
	
	found_on_root:
	
		p_head = &(p_item->msg_list_head);
		
		if( list_empty(p_head) )
			goto no_found_on_msg_list;
		
		nxmutex_lock(&g_commu_bottomhalf_lock);
			
		for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
		{
			pTempBak = pTemp->next;
			
			p_msg_item = (struct msg_list_item *)pTemp;

			print_single_msg( &(p_msg_item->msg_header) );	
		}
		
		nxmutex_unlock(&g_commu_bottomhalf_lock);
		
		return;
		
	no_found_on_msg_list:
	
		print_no_msg(is_broadcast, is_kernel, pid);
	
		return;

}	

void handle_destroy_msg(struct rtonboot_msg_header * p_header, uint8_t * p_content, uint8_t * p_start)
{
	struct rtonboot_msg_header * p_dst_header;
	uint32_t status;
    uint32_t * ptemp;
    int is_rtos_origin;
    
    RtonbootTraceDetail("Enter %s", __func__);
	
	p_dst_header = (struct rtonboot_msg_header *)p_content;
	
	status = scan_list_find_this_pid_entry_and_delete(p_dst_header, p_start);
	if(status == RESP_STATUS_SUCCESS)
	{	
		if(	(p_dst_header->msg_origin) == MSG_ORIGIN_RTOS )
		{
			is_rtos_origin = 1; 
		}	
		else
		{
			is_rtos_origin = 0;
		}	
		
		call_rtos_delete_hook(p_dst_header->msgid, p_header->pid, is_rtos_origin);
	}
	flush_dirty_list();
	
	ptemp = (uint32_t *)p_start;
	
	atomic_store_release_32(ptemp, 0);
	
	#ifdef RTONBOOT_DEBUG
	
		linux_printf("Winfred Young RTOS handle_destroy_msg success \n");
		
	#endif	
	
	RtonbootTraceDetail("in %s RTOS handle_destroy_msg success", __func__);
}

void set_is_receive_state(uint32_t pid, uint32_t is_receive)
{
	int32_t spid;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	struct root_list_item * p_item;
	
	
    RtonbootTraceDetail("Enter %s", __func__);
	
	spid = (int32_t)pid;
	
	if(spid < 0)
		return;
		
	if(list_empty(p_root_list_head))
	{
		return;
	}
	
	nxmutex_lock(&g_commu_bottomhalf_lock);
	
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;

		if(pid == (p_item->pid) )
		{
			p_item->is_receive = is_receive;
			
			nxmutex_unlock(&g_commu_bottomhalf_lock);
			
			return;
		}	
	}
	
	nxmutex_unlock(&g_commu_bottomhalf_lock);	
}	

void handle_receive_broadcast_msg(struct rtonboot_msg_header * p_header, uint8_t * p_content, uint8_t * p_start)
{
	struct rtonboot_broadcast_receive_header * p_dst_header;
    uint32_t * ptemp;
      
    RtonbootTraceDetail("Enter %s", __func__);
	
	p_dst_header = (struct rtonboot_broadcast_receive_header *)p_content;
	
	set_is_receive_state(p_header->pid, p_dst_header->is_receive);
	
	flush_dirty_list();
	
	ptemp = (uint32_t *)p_start;
	
	atomic_store_release_32(ptemp, 0);
	
	#ifdef RTONBOOT_DEBUG
	
		linux_printf("Winfred Young RTOS handle_receive_broadcast_msg \n");
		
	#endif
	
	 RtonbootTraceDetail("in %s RTOS handle_receive_broadcast_msg success", __func__);	
}	

void call_resp_callback(uint32_t msgid, uint32_t max_content_size, uint32_t * p_msglen, struct msg_list_item * p_item, uint8_t * p_content)
{
	resp_callack_func_t resp_callack = (resp_callack_func_t)(p_item->resp_callback);
	void * priv = (void *)(p_item->resp_callback_priv);
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if(resp_callack)
	{
		resp_callack(msgid, p_msglen, p_content, max_content_size, priv);
	}
	else
	{
		*p_msglen = max_content_size;
	}		
}

void call_end_callback(uint32_t msgid, uint32_t msglen, struct msg_list_item * p_item, uint8_t * p_content)
{
	end_callack_func_t end_callack = (end_callack_func_t)(p_item->end_callback);
	void * priv = (void *)(p_item->end_callback_priv);
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	if(end_callack)
	{
		end_callack(msgid, msglen, p_content, priv);
	}	
}

void common_resp_callback_post_process(uint32_t msglen, uint8_t * p_content)
{
	uint8_t * p_start;
	uint8_t * ptr_msg_header;
	uint32_t off_cur;
	unsigned long start;
    unsigned long stop;
    struct rtonboot_msg_header * p_header;
    
    RtonbootTraceDetail("Enter %s", __func__);
	
	start = ( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER );
    
	stop = start + 64;
	
	p_start = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER );
	
	ptr_msg_header = p_start + 4;
	
	p_header = (struct rtonboot_msg_header *)(ptr_msg_header);
	
	p_header->msg_len = msglen;
	
	flush_dcache_range(start, stop);
	
	off_cur = (p_content) - (p_start);
			
	start = ( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER + off_cur);
				
	stop = start + (msglen);
				
	flush_dcache_range(start, stop);
	
    #ifdef RTONBOOT_DEBUG
		
			linux_printf("Winfred Young RTOS flush_response_and_trigger \n");
			
	#endif
	
	RtonbootTraceDetail("in %s RTOS flush_response_and_trigger", __func__);	
			
	arm64_gic_raise_rtonboot_sgi();
}

void common_end_callback_pre_process(uint8_t * p_start)
{
    uint32_t * ptemp;
    
    RtonbootTraceDetail("Enter %s", __func__);
    
    ptemp = (uint32_t *)p_start;
	
    atomic_store_release_32(ptemp, 0);	
}

void handle_user_msg(struct rtonboot_msg_header * p_header, uint8_t * p_content, uint8_t * p_start)
{
    uint32_t off_cur;
    struct msg_list_item * p_item;
    uint32_t msglen;
    
    RtonbootTraceDetail("Enter %s", __func__);
    
    off_cur = p_header->offset_cur_listem;
    p_item = (struct msg_list_item *)(p_start + off_cur);
    
    if( ( (p_header->msg_origin == MSG_ORIGIN_LINUX) && (p_header->msg_has_response == MSG_NO_RESPONSE) ) ||
		( (p_header->msg_origin == MSG_ORIGIN_RTOS) && (p_header->msg_has_response == MSG_HAS_RESPONSE) ) )
	{	
		call_end_callback(p_header->msgid, p_header->msg_len, p_item, p_content);
		
		common_end_callback_pre_process(p_start);
		
	}
	else if( (p_header->msg_origin == MSG_ORIGIN_LINUX) && (p_header->msg_has_response == MSG_HAS_RESPONSE) )
	{	
		call_resp_callback(p_header->msgid, p_header->max_content_size, &msglen, p_item, p_content);
		
		common_resp_callback_post_process(msglen, p_content);
	}
}

uint8_t * get_bulk_ptr(uint32_t offset)
{
	uint8_t * p_start;
	
	p_start = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER ); 
	
	p_start += OFFSET_TO_RTONBOOT_BULK_BUFFER;
	
	p_start += offset;
	
	return (p_start);
}

void flush_rtos_bulk_before_send(uint32_t offset, uint32_t len)
{
	unsigned long start;
    unsigned long stop;
	
	start = ( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER );
	
	start += OFFSET_TO_RTONBOOT_BULK_BUFFER;
	
	start += offset;
    
	stop = start + len;
	
	flush_dcache_range(start, stop);
}

extern void create_and_init_ec_kernel_exec_thread(void);

extern void init_rtonboot_trace(void);

static void rtonboot_commu_bottomhalf(void *arg)
{
	uint8_t * p_start;
	uint8_t * p_start_temp;
	uint8_t * p_content;
	struct rtonboot_msg_header * p_header;
	
	p_start = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER ); 
	
	if(is_main_first_kick_received == 1)
	{
			is_main_first_kick_received = 2;
			
			linux_printf("Winfred Young RTOS first kick main core \n");
			
			init_rtonboot_trace();
			
			arm64_gic_raise_rtonboot_sgi();
			
			create_and_init_rtos_send_thread();
			
			create_and_init_ec_kernel_exec_thread();
	}	
	else	
	{
		p_start_temp = p_start + 4;
	
		p_header = (struct rtonboot_msg_header *)p_start_temp;
		
		p_content = p_start + (p_header->offset_content);
		
		if( (p_header->msgid) == MSGID_CREATE_MSG )
		{
			handle_create_msg(p_header, p_content, p_start);
		}
		else if( (p_header->msgid) == MSGID_DESTORY_MSG )
		{
			handle_destroy_msg(p_header, p_content, p_start);
		}
		else if( (p_header->msgid) == MSGID_RECEIVE_BROADCAST_MSG )
		{
			handle_receive_broadcast_msg(p_header, p_content, p_start);
		}
		else
		{
			handle_user_msg(p_header, p_content, p_start);
		}		
	}
}

atomic_int msgid_from_slave;

static void rtonboot_slave_send_bottomhalf(void *arg)
{
	int slave_send_msgid;
	
	slave_send_msgid = atomic_load(&msgid_from_slave);
	
	(void)prepare_send_and_wake_up((uint32_t)slave_send_msgid);
}

int rtonboot_commu_handler(int irq, void *context, void *arg)
{	
	if(!is_main_first_kick_received)
	{
		is_main_first_kick_received = 1;
	}
	
	DEBUGASSERT(work_available(&rtonboot_commu_bhalf));

    DEBUGVERIFY(work_queue(HPWORK, &rtonboot_commu_bhalf,
       rtonboot_commu_bottomhalf,
      (void *)0, 0));
	
	return OK;
}

int rtonboot_slave_handler(int irq, void *context, void *arg)
{	
	DEBUGASSERT(work_available(&rtonboot_slave_send_bhalf));
	
	DEBUGVERIFY(work_queue(HPWORK, &rtonboot_slave_send_bhalf,
       rtonboot_slave_send_bottomhalf,
      (void *)0, 0));
	
	return OK;
}

int up_send_from_slave(void)
{
	return OK;
}

#define RTOS_SEND_TASK_PRIORITY 146
#define RTOS_SEND_TASK_STACK_SIZE 8192

#define RTOS_SEND_TASK_NAME "rtos_send"

int pid_rtos_send_task = -1;

sem_t g_rtos_send_sig;

spinlock_t rtos_send_lock;

struct list_head rtos_send_list_head;

struct prepare_send_list_item * find_prepare_item(uint32_t msgid)
{
	struct prepare_send_list_item * p_prepare_item;
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_head = &(rtos_prepare_send_list_head);
		
	if( list_empty(p_head) )
			return NULL;
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		p_prepare_item = (struct prepare_send_list_item *)pTemp;
		
		if( (p_prepare_item->msgid) ==  msgid)
		{
			return p_prepare_item;
		}	
	}
	
	return NULL;
}

int prepare_send_and_wake_up(uint32_t msgid)
{
	struct prepare_send_list_item * p_prepare_item;
	struct rtos_send_list_item * p_item;
	uint8_t * p_start;
	uint8_t * ptr_msg_header;
	struct msg_list_item * p_msg_item;
	irqstate_t flags;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_prepare_item = find_prepare_item(msgid);
	if(!p_prepare_item)
	{
		linux_printf("Winfred Young ERROR: cannot find_prepare_item\n");
		
		RtonbootTraceError("in %s cannot find_prepare_item", __func__);
		
		return -1;
	}
	
	if( (!(p_prepare_item->offset_content)) || (!(p_prepare_item->offset_cur_list)) )
	{
		linux_printf("Winfred Young ERROR: prepare_item offset is zero\n");
		
		RtonbootTraceError("in %s prepare_item offset is zero", __func__);
		
		return -1;
	}
	
	p_item = (struct rtos_send_list_item *)kmm_malloc(sizeof(struct rtos_send_list_item));
	if(!p_item)
	{
		linux_printf("Winfred Young ERROR: cannot kmm_malloc rtos_send_list_item\n");
		
		RtonbootTraceError("in %s cannot kmm_malloc rtos_send_list_item", __func__);
		
		return -1;
	}
	
	memset(p_item, 0, sizeof(struct rtos_send_list_item));
	
	p_start = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER ); 
	
	ptr_msg_header = p_start + (p_prepare_item->offset_cur_list); 
	
	p_msg_item = (struct msg_list_item *)ptr_msg_header;
	
	p_item->p_msg_header = &(p_msg_item->msg_header);
	
	p_item->p_content = p_start + (p_prepare_item->offset_content);
	
	if((p_prepare_item->prepare_callback) )
	{
		(*(p_prepare_item->prepare_callback))(msgid, &(p_item->p_msg_header->msg_len),
			p_item->p_content, p_msg_item->msg_header.max_content_size, p_prepare_item->prepare_priv);
	}
	
	flags = spin_lock_irqsave(&rtos_send_lock);
	
	list_add_tail(&(p_item->rtos_send_list), &(rtos_send_list_head));

	spin_unlock_irqrestore(&rtos_send_lock, flags);
	
	nxsem_post(&g_rtos_send_sig);
	
	return 0;
}


void prepare_send_result_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_SEND_EXECCMD_RESULT);
}

void prepare_testrr_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_TESTRR);
}

void prepare_testbc_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_TESTBC);
}

#define RTOS_SEND_WAIT_MS 100

int rtos_send_thread(int argc, char *argv[])
{
	/*int ret;*/
	irqstate_t flags;
	uint8_t * p_start;
	uint32_t * ptemp;
	uint32_t msg_pending_status;
	int is_empty;
	struct list_head * pTemp;
	struct rtos_send_list_item * p_item;
	uint8_t * ptr_msg_header;
	uint8_t * ptr_msg_content;
	unsigned long start;
    unsigned long stop;
    struct timespec delay;
    uint64_t timeout;
    struct timespec now;
    struct timespec rqtp;
	
	p_start = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER ); 
	
	ptemp = (uint32_t *)(p_start);
	
	while (1)
	{
		/*ret = nxsem_wait(&g_rtos_send_sig);
		if (ret)
		{
			linux_printf("Winfred Young ERROR: Wait g_rtos_send_sig error=%d\n", ret);
			
			RtonbootTraceError("in %s Wait g_rtos_send_sig error=%d", __func__, ret);
		}*/
		
		clock_gettime(CLOCK_REALTIME, &now);
		
		timeout = RTOS_SEND_WAIT_MS;

		delay.tv_sec  = timeout / 1000;
		delay.tv_nsec = (timeout % 1000) * 1000000;
      
		clock_timespec_add(&now, &delay, &rqtp); 
	
		nxsem_timedwait(&g_rtos_send_sig, &rqtp);

		flags = spin_lock_irqsave(&rtos_send_lock);
		
		msg_pending_status = atomic_load_acquire_32(ptemp);
				 
		is_empty = list_empty(&(rtos_send_list_head));
		if( (!(is_empty)) && (!(msg_pending_status)) )
		{
			pTemp = ((&(rtos_send_list_head))->next);	
					
			p_item = (struct rtos_send_list_item *)pTemp;
		}

		spin_unlock_irqrestore(&rtos_send_lock, flags);
		
		if( (!(is_empty)) && (!(msg_pending_status)) )
					goto work_to_do;
					
		continue;			
					
		work_to_do:
		
		    start = ( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER );
    
			stop = start + 64;
						
			msg_pending_status = atomic_load_acquire_32(ptemp);
			
			if(msg_pending_status)
				goto add_back_and_wait;
		
			flags = spin_lock_irqsave(&rtos_send_lock);
			
			atomic_store_release_32(ptemp, 1);	
						
			list_del(&(p_item->rtos_send_list));
			
			ptr_msg_header = p_start + 4;
			
			memcpy(ptr_msg_header, (p_item->p_msg_header), sizeof(struct rtonboot_msg_header) );
			
			flush_dcache_range(start, stop);
			
			ptr_msg_content = p_start + (p_item->p_msg_header->offset_content);
			
			memcpy(ptr_msg_content, (p_item->p_content), p_item->p_msg_header->msg_len );
			
			start = ( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER + 
				p_item->p_msg_header->offset_content);
				
			stop = start + (p_item->p_msg_header->msg_len);
				
			flush_dcache_range(start, stop);
			
			spin_unlock_irqrestore(&rtos_send_lock, flags);
			
			#ifdef RTONBOOT_DEBUG
			
				linux_printf("Winfred Young arm64_gic_raise_rtonboot_sgi\n");
				
			#endif	
			
			RtonbootTraceDetail("in %s arm64_gic_raise_rtonboot_sgi", __func__);
			
			arm64_gic_raise_rtonboot_sgi();
			
			kmm_free(p_item);
			
			continue;
			
		add_back_and_wait:
			
    };

	return 0;
}

void create_and_init_rtos_send_thread(void)
{
	#if defined (CONFIG_SMP)
	
	cpu_set_t cpuset;
	int ret;
	
	#endif
	
	INIT_LIST_HEAD(&rtos_send_list_head);
	
	INIT_LIST_HEAD(&rtos_prepare_send_list_head);
	
	spin_lock_init(&rtos_send_lock);
	
	nxsem_init(&g_rtos_send_sig, 0, 0);
	
	pid_rtos_send_task = kthread_create(RTOS_SEND_TASK_NAME,
                       RTOS_SEND_TASK_PRIORITY,
                       RTOS_SEND_TASK_STACK_SIZE,
                       rtos_send_thread,
                       NULL);
    if (pid_rtos_send_task < 0)
    {
		linux_printf("Winfred Young ERROR: Failed to create RTOS Send Task error=%d\n", pid_rtos_send_task);
		
		return;
    }

    #if defined (CONFIG_SMP)
        
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    
    ret = nxsched_set_affinity(pid_rtos_send_task, sizeof(cpuset), &cpuset);
    if (ret < 0)
    {
        linux_printf("Winfred Young ERROR: Failed to set RTOS Send Task affinity ret=%d\n", ret);
    }
    
    #endif
}	

extern void prvHeapInit( void );

void board_early_initialize(void)
{
	prvHeapInit();
}



