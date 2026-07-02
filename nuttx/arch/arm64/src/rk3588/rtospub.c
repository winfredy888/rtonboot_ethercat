#include <nuttx/config.h>
#include <nuttx/kthread.h>
#include <nuttx/signal.h>
#include <nuttx/kmalloc.h>

#include <stdatomic.h>

#include "rtospub.h"

#include "rtonboot_trace.h"

#include "rt_common_internal.h"


#define ADSB3 1

#undef ADSA2

extern int linux_printf(const char *fmt, ...);

extern uint8_t * get_bulk_ptr(uint32_t offset);

extern void flush_rtos_bulk_before_send(uint32_t offset, uint32_t len);

struct rtonboot_bulkpara g_bulkpara;

int pid_ec_kernel_exec_task = -1;

sem_t g_ec_kernel_exec_sig;

struct rtonboot_ec_allpapa g_allpara;

extern int ec_cyclic_main(int argc, char **argv);

extern int ec_inter_main(int argc, char **argv);

extern int simpletest_main(int argc, char **argv);

extern int motorctl_main(int argc, char **argv);

extern int motorpp_main(int argc, char **argv);

int taskid_ec_cyclic = -1;

int taskid_ec_inter = -1;

int taskid_simpletest = -1;

int taskid_motorctl = -1;

int taskid_motorpp = -1;

sem_t g_sem_ec_cyclic_task = SEM_INITIALIZER(0);

sem_t g_sem_ec_inter_task = SEM_INITIALIZER(0);

sem_t g_sem_simpletest = SEM_INITIALIZER(0);

sem_t g_sem_motorctl = SEM_INITIALIZER(0);

sem_t g_sem_motorpp = SEM_INITIALIZER(0);

int is_cyclic_quit = 0;

int g_ec_inter_stop_run = 0;

int g_simpletest_quit = 0;

int g_motorctl_stop_run = 0;

int g_motorpp_stop_run = 0;

#if defined (CONFIG_ETHERCAT_SOEM_MOTORCTL)

extern int g_motorctl_target_step;

#endif

#if defined (CONFIG_ETHERCAT_SOEM_MOTORPP)

extern bool target_position_updated;

extern volatile int32_t target_position;

#endif

#if defined (CONFIG_ETHERCAT_SOEM_SIMPLETEST)

pthread_key_t g_pthread_key;

int g_soem_thread_keycount = 1;

int g_soem_thread_keycount_saved = 1;

#endif

uint8_t g_rteth_buf[SINGLE_RTETH_BUFFER_SIZE];
uint8_t g_rteth_pktlen;
int ec_cyclic_task_routine(int argc, char *argv[])
{
	#if 0
	
	is_cyclic_quit = 0;
	
	ec_cyclic_main(0, NULL);
    
    while(1)
    {
		sem_wait(&g_sem_ec_cyclic_task);
		
		is_cyclic_quit = 0;
		
		ec_cyclic_main(0, NULL);
	};
	
	#endif	
    
	return 0;
}	

int ec_inter_task_routine(int argc, char *argv[])
{
	#if 0
	
	g_ec_inter_stop_run = 0;
	
	ec_inter_main(0, NULL);
    
    while(1)
    {
		sem_wait(&g_sem_ec_inter_task);
		
		g_ec_inter_stop_run = 0;
		
		ec_inter_main(0, NULL);
	};
	
	#endif	
    
	return 0;
}

void set_thread_key(void)
{
	int ret;
	
	if(g_soem_thread_keycount == 1)
	{
		ret = pthread_setspecific(g_pthread_key, (FAR const void *)0x88888888);
        if (ret != 0)
        {
			linux_printf("ERROR: pthread_setspecific() failed: %d\n", ret);
			
            DEBUGASSERT(false);
        }
    }
    else if(g_soem_thread_keycount == 2)
    {
		ret = pthread_setspecific(g_pthread_key, (FAR const void *)0x99999999);
        if (ret != 0)
        {
			linux_printf("ERROR: pthread_setspecific() failed: %d\n", ret);
			
            DEBUGASSERT(false);
        }
    }
    else
    {
		ret = pthread_setspecific(g_pthread_key, (FAR const void *)0xaaaaaaaa);
        if (ret != 0)
        {
			linux_printf("ERROR: pthread_setspecific() failed: %d\n", ret);
			
            DEBUGASSERT(false);
        }
    }			
    
    ++g_soem_thread_keycount;
}	

/*int simpletest_task_routine(int argc, char *argv[])*/
void * simpletest_task_routine(FAR void *parameter)
{
    set_thread_key();
   
	g_simpletest_quit = 0;
	
	simpletest_main(0, NULL);
    
    while(1)
    {
		sem_wait(&g_sem_simpletest);
		
		g_simpletest_quit = 0;
		
		simpletest_main(0, NULL);
	};	
    
	return 0;
}

void * motorctl_task_routine(FAR void *parameter)
{
    set_thread_key();
   
	g_motorctl_stop_run = 0;
	
	#if defined (ADSB3)
	
		g_motorctl_target_step = 100000;
		
	#elif defined (ADSA2)
	
		g_motorctl_target_step = 1000;
		
	#else
	
		#error "Unsopport Servo type"
	
	#endif	
	
	motorctl_main(0, NULL);
    
    while(1)
    {
		sem_wait(&g_sem_motorctl);
		
		g_motorctl_stop_run = 0;
		
		g_motorctl_target_step = 100;
		
		/*motorctl_main(0, NULL);*/
	};	
    
	return 0;
}

void * motorpp_task_routine(FAR void *parameter)
{
    set_thread_key();
   
	g_motorpp_stop_run = 0;
	
	motorpp_main(0, NULL);
    
    while(1)
    {
		sem_wait(&g_sem_motorpp);
		
		g_motorpp_stop_run = 0;
		
		motorpp_main(0, NULL);
	};	
    
	return 0;
}


