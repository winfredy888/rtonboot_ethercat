#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mm.h>

#include <linux/uaccess.h>
#include <linux/bug.h>

#include <linux/sched.h>

#if defined (CONFIG_RTONBOOT_SUPPORT)

#include "rtonboot.h"

#include <linux/completion.h>

#include "../../drivers/rtonboot/rtbridge.h"

#endif

#include <linux/vmalloc.h>
#include <linux/timer.h>

extern void RTOnBootTraceDetail(char * fmt, ...);
	
extern void RTOnBootTraceInfo(char * fmt, ...);	

extern void RTOnBootTraceWarn(char * fmt, ...);	

extern void RTOnBootTraceError(char * fmt, ...);	

extern void scan_pollpara_list_set_pollpara(uint32_t pid, uint32_t val);

extern void kmalloc_k7_msgitem_and_add(uint32_t msgid, uint32_t offset_cur_list);

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void rtonboot_flush_dcache_range(unsigned long start, unsigned long stop)
{
	__asm_flush_dcache_range(start, stop);
}

#if defined (CONFIG_RTONBOOT_SUPPORT)

char * g_ptr_rtonboot = NULL;

extern int rtonboot_ioremap(uint64_t addr, phys_addr_t phys_addr, size_t size);

extern void gic_raise_softirq(const struct cpumask *mask, unsigned int irq);

struct timer_list g_async_timer;

int is_async_timer_start = 0;

extern struct rtbridge_dev * rtbridge_devp;

extern void internal_set_pending_status(uint32_t * p_pending, uint32_t status);

extern void set_pending_status(uint32_t status);

extern void bc_set_pending_status(void);

char * get_ptr_rtonboot(void)
{
	int err;
	uint64_t temp;

	if(!(g_ptr_rtonboot))
	{
		/*temp = (uint64_t)VMALLOC_END - CONFIG_RTONBOOT_RESERVE_MEMORY_SIZE;

		temp &= ~(0x200000 - 1);*/
		
		/*temp = 0xffffffc000000000ULL;*/
		
		temp = CONFIG_IMAGE_VADDR_BASE;

		printk("Winfred rtonboot Image Virtual base is %llx \n", (uint64_t)temp);

		err = rtonboot_ioremap(temp, CONFIG_RTONBOOT_LOAD_ADDR, CONFIG_RTONBOOT_RUNBUF_SIZE);
		if(err)
		{
			printk("Winfred rtonboot_ioremap err is %d Fatal error\n", err);

			return NULL;
		}

		g_ptr_rtonboot = (char *)(temp);
	}

	return g_ptr_rtonboot;
}

void set_rtonboot_phase(uint64_t cur_phase)
{
	char * ptr_rtonboot;
	uint64_t * p_phase;
	uint64_t * p_pending;

	ptr_rtonboot = get_ptr_rtonboot();
	if(!(ptr_rtonboot))
	{
		printk("Winfred rtonboot set phase fatal error NULL ptr_rtonboot \n");
		
		return;
	}
	
	p_phase = (uint64_t *)(&(ptr_rtonboot[0xc00000]));
	
	(*(p_phase)) = 1;
	
	printk("Winfred rtonboot check remap is %llx \n", (*(p_phase)));

	p_phase = (uint64_t *)(&(ptr_rtonboot[RTONBOOT_PHASE_OFFSET]));
	
	(*(p_phase)) = cur_phase;

	printk("Winfred rtonboot set phase is %llx \n", (*(p_phase)));
	
	p_pending = (uint64_t *)(&(ptr_rtonboot[OFFSET_TO_RTONBOOT_SHARE_BUFFER]));
		
	atomic_store_release_64(p_pending, 0);
	
	printk("Winfred rtonboot set pending is %llx \n", (*(p_pending)));
}

void async_timer_function(struct timer_list *t)
{
	char * ptr_rtonboot;
    uint32_t * ptemp;
    
    printk("Winfred Young enter into async_timer_function\n");
    
    RTOnBootTraceDetail("Enter %s", __func__);
    
	del_timer(&g_async_timer);
	
	is_async_timer_start = 0;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
    ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
    ptemp = (uint32_t *)(ptr_rtonboot);
	
	internal_set_pending_status(ptemp, 0);
	
	wake_up(&(rtbridge_devp->send_queue));
}

