/****************************************************************************
 * apps/testing/osperf/osperf.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/poll.h>

#include <nuttx/sched.h>
#include <nuttx/kthread.h>

#include "../common/arm64_internal.h"

extern int linux_printf(const char *fmt, ...);

#define printf linux_printf

#define CONFIG_TESTING_OSPERF_PRIORITY 120

#define CONFIG_TESTING_CONTEXT_PRIORITY (CONFIG_INIT_PRIORITY)

#define CONFIG_TEST_KTHREAD_PRIORITY 130

#define TEST_KTHREAD_TASK_STACK_SIZE 8192

#define CONFIG_YILED_KTHREAD_PRIORITY 125

#define YIELD_KTHREAD_TASK_STACK_SIZE 8192

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct performance_time_s
{
  /*clock_t start;
  clock_t end;*/
  uint64_t start;
  uint64_t end;
};

struct performance_thread_s
{
  sem_t sem;
  struct performance_time_s time;
};

struct performance_entry_s
{
  const char name[NAME_MAX];
  CODE size_t (*entry)(void);
};

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

static size_t pthread_create_performance(void);
static size_t pthread_switch_performance(void);
static size_t context_switch_performance(void);
static size_t hpwork_performance(void);
/*static size_t poll_performance(void);*/
static size_t semwait_performance(void);
static size_t sempost_performance(void);

pid_t pid_high = -1;
pid_t pid_low = -1;

sem_t g_high_sig;
sem_t g_low_sig;

struct performance_thread_s g_kthread_perf;

int is_end_test;

static size_t kthread_sempost_performance(void);

pid_t pid_yield_high = -1;
pid_t pid_yield_low = -1;

sem_t g_yield_high_sig;
sem_t g_yield_low_sig;

static size_t kthread_yield_performance(void);