void resp_callack_for_start_ec(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_ec_allpapa * p_allrara = (struct rtonboot_ec_allpapa *)(p_content);
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	*p_msglen = sizeof(struct rtonboot_ec_allpapa);
	
	if( ((p_allrara->net_port_idx) < 0) || (p_allrara->is_null) )
	{
		linux_printf("resp_callack_for_start_ec check invalid do not start ec \n");
		
		return;
	}	 
	
	memcpy(&g_allpara, p_allrara, sizeof(struct rtonboot_ec_allpapa));
	 
	nxsem_post(&g_ec_kernel_exec_sig);
}

uint16_t g_is_link_up = 0;

void resp_callack_for_ec_master_ready(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_ec_ready_status * p_ready_status = (struct rtonboot_ec_ready_status *)(p_content);
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	*p_msglen = sizeof(struct rtonboot_ec_ready_status);
	
	p_ready_status->is_ready = g_is_link_up;
	p_ready_status->status = 0;
}

void resp_callack_for_ec_inter_master_ready(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_ec_ready_status * p_ready_status = (struct rtonboot_ec_ready_status *)(p_content);
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	*p_msglen = sizeof(struct rtonboot_ec_ready_status);
	
	p_ready_status->is_ready = 1; /*g_is_link_up;*/
	p_ready_status->status = 0;
}

void resp_callack_for_rteth_send(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	uint32_t pkt_len;
	
	pkt_len = *((uint32_t *)p_content);
	
	linux_printf("resp_callack_for_rteth_send pkt_len is %d \n", pkt_len);
	
	g_rteth_pktlen = pkt_len;
	
	memcpy(&g_rteth_buf[0], p_content, g_rteth_pktlen);
	
	*((uint32_t *)p_content) = 0;
	
	*p_msglen = 4;
	
	prepare_send_and_wake_up(MSGID_RTETH_RECV);
}
void rtospub_set_resp_callback(uint32_t dst_msgid, uint64_t * p_resp_cb, uint64_t * p_resp_cb_priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	switch (dst_msgid)
	{
		case MSGID_START_EC:
		
			*p_resp_cb = (uint64_t)(&resp_callack_for_start_ec);
		
			*p_resp_cb_priv = 0;
			
			break;
			
		case MSGID_EC_MASTER_READY:
		
			*p_resp_cb = (uint64_t)(&resp_callack_for_ec_master_ready);
		
			*p_resp_cb_priv = 0;
			
			break;
			
		case MSGID_EC_INTER_GET_MASTER_READY:
		
			*p_resp_cb = (uint64_t)(&resp_callack_for_ec_inter_master_ready);
		
			*p_resp_cb_priv = 0;
			
			break;	
			
		case MSGID_RTETH_SEND:
		
			*p_resp_cb = (uint64_t)(&resp_callack_for_rteth_send);
		
			*p_resp_cb_priv = 0;
			
			break;				
	}	
}

extern void prepare_send_result_and_wake_up(void);

void linux_bulk_ready_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_bulkpara * p_bulkpara;
	uint8_t * p_bulk;
	uint32_t * p_temp;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_bulkpara = (struct rtonboot_bulkpara *)(p_content);
	
	linux_printf("linux_bulk_ready_end_callack offset is %x, len is %x \n", p_bulkpara->offset, p_bulkpara->len);
	
	p_bulk = get_bulk_ptr(p_bulkpara->offset);
	
	p_temp = (uint32_t *)(p_bulk);
	
	linux_printf("linux_bulk_ready_end_callack bulk head 32 %x \n", *(p_temp) );
	
	prepare_send_result_and_wake_up();
}

void start_ec_cyclic_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	#if 0
	
	struct rtonboot_start_ec_cyclic * p_start_ec_cyclic;
	
	#if defined (CONFIG_SMP)
	
	cpu_set_t cpuset;
	int ret;
	
	#endif
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_start_ec_cyclic = (struct rtonboot_start_ec_cyclic *)(p_content);
	
	if(p_start_ec_cyclic->is_start)
	{
		linux_printf("start_ec_cyclic_end_callack start ec cyclic \n");
		
		if(taskid_ec_cyclic < 0)
		{
			  taskid_ec_cyclic = task_create("ec_cyclic_task", CONFIG_ETHERCAT_EC_CYCLIC_PRIORITY,
                       CONFIG_ETHERCAT_EC_CYCLIC_STACKSIZE, ec_cyclic_task_routine,
                       NULL);
              if(taskid_ec_cyclic < 0)
              {
				  linux_printf("Winfred Young create ec_cyclic_task fail\n");
			  }
			  
			  #if defined (CONFIG_SMP)
			  
			  CPU_ZERO(&cpuset);
			  CPU_SET(0, &cpuset);
    
			  ret = nxsched_set_affinity(taskid_ec_cyclic, sizeof(cpuset), &cpuset);
			  if (ret < 0)
			  {
					linux_printf("Winfred Young ERROR: Failed to set ec_cyclic Task affinity ret=%d\n", ret);
			  }
			  
			  #endif	         
	    }
	    else
	    {
			  sem_post(&g_sem_ec_cyclic_task);
		}
	}
	
	#endif
}

void stop_ec_cyclic_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_stop_ec_cyclic * p_stop_ec_cyclic;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_stop_ec_cyclic = (struct rtonboot_stop_ec_cyclic *)(p_content);
	
	if(p_stop_ec_cyclic->is_stop)
	{
		linux_printf("stop_ec_cyclic_end_callack stop ec cyclic \n");
		
		is_cyclic_quit = 1;
	}
}

void start_ec_inter_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_EC_INTER)
	
	struct rtonboot_start_ec_inter * p_start_ec_inter;
	
	p_start_ec_inter = (struct rtonboot_start_ec_inter *)(p_content);
	
	if(p_start_ec_inter->is_start)
	{
		linux_printf("start_ec_inter_end_callack start ec inter \n");
		
		if(taskid_ec_inter < 0)
		{
			  taskid_ec_inter = task_create("ec_inter_task", CONFIG_ETHERCAT_EC_INTER_PRIORITY,
                       CONFIG_ETHERCAT_EC_INTER_STACKSIZE, ec_inter_task_routine,
                       NULL);
              if(taskid_ec_inter < 0)
              {
				  linux_printf("Winfred Young create ec_inter_task fail\n");
			  }	           
	    }
	    else
	    {
			  sem_post(&g_sem_ec_inter_task);
		}
	}
	
	#endif
}