void start_async_timer(uint32_t ms)
{
	RTOnBootTraceDetail("Enter %s", __func__);
	
	if(!is_async_timer_start)
	{
		timer_setup(&g_async_timer, async_timer_function, 0);
	
		mod_timer(&g_async_timer, jiffies + msecs_to_jiffies(ms));
	
		is_async_timer_start = 1;
	}		
}

/*int is_process_alive(pid_t pid)
{
    struct task_struct *task;
    int is_alive;
    
    rcu_read_lock();
    
    is_alive = 0; 
    
    task = find_task_by_vpid(pid);
    if (!task) {
        goto out;
    }
    
    if (task->state == TASK_DEAD || task->exit_state == EXIT_ZOMBIE) {
        is_alive = 0;
    } else {
		is_alive = 1;
    }
    
out:
    rcu_read_unlock();
    
    return is_alive;
}*/

int is_process_alive(pid_t target_pid) {
    struct pid *pid_struct;
    int ret;

    pid_struct = find_get_pid(target_pid);
    if (!pid_struct) {
        return 0;
    }

    ret = kill_pid_info(0, NULL, pid_struct);

    put_pid(pid_struct);

    if(!ret)
    {
		return 1;
	}	
	
    return 0;
}

#endif

/*Winfred Young add for rtonboot*/

extern int rtbridge_ever_open;

extern uint32_t offset_root_list_head;

static struct tasklet_struct rtonboot_ipi_tasklet;

uint32_t count_cur_broadcast = 0;

uint32_t count_broadcast_received = 0;

struct list_head * get_root_list_head_ptr(void)
{
	struct list_head * p_root_list_head;
	char * ptr_rtonboot;
	
	RTOnBootTraceDetail("Enter %s offset_root_list_head is %x", __func__, offset_root_list_head);
	
	if(!(offset_root_list_head))
		return NULL;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
	
	ptr_rtonboot += offset_root_list_head;
	
	p_root_list_head = (struct list_head *)(ptr_rtonboot);
	
	return p_root_list_head;
}	

uint32_t scan_root_list_get_receive_count(void)
{
	struct list_head * p_root_list_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	int32_t spid;
	uint32_t count;
	struct root_list_item * p_item;
	int is_alive;
	
	RTOnBootTraceDetail("Enter %s", __func__);
	
	p_root_list_head = get_root_list_head_ptr();
	if(!p_root_list_head)
		return 0;
	
	if(list_empty(p_root_list_head))
		return 0;
		
	count = 0;	
		
	for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;
		
		spid = (int32_t)(p_item->pid);
		
		if(spid < 0)
			continue;
			
		if(!(p_item->is_receive))
			continue;
			
		if(list_empty(&(p_item->msg_list_head)))
			continue;
			
		is_alive = is_process_alive(spid);		
		
		if(is_alive)
		{
			++count;	
		}	
	}	
	
	return count;
}

