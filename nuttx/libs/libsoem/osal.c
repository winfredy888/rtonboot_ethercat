/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include "osal.h"

#include <nuttx/signal.h>
#include <nuttx/kmalloc.h>

#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define CONFIG_SOEM_TASK_PRIORITY 129
#define CONFIG_SOEM_RTTASK_PRIORITY 130

extern void ecmain_timer_usleep(uint64_t us);
extern void ecslave_timer_usleep(uint64_t us);

extern pthread_key_t g_pthread_key;

int osal_usleep (uint32 usec)
{
   FAR void *data;
   
   data = pthread_getspecific(g_pthread_key);
   if (data == (FAR void *)0x88888888)
   {
	   ecmain_timer_usleep(usec);
   }
   else if(data == (FAR void *)0x99999999)
   {
	   ecslave_timer_usleep(usec);
   }
   else
   {
	   /*DEBUGASSERT(false);*/
	   
		nxsig_usleep(usec);
   }		
   
   return 0;
}

extern uint64_t maincore_arm64_arch_timer_count(void);

ec_timet osal_current_time (void)
{
   uint64_t cur_cnt;
   ec_timet return_value;
   uint64_t temp; 
   
   cur_cnt = maincore_arm64_arch_timer_count();
   
   return_value.cycles = cur_cnt;
   
   cur_cnt *= 41;
   
   temp = 1000ULL * 1000ULL * 1000ULL;
   
   return_value.sec = cur_cnt / temp;
   
   temp *= (return_value.sec);
   
   cur_cnt -= temp;
   
   return_value.usec = cur_cnt / 1000ULL;
   
   return return_value;
}

void osal_timer_start (osal_timert * self, uint32 timeout_usec)
{
   uint64_t cur_cnt;
   uint64_t temp; 
   uint64_t tmp1;
   
   cur_cnt = maincore_arm64_arch_timer_count();
   
   cur_cnt *= 41;
   
   temp = 1000ULL * timeout_usec;
   
   cur_cnt += temp;
   
   self->stop_time.cycles = cur_cnt / 41;
   
   tmp1 = 1000ULL * 1000ULL * 1000ULL;
   
   self->stop_time.sec = cur_cnt / tmp1;
   
   tmp1 *= (self->stop_time.sec);
   
   cur_cnt -= tmp1;
   
   self->stop_time.usec = cur_cnt / 1000ULL;
}

boolean osal_timer_is_expired (osal_timert * self)
{
   uint64_t cur_cnt;
   int is_not_yet_expired;
   
   cur_cnt = maincore_arm64_arch_timer_count();
   
   if(cur_cnt < (self->stop_time.cycles))
   {
	   is_not_yet_expired = 1;
   }	   
   else
   {
	   is_not_yet_expired = 0;
   }	   
   
   if(is_not_yet_expired)
		return false;
   else
		return true;
}

void *osal_malloc(size_t size)
{
   return kmm_malloc(size);
}

void osal_free(void *ptr)
{
   kmm_free(ptr);
}

/*int osal_task(int argc, FAR char * argv[])
{
	soem_taskfunc_t soem_task;
	void * param;
	
	soem_task = (soem_taskfunc_t)((uintptr_t)strtoul(argv[1], NULL, 16));
	
	param = (void *)(uintptr_t)((uintptr_t)strtoul(argv[2], NULL, 16));
	
	(*soem_task)(param);
	
    return 0;
}

int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
   FAR char *argv[3];
   pid_t * tid = (pid_t *)thandle;
   char faddr[80];
   char paddr[80];
      
   snprintf(faddr, sizeof(faddr), "0x%" PRIxPTR, (uintptr_t)func);
   
   argv[0] = faddr;
   
   snprintf(paddr, sizeof(paddr), "0x%" PRIxPTR, (uintptr_t)param);
   
   argv[1] = paddr;
   
   argv[2] = NULL; 
   
   *tid = task_create("worker", CONFIG_SOEM_TASK_PRIORITY, stacksize, osal_task, argv);
   if ((*tid) < 0)
   {
      return 0;
   }
   
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{  
   FAR char *argv[3];
   pid_t * tid = (pid_t *)thandle;
   char faddr[80];
   char paddr[80];
      
   snprintf(faddr, sizeof(faddr), "0x%" PRIxPTR, (uintptr_t)func);
   
   argv[0] = faddr;
   
   snprintf(paddr, sizeof(paddr), "0x%" PRIxPTR, (uintptr_t)param);
   
   argv[1] = paddr;
   
   argv[2] = NULL; 
   
   *tid = task_create("worker_rt", CONFIG_SOEM_TASK_PRIORITY, stacksize, osal_task, argv);
   if ((*tid) < 0)
   {
      return 0;
   }
   
   return 1;
}*/

extern void set_thread_key(void);

soem_taskfunc_t g_soem_task;
void * g_soem_param;

void * osal_task(FAR void *parameter)
{
    set_thread_key();
    
    (*g_soem_task)(g_soem_param);
	
    return 0;
}

int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
   pthread_t * threadp;
   
   pthread_attr_t attr;
   struct sched_param sparam;
   
   int ret;
   
   threadp = thandle;
   
   g_soem_task = func;
   g_soem_param = param;
   
   pthread_attr_init(&attr);
   sparam.sched_priority = CONFIG_SOEM_TASK_PRIORITY;
   pthread_attr_setschedparam(&attr, &sparam);
   pthread_attr_setstacksize(&attr, stacksize);
   
   ret = pthread_create(threadp, &attr, osal_task, NULL);
   pthread_attr_destroy(&attr);
   if (ret != 0)
   {
		linux_printf("ERROR: osal_thread_create Failed : %d\n", ret);
					
		return 0;
   }  
              
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   pthread_t * threadp;
   
   pthread_attr_t attr;
   struct sched_param sparam;
   
   int ret;
   
   threadp = thandle;
   
   g_soem_task = func;
   g_soem_param = param;
   
   pthread_attr_init(&attr);
   sparam.sched_priority = CONFIG_SOEM_RTTASK_PRIORITY;
   pthread_attr_setschedparam(&attr, &sparam);
   pthread_attr_setstacksize(&attr, stacksize);
   
   ret = pthread_create(threadp, &attr, osal_task, NULL);
   pthread_attr_destroy(&attr);
   if (ret != 0)
   {
		linux_printf("ERROR: osal_thread_create_rt Failed : %d\n", ret);
					
		return 0;
   }  
              
   return 1;
}	