void stop_ec_inter_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_stop_ec_inter * p_stop_ec_inter;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_stop_ec_inter = (struct rtonboot_stop_ec_inter *)(p_content);
	
	if(p_stop_ec_inter->is_stop)
	{
		linux_printf("stop_ec_inter_end_callack stop ec inter \n");
		
		g_ec_inter_stop_run = 1;
	}
}

void start_simpletest_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_SIMPLETEST)
	
	int ret;
	
	pthread_attr_t attr;
	struct sched_param sparam;
	pthread_t tid;
	
	struct rtonboot_start_simpletest * p_start_simpletest;
	
	p_start_simpletest = (struct rtonboot_start_simpletest *)(p_content);
	
	if(p_start_simpletest->is_start)
	{
		linux_printf("start_simpletest_end_callack start simpletest \n");
		
		if(taskid_simpletest < 0)
		{
			  ret = pthread_key_create(&g_pthread_key, NULL);
			  if (ret != 0)
			  {
					linux_printf("ERROR: pthread_key_create() failed: %d\n", ret);
					
					DEBUGASSERT(false);
			  }
			  
			  g_soem_thread_keycount = 1;
			  
			  g_soem_thread_keycount_saved = 1;
			  
			  pthread_attr_init(&attr);
			  sparam.sched_priority = CONFIG_ETHERCAT_SOEM_SIMPLETEST_PRIORITY;
			  pthread_attr_setschedparam(&attr, &sparam);
			  pthread_attr_setstacksize(&attr, CONFIG_ETHERCAT_SOEM_SIMPLETEST_STACKSIZE);
			  
			  ret = pthread_create(&tid, &attr, simpletest_task_routine, NULL);
			  pthread_attr_destroy(&attr);
			  if (ret != 0)
			  {
					linux_printf("ERROR: Failed to create simpletest_task_routine thread: %d\n", ret);
					
					DEBUGASSERT(false);
              }  
              else
              {
				    taskid_simpletest = 1;
		      }	  
			  
			  /*taskid_simpletest = task_create("simpletest", CONFIG_ETHERCAT_SOEM_SIMPLETEST_PRIORITY,
                       CONFIG_ETHERCAT_SOEM_SIMPLETEST_STACKSIZE, simpletest_task_routine,
                       NULL);
              if(taskid_simpletest < 0)
              {
				  linux_printf("Winfred Young create taskid_simpletest fail\n");
			  }*/           
	    }
	    else
	    {
			  sem_post(&g_sem_simpletest);
		}
	}
	
	#endif
}

extern void print_sleep_lat(void);

void stop_simpletest_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_SIMPLETEST)
	
	struct rtonboot_stop_simpletest * p_stop_simpletest;
	
	p_stop_simpletest = (struct rtonboot_stop_simpletest *)(p_content);
	
	if(p_stop_simpletest->is_stop)
	{
		linux_printf("stop_simpletest_end_callack stop simpletest \n");
		
		g_simpletest_quit = 1;
	}
	
	#endif
}

void start_motorctl_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_MOTORCTL)
	
	int ret;
	
	pthread_attr_t attr;
	struct sched_param sparam;
	pthread_t tid;
	
	struct rtonboot_start_motorctl * p_start_motorctl;
	
	p_start_motorctl = (struct rtonboot_start_motorctl *)(p_content);
	
	if(p_start_motorctl->is_start)
	{
		linux_printf("start_motorctl_end_callack start motorctl \n");
		
		if(taskid_motorctl < 0)
		{
			  ret = pthread_key_create(&g_pthread_key, NULL);
			  if (ret != 0)
			  {
					linux_printf("ERROR: pthread_key_create() failed: %d\n", ret);
					
					DEBUGASSERT(false);
			  }
			  
			  g_soem_thread_keycount = 0;
			  
			  g_soem_thread_keycount_saved = 0;
			  
			  pthread_attr_init(&attr);
			  sparam.sched_priority = CONFIG_ETHERCAT_SOEM_MOTORCTL_PRIORITY;
			  pthread_attr_setschedparam(&attr, &sparam);
			  pthread_attr_setstacksize(&attr, CONFIG_ETHERCAT_SOEM_MOTORCTL_STACKSIZE);
			  
			  ret = pthread_create(&tid, &attr, motorctl_task_routine, NULL);
			  pthread_attr_destroy(&attr);
			  if (ret != 0)
			  {
					linux_printf("ERROR: Failed to create motorctl_task_routine thread: %d\n", ret);
					
					DEBUGASSERT(false);
              }  
              else
              {
				    taskid_motorctl = 1;
		      }           
	    }
	    else
	    {
			  sem_post(&g_sem_motorctl);
		}
	}
	
	#endif
}

void stop_motorctl_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_MOTORCTL)
	
	struct rtonboot_stop_motorctl * p_stop_motorctl;
	
	p_stop_motorctl = (struct rtonboot_stop_motorctl *)(p_content);
	
	if(p_stop_motorctl->is_stop)
	{
		linux_printf("stop_motorctl_end_callack stop motorctl \n");
		
		g_motorctl_stop_run = 1;
	}
	
	#endif
}