void rtonboot_ipi_tasklet_handler(unsigned long data)
{
	uint32_t ms;
	struct list_head * p_root_list_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	int32_t spid;
	struct root_list_item * p_item;
	struct msg_list_item * p_msg_item;
	struct fasync_struct * async_queue;
	int is_alive;
	int tempcnt;
	
	count_cur_broadcast = scan_root_list_get_receive_count();
	
	ms = (count_cur_broadcast * ASYNC_TIMER_EXPIRE_MS) / 2;
	
	p_root_list_head = get_root_list_head_ptr();
	if(!p_root_list_head)
		return;
		
	if(list_empty(p_root_list_head))
		return;
		
	count_broadcast_received = 0;	
	
	if(count_cur_broadcast)
	{
		start_async_timer(ms);
	}	
	
	tempcnt = 0;
	
    for(pTemp = p_root_list_head->next; pTemp != p_root_list_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;

		p_item = (struct root_list_item *)pTemp;
		
		spid = (int32_t)(p_item->pid);
		
		if(spid < 0)
			continue;
			
		if(!(p_item->is_receive))
			continue;
			
		if(list_empty(&(p_item->msg_list_head)))
			continue;
			
		is_alive = is_process_alive(spid);	
		
		if(is_alive)
		{
			p_msg_item = (struct msg_list_item *)(p_item->msg_list_head.next);
			
			if( (p_msg_item->msg_header.is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY)
			{
				if(async_queue)
				{
					async_queue = (struct fasync_struct *)(p_msg_item->msg_header.async_or_callback);
		
					kill_fasync(&async_queue, SIGIO, POLL_IN);
					
					++tempcnt;
				}	
			}
			else
			{
				scan_pollpara_list_set_pollpara(p_msg_item->msg_header.pid, p_msg_item->msg_header.msgid);	
				
				++tempcnt;
			}	
		}	
	}
	
	if(!tempcnt)
	{
		bc_set_pending_status();
	}	
}

typedef void (*kcallack_func_t)(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv);

void call_user_defined_kcallback(struct rtonboot_msg_header * p_msg_header)
{
	kcallack_func_t kcallback = (kcallack_func_t)(p_msg_header->async_or_callback);
	void * priv = (void *)(p_msg_header->callback_priv);
	uint32_t offset_content = (p_msg_header->offset_content);
	char * ptr_rtonboot;
	uint32_t * ptemp;
	
	RTOnBootTraceDetail("Enter %s, kcallback %x", __func__, kcallback);
	
	if(!(kcallback))
		return;
	
	ptr_rtonboot = get_ptr_rtonboot(); 
     
	ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
	
	ptemp = (uint32_t *)(ptr_rtonboot);
				
	ptr_rtonboot += offset_content;	
		
	kcallback(p_msg_header->msgid, p_msg_header->msg_len, (uint8_t *)(ptr_rtonboot), priv);
	
	internal_set_pending_status(ptemp, 0);
}

#define RESP_STATUS_SUCCESS 0
#define RESP_STATUS_NOMEM 1
#define RESP_STATUS_MSG_DUPLICATE 2
#define RESP_STATUS_NO_FOUND 3

int rtonboot_ipi_on_main_core(void)
{
	#if defined (CONFIG_RTONBOOT_SUPPORT)
	
		char * ptr_rtonboot;
		char * ptr_msg_header;
		char * ptr_resp_header;
		struct rtonboot_msg_header * p_msg_header;
		struct rtonboot_resp_header * p_resp_header;
		struct fasync_struct * async_queue;
		uint32_t pending_status;
		uint32_t * ptemp;
		uint32_t tmp_msgid;
		int create_skip_timer;
		int is_alive;
		uint32_t ms;
				
		if(!rtbridge_ever_open)
		{
			tasklet_init(&rtonboot_ipi_tasklet, rtonboot_ipi_tasklet_handler, 0);
			 
			printk("Winfred Young got first kick from RTOS core\n");
		}	
		else
		{	
			count_cur_broadcast = 0;
			
			ptr_rtonboot = get_ptr_rtonboot(); 
     
			ptr_rtonboot += OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
			
			ptemp = (uint32_t *)(ptr_rtonboot);
			
			pending_status = atomic_load_acquire_32(ptemp);
			
			/*pending_status = (*(ptemp));*/
			
			if(!pending_status)
				return 0;	
				
			ms = ASYNC_TIMER_EXPIRE_MS;	
				
			create_skip_timer = 0;	
			
			ptr_msg_header = ptr_rtonboot + 4;
			
			p_msg_header = (struct rtonboot_msg_header *)ptr_msg_header;
			
			if( (p_msg_header->msgid) == MSGID_CREATE_MSG)
			{			
				ptr_resp_header = ptr_rtonboot + (p_msg_header->offset_content);
			
				p_resp_header = (struct rtonboot_resp_header *)ptr_resp_header;
				
				offset_root_list_head = p_resp_header->offset_root_list_head;	
				
				scan_pollpara_list_set_pollpara(p_msg_header->pid, p_msg_header->msgid);	
				
				if(	(p_resp_header->resp_status) == RESP_STATUS_SUCCESS)
				{
					if( ( (p_resp_header->dst_msgid) & 0x80000000) )
					{
						tmp_msgid = p_resp_header->dst_msgid;
						
						tmp_msgid &= ~0x80000000;
						
						kmalloc_k7_msgitem_and_add(tmp_msgid, p_resp_header->offset_cur_list);
					}	
				}	
				
				create_skip_timer = 0;	
				
				ms = CREATE_MSG_TIMEOUT_MS;	
			}
			else if( (!(p_msg_header->is_broadcast)) && ((p_msg_header->is_kernel) == IS_KERNEL_SCRIPTING_NOTIFY) )
			{
				if(  ( (p_msg_header->msg_origin) == MSG_ORIGIN_RTOS ) ||
					( ((p_msg_header->msg_origin) == MSG_ORIGIN_LINUX) && ((p_msg_header->msg_has_response) == MSG_HAS_RESPONSE) ) )
				{					
					scan_pollpara_list_set_pollpara(p_msg_header->pid, p_msg_header->msgid);	
				}	
			}	
			
			if( (p_msg_header->is_broadcast) )
			{
				tasklet_schedule(&rtonboot_ipi_tasklet);
			}
			else if( (!(p_msg_header->is_kernel)) ||
				((p_msg_header->is_kernel) == IS_KERNEL_SCRIPTING_NOTIFY) )
			{
				if(  ( (p_msg_header->msg_origin) == MSG_ORIGIN_RTOS ) ||
					( ((p_msg_header->msg_origin) == MSG_ORIGIN_LINUX) && ((p_msg_header->msg_has_response) == MSG_HAS_RESPONSE) ) )
				{	
					if( (p_msg_header->async_or_callback) && 
						((p_msg_header->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
					{
						is_alive = is_process_alive(p_msg_header->pid);
						
						if( (!create_skip_timer) && (is_alive) )
						{
							start_async_timer(ms);
						}	
						
						if(is_alive)
						{
							async_queue = (struct fasync_struct *)(p_msg_header->async_or_callback);
						
							kill_fasync(&async_queue, SIGIO, POLL_IN);
						}
						else
						{
							set_pending_status(0);
						}		
					}
					else if( (p_msg_header->is_kernel) == IS_KERNEL_SCRIPTING_NOTIFY )
					{
						start_async_timer(ms);
					}	 
				}		
			}
			else if( (p_msg_header->is_kernel) && 
				((p_msg_header->is_kernel) != IS_KERNEL_SCRIPTING_NOTIFY) )
			{
				call_user_defined_kcallback(p_msg_header);
			}
		}	

		return 1;

	#else

		return 0;

	#endif
}

#  define GIC_SMP_SCHED             1
#  define GIC_SMP_CPUSTART          2
#  define GIC_SMP_CALL              3

int is_print_sgi_val = 0;

static int rtonboot_linux_task_proc(void *arg)
{
	cpumask_t both_mask;
	
	printk("Winfred Young nter rtonboot_linux_task_proc\n");

	set_current_state( TASK_INTERRUPTIBLE );
	
	cpumask_or(&both_mask, cpumask_of(1), cpumask_of(3));
	
	while(1)
	{
		if(kthread_should_stop())
			goto return_from_loop;
		
		schedule_timeout(msecs_to_jiffies(1000));
		
		is_print_sgi_val = 1;
		
		gic_raise_softirq(cpumask_of(1), GIC_SMP_SCHED);
		
		is_print_sgi_val = 0;
		
		schedule_timeout(msecs_to_jiffies(1000));
		
		is_print_sgi_val = 1;
		
		gic_raise_softirq(&both_mask, GIC_SMP_SCHED);
		
		is_print_sgi_val = 0;
		
		schedule_timeout(msecs_to_jiffies(1000));
		
		is_print_sgi_val = 1;
		
		gic_raise_softirq(cpumask_of(1), GIC_SMP_CALL);
		
		is_print_sgi_val = 0;
		
		schedule_timeout(msecs_to_jiffies(1000));
		
		is_print_sgi_val = 1;
		
		gic_raise_softirq(&both_mask, GIC_SMP_CALL);
		
		is_print_sgi_val = 0;

		break;
	};	
		
	return_from_loop:	

		return 0;
}

void create_and_start_linux_boot_thread(void)
{
	struct task_struct * tsk = NULL;

    printk("Winfred Young RTONBOOT before create linux_boot_thread\n");

	tsk = kthread_create(rtonboot_linux_task_proc, NULL, "linux_boot_thread");
	if (IS_ERR(tsk)) {

		printk("Winfred Young rtonboot error on kthread_create_on_cpu %llx\n", (long long unsigned int)(PTR_ERR(tsk)));

		return;
	}

	printk("Winfred Young RTONBOOT before bind linux_boot_thread\n");

	kthread_bind(tsk, 3);

	wake_up_process(tsk);
}

static int __init xenomai_init(void)
{
	#if defined (CONFIG_RTONBOOT_SUPPORT)
		
		set_rtonboot_phase(1);
		
		/*create_and_start_linux_boot_thread();*/
		
		printk("Winfred Young first kick RTONBOOT_CPU.\n");
		
		rtonboot_gic_unmask_rtonboot_ipi();
		
		gic_raise_softirq(cpumask_of(CONFIG_RTONBOOT_CPUID), RTONBOOT_SGI_ID);
		
	#endif
	
	return 0;
}

device_initcall(xenomai_init);