extern uint64_t maincore_arm64_arch_timer_count(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct performance_entry_s g_entry_list[] =
{
  {"pthread-create", pthread_create_performance},
  {"pthread-switch", pthread_switch_performance},
  {"context-switch", context_switch_performance},
  {"hpwork", hpwork_performance},
  /*{"poll-write", poll_performance},*/
  {"semwait", semwait_performance},
  {"sempost", sempost_performance},
  {"kthread-sempost", kthread_sempost_performance},
  {"kthread-yield", kthread_yield_performance},
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int performance_thread_create(FAR void *(*entry)(FAR void *),
                                     FAR void *arg, int priority)
{
  struct sched_param param;
  pthread_attr_t attr;
  pthread_t tid;

  param.sched_priority = priority;
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  pthread_attr_setschedparam(&attr, &param);
  pthread_create(&tid, &attr, entry, arg);
  DEBUGASSERT(tid > 0);
  return tid;
}

int is_init = 0;

static void performance_start(FAR struct performance_time_s *result)
{
  #if 0
  	
  result->start = perf_gettime();
  
  #else
  
  if(is_init)
     goto check;
     
  is_init = 1;   
        
  check:
  
  result->start = maincore_arm64_arch_timer_count();
  
  #endif
}

static void performance_end(FAR struct performance_time_s *result)
{
  #if 0
  
  result->end = perf_gettime();
  
  #else
  
  result->end = maincore_arm64_arch_timer_count();
  
  #endif
}

static size_t performance_gettime(FAR struct performance_time_s *result)
{
  unsigned long long temp;
	
  /*struct timespec ts;
  perf_convert(result->end - result->start, &ts);*/
  
  temp = ((unsigned long long)(result->end - result->start) ) * 41;
  
  //linux_printf("winfred Young temp is %lld\n", temp);
  
  //temp /= 255564;
  /*return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;*/
  
  return temp;
}

/****************************************************************************
 * Pthread swtich performance
 ****************************************************************************/

static FAR void *pthread_switch_task(FAR void *arg)
{
  FAR struct performance_thread_s *perf = arg;
  irq_t flags = enter_critical_section();
  sem_wait(&perf->sem);
  performance_end(&perf->time);
  leave_critical_section(flags);
  return NULL;
}

static size_t pthread_switch_performance(void)
{
  struct performance_thread_s perf;
  pthread_t tid;

  sem_init(&perf.sem, 0, 0);
  tid = performance_thread_create(pthread_switch_task, &perf,
                                  CONFIG_TESTING_OSPERF_PRIORITY + 1);

  performance_start(&perf.time);
  sem_post(&perf.sem);
  pthread_join(tid, NULL);

  return performance_gettime(&perf.time);
}

/****************************************************************************
 * Pthread create performance
 ****************************************************************************/

static FAR void *pthread_create_task(FAR void *arg)
{
  FAR struct performance_time_s *time = arg;
  performance_end(time);
  return NULL;
}

static size_t pthread_create_performance(void)
{
  struct performance_time_s result;
  struct sched_param param;
  pthread_attr_t attr;
  pthread_t tid;

  sched_getparam(gettid(), &param);
  param.sched_priority++;
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  pthread_attr_setschedparam(&attr, &param);

  performance_start(&result);
  pthread_create(&tid, &attr, pthread_create_task, &result);
  pthread_join(tid, NULL);

  return performance_gettime(&result);
}

/****************************************************************************
 * Contxt create performance
 ****************************************************************************/

static FAR void *context_swtich_task(FAR void *arg)
{
  FAR struct performance_time_s *time = arg;
  sched_yield();
  performance_end(time);
  return 0;
}

static size_t context_switch_performance(void)
{
  struct performance_time_s time;
  int tid;

  tid = performance_thread_create(context_swtich_task, &time,
                                  CONFIG_TESTING_CONTEXT_PRIORITY);
  sched_yield();
  performance_start(&time);
  sched_yield();
  pthread_join(tid, NULL);
   
  /*while(1)
  {
	  performance_start(&time);
  
	  up_udelay(1);
  
	  performance_end(&time);
	  
	  ++tid;
	  --tid;
  };*/	  
  
  return performance_gettime(&time);
}

/****************************************************************************
 * wdog performance
 ****************************************************************************/

static void work_handle(void *arg)
{
  FAR struct performance_time_s *time = ((FAR void **)arg)[2];
  FAR sem_t *sem = ((void **)arg)[1];
  performance_end(time);
  sem_post(sem);
}

static size_t hpwork_performance(void)
{
  struct performance_time_s result;
  struct work_s work;
  sem_t sem = SEM_INITIALIZER(0);
  int ret;

  FAR void *args = (void *[])
  {
    (FAR void *)&work,
    (FAR void *)&sem,
    (FAR void *)&result
  };

  memset(&work, 0, sizeof(work));
  performance_start(&result);
  ret = work_queue(HPWORK, &work, work_handle, args, 0);
  DEBUGASSERT(ret == 0);

  sem_wait(&sem);
  return performance_gettime(&result);
}

/****************************************************************************
 * poll-write performance
 ****************************************************************************/

#if 0

static FAR void *poll_task(FAR void *arg)
{
  FAR void **argv = arg;
  FAR struct performance_time_s *time = argv[0];
  int pipefd = (int)(uintptr_t)argv[1];

  performance_start(time);
  write(pipefd, "a", 1);
  return 0;
}


static size_t poll_performance(void)
{
  struct performance_time_s result;
  struct pollfd fds;
  FAR void *argv[2];
  int pipefd[2];
  int ret;

  ret = pipe(pipefd);
  DEBUGASSERT(ret == 0);
  fds.fd = pipefd[0];
  fds.events = POLLIN;
  argv[0] = (FAR char *)&result;
  argv[1] = (FAR char *)(uintptr_t)pipefd[1];

  ret = performance_thread_create(poll_task, argv, CONFIG_INIT_PRIORITY);

  poll(&fds, 1, -1);
  performance_end(&result);

  close(pipefd[0]);
  close(pipefd[1]);
  pthread_join(ret, NULL);
  return performance_gettime(&result);
}

#endif

/****************************************************************************
 * semwait_performance
 ****************************************************************************/

static size_t semwait_performance(void)
{
  struct performance_time_s result;
  sem_t sem;

  sem_init(&sem, 0, 2);

  performance_start(&result);
  sem_wait(&sem);
  performance_end(&result);

  sem_destroy(&sem);
  return performance_gettime(&result);
}

/****************************************************************************
 * sempost_performance
 ****************************************************************************/

static size_t sempost_performance(void)
{
  struct performance_time_s result;
  sem_t sem;

  sem_init(&sem, 0, 0);

  performance_start(&result);
  sem_post(&sem);
  performance_end(&result);

  sem_destroy(&sem);
  return performance_gettime(&result);
}

int low_thread(int argc, char *argv[])
{
	while (1)
	{
		nxsem_wait(&g_low_sig);
		
		performance_start(&(g_kthread_perf.time));
		
		nxsem_post(&g_high_sig);
    };

	return 0;
}

int high_thread(int argc, char *argv[])
{
	while (1)
	{
		nxsem_wait(&g_high_sig);
		
		performance_end(&(g_kthread_perf.time));
		
		//sem_post(&(g_kthread_perf.sem));
		
		is_end_test = 1;
    };

	return 0;
}

static size_t kthread_sempost_performance(void)
{
	if( (pid_low < 0) || (pid_high < 0) )
	{
		nxsem_init(&g_high_sig, 0, 0);
	
		nxsem_init(&g_high_sig, 0, 0);
	
		//sem_init(&(g_kthread_perf.sem), 0, 0);
	}	

    if(pid_low < 0)
    {
		pid_low = kthread_create("test_kthread_low",
                       CONFIG_TEST_KTHREAD_PRIORITY,
                       TEST_KTHREAD_TASK_STACK_SIZE,
                       low_thread,
                       NULL);
    } 
    
    if(pid_high < 0)
    {                                    
		pid_high = kthread_create("test_kthread_high",
                       CONFIG_TEST_KTHREAD_PRIORITY + 1,
                       TEST_KTHREAD_TASK_STACK_SIZE,
                       high_thread,
                       NULL);                   
    }
    
    is_end_test = 0;                     
    
    nxsem_post(&g_low_sig);
    
    while(!is_end_test)
    {
	};
	
	return performance_gettime(&(g_kthread_perf.time));
}

int is_yielded = 0;

int low_yield_thread(int argc, char *argv[])
{
	while (1)
	{
		nxsem_wait(&g_yield_low_sig);
		
		nxsem_post(&g_yield_high_sig);
		
		do 
		{
		} while(!is_yielded);
		
		performance_end(&(g_kthread_perf.time));
		
		is_end_test = 1;
    };

	return 0;
}

int high_yield_thread(int argc, char *argv[])
{
	while (1)
	{
		nxsem_wait(&g_yield_high_sig);
		
		is_yielded = 1;
		
		performance_start(&(g_kthread_perf.time));
		
		sched_yield();	
    };

	return 0;
}	

static size_t kthread_yield_performance(void)
{
	if( (pid_yield_low < 0) || (pid_yield_high < 0) )
	{
		nxsem_init(&g_yield_high_sig, 0, 0);
	
		nxsem_init(&g_yield_high_sig, 0, 0);
	
		//sem_init(&(g_kthread_perf.sem), 0, 0);
	}	

    if(pid_yield_low < 0)
    {
		pid_yield_low = kthread_create("yiled_kthread_low",
                       CONFIG_YILED_KTHREAD_PRIORITY,
                       YIELD_KTHREAD_TASK_STACK_SIZE,
                       low_yield_thread,
                       NULL);
    } 
    
    if(pid_yield_high < 0)
    {                                    
		pid_yield_high = kthread_create("yield_kthread_high",
                       CONFIG_YILED_KTHREAD_PRIORITY + 1,
                       YIELD_KTHREAD_TASK_STACK_SIZE,
                       high_yield_thread,
                       NULL);                   
    }
    
    is_end_test = 0;
    
    is_yielded = 0;                  
    
    nxsem_post(&g_yield_low_sig);
    
    while(!is_end_test)
    {
	};
	
	return performance_gettime(&(g_kthread_perf.time));
}

/****************************************************************************
 * performance_help
 ****************************************************************************/

static void performance_help(void)
{
  printf("Usage: performance [OPTIONS] [name]\n\n");
  printf("OPTIONS:\n");
  printf("\t-c, \tNumber of times to run each test\n");
  printf("\t-d, \tShow detail of each test\n");
  printf("\t-h, \tShow this help message\n");
  printf("\t-l, \tList all tests\n");
}

/****************************************************************************
 * performance_run
 ****************************************************************************/

static void performance_run(const FAR struct performance_entry_s *item,
                            size_t count, bool detail)
{
  size_t total = 0;
  size_t max = 0;
  size_t min = 0;
  size_t i;

  for (i = 0; i < (count + 1); i++)
    {
      irq_t flags = enter_critical_section();
      size_t time = item->entry();
      leave_critical_section(flags);
      
      if(!i)
		continue;

      total += time;
      if (time > max)
        {
          max = time;
        }

      if (time < min || min == 0)
        {
          min = time;
        }

      if (detail)
        {
          printf("\t%zu: %zu\n", i, time);
        }
    }
    
    /*if(pid_low >= 0)
    {
		kthread_delete(pid_low);
		
		pid_low = -1;
    } 
    
    if(pid_high < 0)
    {   
		kthread_delete(pid_high);    
		
		pid_high = -1;
    } */  
    
    /*if(pid_yield_low >= 0)
    {
		kthread_delete(pid_yield_low);
		
		pid_yield_low = -1;
    } 
    
    if(pid_yiled_high < 0)
    {   
		kthread_delete(pid_yield_high);    
		
		pid_yield_high = -1;
    } */   

  printf("%-*s %10zu %10zu %10zu\n", NAME_MAX, item->name, max, min,
         total / count);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static const FAR
struct performance_entry_s *find_entry(FAR const char *name)
{
  size_t i;

  for (i = 0; i < nitems(g_entry_list); i++)
    {
      if (strcmp(name, g_entry_list[i].name) == 0)
        {
          return &g_entry_list[i];
        }
    }

  return NULL;
}

void performance_list(void)
{
  size_t i;

  for (i = 0; i < nitems(g_entry_list); i++)
    {
      printf("%s\n", g_entry_list[i].name);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int osperf(int argc, FAR char *argv[])
{
  const FAR struct performance_entry_s *item = NULL;
  bool detail = false;
  size_t count = 100;
  size_t i;
  int opt;

  while ((opt = getopt(argc, argv, "dc:hl")) != -1)
    {
      switch (opt)
        {
          case 'd':
            detail = true;
            break;
          case 'c':
            count = strtoul(optarg, NULL, 0);
            break;
          case 'h':
            performance_help();
            return EXIT_SUCCESS;
          case 'l':
            performance_list();
            return EXIT_SUCCESS;
          default:
            performance_help();
            return EXIT_FAILURE;
        }
    }

  if (optind < argc)
    {
      item = find_entry(argv[optind]);
      if (item == NULL)
        {
          printf("Can't find %s\n", argv[optind]);
          return EXIT_FAILURE;
        }
    }

  printf("OS performance args: count:%zu, detail:%s\n", count,
         detail ? "true" : "false");

  printf("==============================================================\n");
  printf("%-*s %10s %10s %10s\n", NAME_MAX, "Describe", "Max", "Min", "Avg");

  if (item != NULL)
    {
      performance_run(item, count, detail);
      return EXIT_SUCCESS;
    }

  for (i = 0; i < nitems(g_entry_list); i++)
    {
      item = &g_entry_list[i];
      performance_run(item, count, detail);
    }

  return EXIT_SUCCESS;
}

uint64_t ec_perf_get_calc_diff(uint64_t old_cnt, uint64_t cur_cnt, uint64_t ns)
{
	unsigned long long temp;
	
	temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
	
	if(temp > ns)
	{
		return (temp - ns);
	}	
	else
	{
		return (ns - temp);
	}	
}	