void start_motorpp_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_MOTORPP)
	
	int ret;
	
	pthread_attr_t attr;
	struct sched_param sparam;
	pthread_t tid;
	
	struct rtonboot_start_motorpp * p_start_motorpp;
	
	p_start_motorpp = (struct rtonboot_start_motorpp *)(p_content);
	
	if(p_start_motorpp->is_start)
	{
		linux_printf("start_motorpp_end_callack start motorpp \n");
		
		if(taskid_motorpp < 0)
		{
			  ret = pthread_key_create(&g_pthread_key, NULL);
			  if (ret != 0)
			  {
					linux_printf("ERROR: pthread_key_create() failed: %d\n", ret);
					
					DEBUGASSERT(false);
			  }
			  
			  g_soem_thread_keycount = 1;
			  
			  g_soem_thread_keycount_saved = 1;
			  
			  pthread_attr_init(&attr);
			  sparam.sched_priority = CONFIG_ETHERCAT_SOEM_MOTORPP_PRIORITY;
			  pthread_attr_setschedparam(&attr, &sparam);
			  pthread_attr_setstacksize(&attr, CONFIG_ETHERCAT_SOEM_MOTORPP_STACKSIZE);
			  
			  ret = pthread_create(&tid, &attr, motorpp_task_routine, NULL);
			  pthread_attr_destroy(&attr);
			  if (ret != 0)
			  {
					linux_printf("ERROR: Failed to create motorpp_task_routine thread: %d\n", ret);
					
					DEBUGASSERT(false);
              }  
              else
              {
				    taskid_motorpp = 1;
		      }           
	    }
	    else
	    {
			  sem_post(&g_sem_motorpp);
		}
	}
	
	#endif
}

void stop_motorpp_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_MOTORPP)
	
	struct rtonboot_stop_motorpp * p_stop_motorpp;
	
	p_stop_motorpp = (struct rtonboot_stop_motorpp *)(p_content);
	
	if(p_stop_motorpp->is_stop)
	{
		linux_printf("stop_motorpp_end_callack stop motorpp \n");
		
		g_motorpp_stop_run = 1;
	}
	
	#endif
}

void motorpp_update_pos_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	RtonbootTraceDetail("Enter %s", __func__);
	
	#if defined (CONFIG_ETHERCAT_SOEM_MOTORPP)
	
	struct rtonboot_motorpp_update_pos * p_motorpp_update_pos;
	
	p_motorpp_update_pos = (struct rtonboot_motorpp_update_pos *)(p_content);
	
	target_position = p_motorpp_update_pos->newpos;
	
	target_position_updated = true;
	
	#endif
}

uint32_t g_foe_write_file_size = 0;

uint32_t g_sii_write_file_size = 0;

extern sem_t g_foe_write_sem;

extern sem_t g_sii_write_sem;

void foe_write_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_foe_write_para * p_foe_write_para;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_foe_write_para = (struct rtonboot_foe_write_para *)(p_content);
	
	g_foe_write_file_size = p_foe_write_para->file_size;	
	
	sem_post(&g_foe_write_sem);
}

void sii_write_end_callack(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct rtonboot_sii_write_para * p_sii_write_para;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_sii_write_para = (struct rtonboot_sii_write_para *)(p_content);
	
	g_sii_write_file_size = p_sii_write_para->file_size;	
	
	sem_post(&g_sii_write_sem);
}

void rtospub_set_end_callback(uint32_t dst_msgid, uint64_t * p_end_cb, uint64_t * p_end_cb_priv)
{
	RtonbootTraceDetail("Enter %s dst_msgid is %d", __func__, dst_msgid);
	
	switch (dst_msgid)
	{
		case MSGID_LINUX_BULK_READY_MSG:
		
		*p_end_cb = (uint64_t)(&linux_bulk_ready_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_START_EC_CYCLIC:
		
		*p_end_cb = (uint64_t)(&start_ec_cyclic_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_STOP_EC_CYCLIC:
		
		*p_end_cb = (uint64_t)(&stop_ec_cyclic_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_START_EC_INTER:
		
		*p_end_cb = (uint64_t)(&start_ec_inter_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_STOP_EC_INTER:
		
		*p_end_cb = (uint64_t)(&stop_ec_inter_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_START_SIMPLETEST:
		
		*p_end_cb = (uint64_t)(&start_simpletest_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_STOP_SIMPLETEST:
		
		*p_end_cb = (uint64_t)(&stop_simpletest_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_START_MOTORCTL:
		
		*p_end_cb = (uint64_t)(&start_motorctl_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_STOP_MOTORCTL:
		
		*p_end_cb = (uint64_t)(&stop_motorctl_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_START_MOTORPP:
		
		*p_end_cb = (uint64_t)(&start_motorpp_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_STOP_MOTORPP:
		
		*p_end_cb = (uint64_t)(&stop_motorpp_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_MOTORPP_UPDATE_POS:
		
		*p_end_cb = (uint64_t)(&motorpp_update_pos_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_FOE_WRITE:
		
		*p_end_cb = (uint64_t)(&foe_write_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
		
		case MSGID_SII_WRITE:
		
		*p_end_cb = (uint64_t)(&sii_write_end_callack);
		
		*p_end_cb_priv = 0;
		
		break;
	};	
}

void rtos_bulk_ready_prepare_callack(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_bulkpara * p_bulk_in;
	struct rtonboot_bulkpara * p_bulk;
	uint8_t * p_bulk_buf;
	uint32_t * p_temp;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_bulk_in = (struct rtonboot_bulkpara *)(priv);
	p_bulk = (struct rtonboot_bulkpara *)(p_content);
	
	p_bulk->offset = p_bulk_in->offset;
	p_bulk->len = p_bulk_in->len;
	
	p_bulk_buf = get_bulk_ptr(p_bulk->offset);
	p_temp = (uint32_t *)(p_bulk_buf);
	
	*(p_temp) = 0x9999;
	
	flush_rtos_bulk_before_send(p_bulk->offset, p_bulk->len);
	
	*p_msglen = sizeof(struct rtonboot_bulkpara);
}

uint32_t g_foe_read_file_size;

void foe_read_prepare_callack(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_foe_read_para * p_foe_read_para;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_foe_read_para = (struct rtonboot_foe_read_para *)(p_content);
	
	p_foe_read_para->file_size = g_foe_read_file_size;
	
	flush_rtos_bulk_before_send(0, g_foe_read_file_size);
	
	*p_msglen = sizeof(struct rtonboot_foe_read_para);
}

struct rtonboot_ec_cyclic_perf g_ec_cyclic_perf;

void ec_perf_prepare_callack(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_ec_cyclic_perf * p_ec_cyclic_perf;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_ec_cyclic_perf = (struct rtonboot_ec_cyclic_perf *)(p_content);
	
	p_ec_cyclic_perf->period_min_ns = g_ec_cyclic_perf.period_min_ns;
	p_ec_cyclic_perf->period_max_ns = g_ec_cyclic_perf.period_max_ns;
	p_ec_cyclic_perf->exec_min_ns = g_ec_cyclic_perf.exec_min_ns;
	p_ec_cyclic_perf->exec_max_ns = g_ec_cyclic_perf.exec_max_ns;
	p_ec_cyclic_perf->latency_min_ns = g_ec_cyclic_perf.latency_min_ns;
	p_ec_cyclic_perf->latency_max_ns = g_ec_cyclic_perf.latency_max_ns;
	
	*p_msglen = sizeof(struct rtonboot_ec_cyclic_perf);
}

void test_slave_prepare_callack(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	struct rtonboot_test_slave * p_test_slave;
	
	RtonbootTraceDetail("Enter %s", __func__);
	
	p_test_slave = (struct rtonboot_test_slave *)(p_content);
	
	p_test_slave->magic = 0x7777;
	
	*p_msglen = sizeof(struct rtonboot_test_slave);
}

void rteth_recv_prepare_callack(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv)
{
	uint8_t * ptr;
	
	ptr = p_content;
	
	*((uint32_t *)ptr) = g_rteth_pktlen;
	
	ptr += 4;
	
	memcpy(ptr, &g_rteth_buf[0], g_rteth_pktlen);
}
	
void rtospub_set_prepare_callback(uint32_t dst_msgid, prepare_send_callack_func_t * p_cb, void ** p_cb_priv)
{
	RtonbootTraceDetail("Enter %s dst_msgid is %d", __func__, dst_msgid);
	
	switch (dst_msgid)
	{
		case MSGID_RTOS_BULK_READY_MSG:
		
		g_bulkpara.offset = 0;
		g_bulkpara.len = 8;
		
		*p_cb = &rtos_bulk_ready_prepare_callack;
		
		*p_cb_priv = (void **)(&g_bulkpara);
		
		break;
		
		
		case MSGID_FOE_READ:
		
		*p_cb = &foe_read_prepare_callack;
		
		*p_cb_priv = NULL;
		
		break;
		
		case MSGID_EC_CYCLIC_PERF:
		
		*p_cb = &ec_perf_prepare_callack;
		
		*p_cb_priv = NULL;
		
		break;
		
		case MSGID_TEST_SLAVE:
		
		*p_cb = &test_slave_prepare_callack;
		
		*p_cb_priv = NULL;
		
		break;
		
		case MSGID_RTETH_RECV:
		
		*p_cb = &rteth_recv_prepare_callack;
		
		*p_cb_priv = NULL;
		
		break;
	};
}

void prepare_rtos_bulk_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_RTOS_BULK_READY_MSG);
}

void prepare_foe_read_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_FOE_READ);
}

void prepare_ec_perf_and_wake_up(void)
{
	prepare_send_and_wake_up(MSGID_EC_CYCLIC_PERF);
}

struct plat_stmmacenet_data * g_plat_stmmacenet_data = NULL;
struct stmmac_mdio_bus_data * g_mdio_bus_data = NULL;
struct stmmac_dma_cfg * g_dma_cfg = NULL;
struct stmmac_axi * g_axi = NULL;
struct nuttx_bsp_priv * g_bsp_priv = NULL;
struct stmmac_resources * g_res = NULL;
char * g_mac;
int * g_irqs;

int prepare_ec_init_para(void)
{
	struct rtonboot_ec_plat_stmmacenet_data * p_plat_dat;
	struct stmmac_rxq_cfg * p_ec_stmmac_rxq_cfg;
	struct stmmac_txq_cfg * p_ec_stmmac_txq_cfg;
	struct rtonboot_ec_stmmac_mdio_bus_data * p_mdio_bus_data;
	int i;
	struct rtonboot_ec_stmmac_dma_cfg * p_dma_cfg;
	struct rtonboot_ec_stmmac_axi * p_axi;
	struct rtonboot_ec_rk_priv_data * p_bsp_priv;
	struct rtonboot_ec_stmmac_resources * p_res;
	
	g_plat_stmmacenet_data = (struct plat_stmmacenet_data *)(kmm_malloc(sizeof(struct plat_stmmacenet_data)));
	if(!(g_plat_stmmacenet_data))
	{
		linux_printf("Winfred Young ERROR: alloc g_p_plat_data \n");
		
		return -1;
	}
	
	memset(g_plat_stmmacenet_data, 0, sizeof(struct plat_stmmacenet_data));
	
	p_plat_dat = &(g_allpara.ec_plat_stmmacenet_data);
	
	g_plat_stmmacenet_data->bus_id = p_plat_dat->bus_id;
	g_plat_stmmacenet_data->phy_addr = p_plat_dat->phy_addr;
	g_plat_stmmacenet_data->interface = p_plat_dat->interface;	
	g_plat_stmmacenet_data->phy_interface = p_plat_dat->phy_interface;	
	g_plat_stmmacenet_data->clk_csr = p_plat_dat->clk_csr;
	g_plat_stmmacenet_data->has_gmac = p_plat_dat->has_gmac;
	g_plat_stmmacenet_data->enh_desc = p_plat_dat->enh_desc;
	g_plat_stmmacenet_data->tx_coe = p_plat_dat->tx_coe;
	g_plat_stmmacenet_data->rx_coe = p_plat_dat->rx_coe;
	g_plat_stmmacenet_data->bugged_jumbo = p_plat_dat->bugged_jumbo;
	g_plat_stmmacenet_data->pmt = p_plat_dat->pmt;
	g_plat_stmmacenet_data->force_sf_dma_mode = p_plat_dat->force_sf_dma_mode;
	g_plat_stmmacenet_data->force_thresh_dma_mode = p_plat_dat->force_thresh_dma_mode;
	g_plat_stmmacenet_data->riwt_off = p_plat_dat->riwt_off;
	g_plat_stmmacenet_data->max_speed = p_plat_dat->max_speed;
	g_plat_stmmacenet_data->maxmtu = p_plat_dat->maxmtu;
	g_plat_stmmacenet_data->multicast_filter_bins = p_plat_dat->multicast_filter_bins;
	g_plat_stmmacenet_data->unicast_filter_entries = p_plat_dat->unicast_filter_entries;
	g_plat_stmmacenet_data->tx_fifo_size = p_plat_dat->tx_fifo_size;
	g_plat_stmmacenet_data->rx_fifo_size = p_plat_dat->rx_fifo_size;
	g_plat_stmmacenet_data->dma_tx_size = p_plat_dat->dma_tx_size;
	g_plat_stmmacenet_data->dma_rx_size = p_plat_dat->dma_rx_size;;
	g_plat_stmmacenet_data->flow_ctrl = p_plat_dat->flow_ctrl;
	g_plat_stmmacenet_data->addr64 = p_plat_dat->addr64;
	g_plat_stmmacenet_data->rx_queues_to_use = p_plat_dat->rx_queues_to_use;
	g_plat_stmmacenet_data->tx_queues_to_use = p_plat_dat->tx_queues_to_use;
	g_plat_stmmacenet_data->rx_sched_algorithm = p_plat_dat->rx_sched_algorithm;
	g_plat_stmmacenet_data->tx_sched_algorithm = p_plat_dat->tx_sched_algorithm;

	for(i = 0; i < MTL_MAX_RX_QUEUES; ++i)
	{
		p_ec_stmmac_rxq_cfg = &(g_plat_stmmacenet_data->rx_queues_cfg[i]);
		
		p_ec_stmmac_rxq_cfg->mode_to_use = p_plat_dat->rx_queues_cfg[i].mode_to_use;		
		p_ec_stmmac_rxq_cfg->chan = p_plat_dat->rx_queues_cfg[i].chan;
		p_ec_stmmac_rxq_cfg->pkt_route = p_plat_dat->rx_queues_cfg[i].pkt_route;
		p_ec_stmmac_rxq_cfg->use_prio = p_plat_dat->rx_queues_cfg[i].use_prio;
		p_ec_stmmac_rxq_cfg->prio = p_plat_dat->rx_queues_cfg[i].prio;;
	}
	
	for(i = 0; i < MTL_MAX_TX_QUEUES; ++i)
	{
		p_ec_stmmac_txq_cfg = &(g_plat_stmmacenet_data->tx_queues_cfg[i]);
		
		p_ec_stmmac_txq_cfg->weight = p_plat_dat->tx_queues_cfg[i].weight;
		p_ec_stmmac_txq_cfg->mode_to_use = p_plat_dat->tx_queues_cfg[i].mode_to_use;
		p_ec_stmmac_txq_cfg->send_slope = p_plat_dat->tx_queues_cfg[i].send_slope; 
		p_ec_stmmac_txq_cfg->idle_slope = p_plat_dat->tx_queues_cfg[i].idle_slope;
		p_ec_stmmac_txq_cfg->high_credit = p_plat_dat->tx_queues_cfg[i].high_credit;
		p_ec_stmmac_txq_cfg->low_credit = p_plat_dat->tx_queues_cfg[i].low_credit;
		p_ec_stmmac_txq_cfg->use_prio = p_plat_dat->tx_queues_cfg[i].use_prio;
		p_ec_stmmac_txq_cfg->prio = p_plat_dat->tx_queues_cfg[i].prio;
		p_ec_stmmac_txq_cfg->tbs_en = p_plat_dat->tx_queues_cfg[i].tbs_en;
	}
	
	g_plat_stmmacenet_data->ptp_max_adj = p_plat_dat->ptp_max_adj;
	g_plat_stmmacenet_data->has_gmac4 = p_plat_dat->has_gmac4;
	g_plat_stmmacenet_data->has_sun8i = p_plat_dat->has_sun8i;
	g_plat_stmmacenet_data->tso_en = p_plat_dat->tso_en;
	g_plat_stmmacenet_data->rss_en = p_plat_dat->rss_en;
	g_plat_stmmacenet_data->mac_port_sel_speed = p_plat_dat->mac_port_sel_speed;
	g_plat_stmmacenet_data->en_tx_lpi_clockgating = p_plat_dat->en_tx_lpi_clockgating;
	g_plat_stmmacenet_data->rx_clk_runs_in_lpi = p_plat_dat->rx_clk_runs_in_lpi;
	g_plat_stmmacenet_data->has_xgmac = p_plat_dat->has_xgmac;
	
	g_plat_stmmacenet_data->vlan_fail_q_en = p_plat_dat->vlan_fail_q_en;
	g_plat_stmmacenet_data->vlan_fail_q = p_plat_dat->vlan_fail_q;
	g_plat_stmmacenet_data->eee_usecs_rate = p_plat_dat->eee_usecs_rate;
	g_plat_stmmacenet_data->sph_disable = p_plat_dat->sph_disable;
	
	if(g_allpara.is_mdio_bus_data_null)
	{
		goto skip_mdio_bus_data;
	}
	
	g_mdio_bus_data = (struct stmmac_mdio_bus_data *)(kmm_malloc(sizeof(struct stmmac_mdio_bus_data)));
	if(!(g_mdio_bus_data))
	{
		linux_printf("Winfred Young ERROR: alloc g_mdio_bus_data \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		return -1;
	}
	
	memset(g_mdio_bus_data, 0, sizeof(struct stmmac_mdio_bus_data));
	
	p_mdio_bus_data = &(g_allpara.ec_stmmac_mdio_bus_data);
	
	g_mdio_bus_data->phy_mask = p_mdio_bus_data->phy_mask;
	g_mdio_bus_data->probed_phy_irq = p_mdio_bus_data->probed_phy_irq;
	/*g_mdio_bus_data->reset_gpio = p_mdio_bus_data->reset_gpio;
	g_mdio_bus_data->active_low = p_mdio_bus_data->active_low;*/
	
	g_mdio_bus_data->has_xpcs = p_mdio_bus_data->has_xpcs;
	g_mdio_bus_data->needs_reset = p_mdio_bus_data->needs_reset;
	
	/*for(i = 0; i < 3; ++i)
	{
		g_mdio_bus_data->delays[i] = p_mdio_bus_data->delays[i];
	}*/
	
	g_plat_stmmacenet_data->mdio_bus_data = g_mdio_bus_data;
	
	if(g_allpara.is_irqs_null)
	{
		g_mdio_bus_data->irqs = NULL;
		
		goto skip_mdio_bus_data;
	}
	
	g_irqs = (int *)(kmm_malloc( sizeof(int) * PHY_MAX_ADDR));
	if(!(g_irqs))
	{
		linux_printf("Winfred Young ERROR: alloc g_irqs \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		kmm_free(g_mdio_bus_data);
		
		return -1;
	}
	
	for(i = 0; i < PHY_MAX_ADDR; ++i)
	{
		g_irqs[i] = g_allpara.irqs[i];
	}
	
	g_mdio_bus_data->irqs = g_irqs;
	
	skip_mdio_bus_data:
	
	if(g_allpara.is_dma_cfg_null)
	{
		goto skip_dma_cfg;
	}
	
	g_dma_cfg = (struct stmmac_dma_cfg *)(kmm_malloc(sizeof(struct stmmac_dma_cfg)));
	if(!(g_dma_cfg))
	{
		linux_printf("Winfred Young ERROR: alloc g_dma_cfg \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		if(g_mdio_bus_data)
		{
			if( (g_mdio_bus_data->irqs) )
			{
				kmm_free(g_mdio_bus_data->irqs);
			}	
			
			kmm_free(g_mdio_bus_data);
		}
		
		return -1;
	}
	
	memset(g_dma_cfg, 0, sizeof(struct stmmac_dma_cfg));
	
	p_dma_cfg = &(g_allpara.ec_stmmac_dma_cfg);
	
	g_dma_cfg->pbl = p_dma_cfg->pbl;
	g_dma_cfg->txpbl = p_dma_cfg->txpbl;
	g_dma_cfg->rxpbl = p_dma_cfg->rxpbl;
	g_dma_cfg->pblx8 = p_dma_cfg->pblx8;
	g_dma_cfg->fixed_burst = p_dma_cfg->fixed_burst;
	g_dma_cfg->mixed_burst = p_dma_cfg->mixed_burst;
	g_dma_cfg->aal = p_dma_cfg->aal;
	g_dma_cfg->eame = p_dma_cfg->eame;
	
	g_plat_stmmacenet_data->dma_cfg = g_dma_cfg;
	
	skip_dma_cfg:
	
	if(g_allpara.is_axi_null)
	{
		goto skip_axi;
	}
	
	g_axi = (struct stmmac_axi *)(kmm_malloc(sizeof(struct stmmac_axi)));
	if(!(g_axi))
	{
		linux_printf("Winfred Young ERROR: alloc g_axi \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		if(g_mdio_bus_data)
		{
			if( (g_mdio_bus_data->irqs) )
			{
				kmm_free(g_mdio_bus_data->irqs);
			}	
			
			kmm_free(g_mdio_bus_data);
		}
		
		if(g_dma_cfg)
		{
			kmm_free(g_dma_cfg);
		}
		
		return -1;
	}
	
	memset(g_axi, 0, sizeof(struct stmmac_axi));
	
	p_axi  = &(g_allpara.ec_stmmac_axi);
	
	g_axi->axi_lpi_en = p_axi->axi_lpi_en;
	g_axi->axi_xit_frm = p_axi->axi_xit_frm;
	g_axi->axi_wr_osr_lmt = p_axi->axi_wr_osr_lmt;
	g_axi->axi_rd_osr_lmt = p_axi->axi_rd_osr_lmt;
	g_axi->axi_kbbe = p_axi->axi_kbbe;
	
	g_plat_stmmacenet_data->axi = g_axi;
	
	skip_axi:
	
	if(g_allpara.is_bsp_priv_null)
	{
		goto skip_bsp_priv;
	}
	
	g_bsp_priv = (struct nuttx_bsp_priv *)(kmm_malloc(sizeof(struct nuttx_bsp_priv)));
	if(!(g_bsp_priv))
	{
		linux_printf("Winfred Young ERROR: alloc g_bsp_priv \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		if(g_mdio_bus_data)
		{
			if( (g_mdio_bus_data->irqs) )
			{
				kmm_free(g_mdio_bus_data->irqs);
			}	
			
			kmm_free(g_mdio_bus_data);
		}
		
		if(g_dma_cfg)
		{
			kmm_free(g_dma_cfg);
		}
		
		if(g_axi)
		{
			kmm_free(g_axi);
		}
		
		return -1;
	}
	
	memset(g_bsp_priv, 0, sizeof(struct nuttx_bsp_priv));
	
	p_bsp_priv = &(g_allpara.ec_rk_priv_data);
	
	g_bsp_priv->phy_iface = p_bsp_priv->phy_iface;
	g_bsp_priv->bus_id = p_bsp_priv->bus_id;
	g_bsp_priv->suspended = p_bsp_priv->suspended;
	g_bsp_priv->clk_enabled = p_bsp_priv->clk_enabled;
	g_bsp_priv->clock_input = p_bsp_priv->clock_input;
	g_bsp_priv->integrated_phy = p_bsp_priv->integrated_phy;
	g_bsp_priv->tx_delay = p_bsp_priv->tx_delay;
	g_bsp_priv->rx_delay = p_bsp_priv->rx_delay;
	g_bsp_priv->otp_data = p_bsp_priv->otp_data;
	g_bsp_priv->bgs_increment = p_bsp_priv->bgs_increment;
	
	g_plat_stmmacenet_data->bsp_priv = g_bsp_priv;
	
	skip_bsp_priv:
	
	g_res = (struct stmmac_resources *)(kmm_malloc(sizeof(struct stmmac_resources)));
	if(!g_res)
	{
		linux_printf("Winfred Young ERROR: alloc g_res \n");
		
		kmm_free(g_plat_stmmacenet_data);
				
		if(g_mdio_bus_data)
		{
			if( (g_mdio_bus_data->irqs) )
			{
				kmm_free(g_mdio_bus_data->irqs);
			}	
			
			kmm_free(g_mdio_bus_data);
		}
		
		if(g_dma_cfg)
		{
			kmm_free(g_dma_cfg);
		}
		
		if(g_axi)
		{
			kmm_free(g_axi);
		}
		
		if(g_bsp_priv)
		{
			kmm_free(g_bsp_priv);
		}	
		
		return -1;
	}
	
	memset(g_res, 0,  sizeof(struct stmmac_resources) );
	
	p_res = &(g_allpara.ec_stmmac_resources);
	
	g_res->irq = p_res->irq;
	g_res->lpi_irq = p_res->lpi_irq;
	g_res->wol_irq = p_res->wol_irq;
	
	g_mac = (char *)(kmm_malloc(7));
	if(!g_mac)
	{
		linux_printf("Winfred Young ERROR: alloc g_mac \n");
		
		kmm_free(g_plat_stmmacenet_data);
		
		if(g_mdio_bus_data)
		{
			if( (g_mdio_bus_data->irqs) )
			{
				kmm_free(g_mdio_bus_data->irqs);
			}	
			
			kmm_free(g_mdio_bus_data);
		}
		
		if(g_dma_cfg)
		{
			kmm_free(g_dma_cfg);
		}
		
		if(g_axi)
		{
			kmm_free(g_axi);
		}
		
		if(g_bsp_priv)
		{
			kmm_free(g_bsp_priv);
		}	
		
		if(g_res)
		{
			kmm_free(g_res);
		}	
		
		return -1;
	}
	
	g_res->mac = g_mac; 
	
	memcpy( (void *)(g_res->mac), (void *)(&(p_res->mac[0])), 6);

	return 0;
}

extern int /*__init*/ ec_init_module(char ** in_main_devices, unsigned int in_master_count,
	char ** in_backup_devices, unsigned int in_backup_count,
	unsigned int in_debug_level);
	
char * g_in_main_devices[1];

extern int simple_stmmac_init(int netport_idx, struct plat_stmmacenet_data *plat_dat, struct stmmac_resources *res);

extern uint64_t maincore_arm64_arch_timer_count(void);

extern int InitDMAPool(void);

extern void rtnl_init(void);

extern void ecmain_ec_timer_init(void);

extern void ecslave_ec_timer_init(void);

extern int ec_cdev_init(void);

int ec_kernel_exec_thread(int argc, char *argv[])
{
	int ret;
	/*u64 tm_cnt;*/
	
	ret = nxsem_wait(&g_ec_kernel_exec_sig);
	if (ret)
	{
		linux_printf("Winfred Young ERROR: Wait g_rtos_send_sig error=%d\n", ret);
	}
	
	linux_printf("ec_kernel_exec_thread begin init \n");
	
	ret = prepare_ec_init_para();
	if(ret)
	{
		linux_printf("ec_kernel_exec_thread prepare_ec_init_para fail skip ec\n");
		
		goto begin_loop;
	}
	
	/*g_in_main_devices[0] = g_mac;
	
	ret = ec_init_module(g_in_main_devices, 1,
			NULL, 0, KERN_DEBUG);
	if(ret)
	{
		linux_printf("Winfred Young ERROR: ec_init_module fail error=%d\n", ret);
		
		goto begin_loop;
	}*/	
	
	(void)InitDMAPool();
	
	rtnl_init();
	
	ecmain_ec_timer_init();

	ecslave_ec_timer_init();
	
	(void)ec_cdev_init();
	
	ret = simple_stmmac_init(g_allpara.net_port_idx, g_plat_stmmacenet_data, g_res);
	if(ret)
	{
		linux_printf("Winfred Young ERROR: simple_stmmac_init fail error=%d\n", ret);
		
		goto begin_loop;
	}	
	
	begin_loop:
	
	while(1)
	{
		nxsig_sleep(1);
		
		/*usleep(1000);*/
		
		/*tm_cnt = maincore_arm64_arch_timer_count();
		
		linux_printf("Winfred Young maincore_arm64_arch_timer_count = %llx\n", tm_cnt);*/
	};	
}	

void create_and_init_ec_kernel_exec_thread(void)
{
	#if defined (CONFIG_SMP)
	
	cpu_set_t cpuset;
	int ret;
	
	#endif
	
	nxsem_init(&g_ec_kernel_exec_sig, 0, 0);
	
	pid_ec_kernel_exec_task = kthread_create(EC_KERNEL_EXEC_TASK_NAME,
                       EC_KERNEL_EXEC_TASK_PRIORITY,
                       EC_KERNEL_EXEC_TASK_STACK_SIZE,
                       ec_kernel_exec_thread,
                       NULL);
    if (pid_ec_kernel_exec_task < 0)
    {
		linux_printf("Winfred Young ERROR: Failed to create EC Kernel Exec Task error=%d\n", pid_ec_kernel_exec_task);
    }
    
    #if defined (CONFIG_SMP)
    
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    
    ret = nxsched_set_affinity(pid_ec_kernel_exec_task, sizeof(cpuset), &cpuset);
    if (ret < 0)
    {
        linux_printf("Winfred Young ERROR: Failed to set ec_kernel_exec Task affinity ret=%d\n", ret);
    }
    
    #endif
}


