/****************************************************************************
 * apps/nshlib/nsh_session.c
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

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>

#include <stdatomic.h>

#ifdef CONFIG_NSH_CLE
#  include "system/cle.h"
#else
#  include "system/readline.h"
#endif

#include "nsh.h"
#include "nsh_console.h"

/*#define RTONBOOT_DEBUG 1*/

#undef RTONBOOT_DEBUG

extern int rtos_printf(const char *fmt, ...);

extern int linux_printf(const char *fmt, ...);

extern int linux_vprintf(const char *fmt, va_list ap);

extern void linux_serial_putc(int ch);
uint64_t mycount = 0;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_session
 *
 * Description:
 *   This is the common session login on any NSH session.  This function
 *   returns when an error reading from the input stream occurs, presumably
 *   signaling the end of the session.
 *
 *   This function:
 *   - Performs the login sequence if so configured
 *   - Executes the NSH login script
 *   - Presents a greeting
 *   - Then provides a prompt then gets and processes the command line.
 *   - This continues until an error occurs, then the session returns.
 *
 * Input Parameters:
 *   pstate - Abstracts the underlying session.
 *
 * Returned Values:
 *   EXIT_SUCCESS or EXIT_FAILURE is returned.
 *
 ****************************************************************************/

sem_t g_cmd_ready_sem;

extern char * g_exec_cmd_str;

uint32_t g_non_tty_debug = 0;

#define CMD_RESULT_BUF_SIZE 32768

char non_tty_cmd_result_buf[CMD_RESULT_BUF_SIZE];

uint32_t g_non_tty_cmd_result_buflen = CMD_RESULT_BUF_SIZE;

uint32_t g_non_tty_cmd_result_bufoff;

uint32_t g_non_tty_cmd_result_curpos;
char * g_non_tty_cmd_result_bufptr;

#define MAX_CMD_ARGV  20

#define MAX_RCMD_LEN 512

int parse_argv(char * str, int* argc, char** argv, int number)
{
    char * p;
    int num = 0;
    int word_start = 1;
 
    if(argc == NULL || argv == NULL)
        return -1;
 
    p = str;
    
    while(*p){
        if((*p == '\r') || (*p == '\n')){
            *p = '\0';
            break;
        }
        if((*p == ' ') || (*p == '\t')){
            word_start = 1;
            *p = '\0';
            p++;
            continue;
        }
        if(num >= number)
            break;
        
        if(word_start){
            argv[num++] = p;
            word_start = 0;
        }
        p++;
    }
    
    *argc = num;
 
    return 0;
}

extern void linux_serial_puts(const char *s);

extern void prepare_send_result_and_wake_up(void);

extern void prepare_testrr_and_wake_up(void);

extern void prepare_testbc_and_wake_up(void);

extern void prepare_rtos_bulk_and_wake_up(void);

extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

int pipe_printf(const char *fmt, ...)
{
	int ret;
	va_list args;
	uint len;
	char printbuffer[512];
	
	if(!g_non_tty_debug)
	{
		va_start(args, fmt);

		ret = linux_vprintf(fmt, args);
	
		va_end(args);

		return ret;
	}
	else
	{
		va_start(args, fmt);

		len = vsnprintf(NULL, 0, fmt, args);
		
		++len;
		
		if( (len + g_non_tty_cmd_result_curpos) <= g_non_tty_cmd_result_buflen)
		{
			vsnprintf(printbuffer, sizeof(printbuffer), fmt, args);
			
			strcpy(g_non_tty_cmd_result_bufptr, printbuffer);
			
			g_non_tty_cmd_result_curpos += len;
			
			g_non_tty_cmd_result_bufptr += len;
		}	
		
		va_end(args);
		
		return 0;
	}	
}

void pipe_puts(const char *s)
{
	uint len;
	
	if(!g_non_tty_debug)
	{
		linux_serial_puts(s);
	}
	else
	{
		len = strlen(s);
		
		++len;
		
		if( (len + g_non_tty_cmd_result_curpos) <= g_non_tty_cmd_result_buflen)
		{
			strcpy(g_non_tty_cmd_result_bufptr, s);
			
			g_non_tty_cmd_result_curpos += len;
			
			g_non_tty_cmd_result_bufptr += len;
		}	
	}	
}

int pipe_vprintf(const char *fmt, va_list ap)
{
	uint i;
	char printbuffer[512];
	
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, ap);
	
	pipe_puts(printbuffer);
	
	return i;
}


void pipe_putc(const char c)
{ 
	uint len;
	
	if(!g_non_tty_debug)
	{
		linux_serial_putc(c);
	}
	else
	{
		len = 1;
		
		if( (len + g_non_tty_cmd_result_curpos) <= g_non_tty_cmd_result_buflen)
		{
			*(g_non_tty_cmd_result_bufptr) = c;
			
			g_non_tty_cmd_result_curpos += len;
			
			g_non_tty_cmd_result_bufptr += len;
		}	
	}
}	
	
int nsh_output(FAR struct nsh_vtbl_s *vtbl, const char *fmt, ...)
{
	va_list args;
	uint i = 0;
	uint len;
	char printbuffer[512];
	
	if(!g_non_tty_debug)
	{
		va_start(args, fmt);

		i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
		va_end(args);

		linux_serial_puts(printbuffer);
	}
	else
	{
		va_start(args, fmt);

		len = vsnprintf(NULL, 0, fmt, args);
		
		++len;
		
		if( (len + g_non_tty_cmd_result_curpos) <= g_non_tty_cmd_result_buflen)
		{
			i = vsnprintf(printbuffer, sizeof(printbuffer), fmt, args);
			
			strcpy(g_non_tty_cmd_result_bufptr, printbuffer);
			
			g_non_tty_cmd_result_curpos += len;
			
			g_non_tty_cmd_result_bufptr += len;
		}	
		
		va_end(args);
	}		
	
	return i;
}	

int nsh_error(FAR struct nsh_vtbl_s *vtbl, const char *fmt, ...)
{
	va_list args;
	uint i = 0;
	char printbuffer[512];
	uint len;
	
	if(!g_non_tty_debug)
	{
		va_start(args, fmt);

		i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
		
		va_end(args);

		linux_serial_puts(printbuffer);
	}
	else
	{
		va_start(args, fmt);

		len = vsnprintf(NULL, 0, fmt, args);
		
		++len;
		
		if( (len + g_non_tty_cmd_result_curpos) <= g_non_tty_cmd_result_buflen)
		{
			i = vsnprintf(printbuffer, sizeof(printbuffer), fmt, args);
			
			strcpy(g_non_tty_cmd_result_bufptr, printbuffer);
			
			g_non_tty_cmd_result_curpos += len;
			
			g_non_tty_cmd_result_bufptr += len;
		}	
		
		va_end(args);
	}
	
	return i;
}	

extern void scan_list_find_this_pid_entry_and_list(int spid);

extern int osperf(int argc, FAR char *argv[]);

extern int ethercat_main(int argc, char **argv);

char rcmd_buf[MAX_RCMD_LEN];

int rargc = 0;
char * rargv[MAX_CMD_ARGV];

int taskid_ethercat = -1;

sem_t g_sem_ethercat_task = SEM_INITIALIZER(0);

int ethercat_task_routine(int argc, char *argv[])
{
	int retval;
	
	retval = ethercat_main(rargc, rargv);

	if(retval != 0x88)
	{
		prepare_send_result_and_wake_up();
	}	
    
    while(1)
    {
		sem_wait(&g_sem_ethercat_task);
		
		retval = ethercat_main(rargc, rargv);
		
		if(retval != 0x88)
		{  
			prepare_send_result_and_wake_up();
		}	
	};	
    
	return 0;
}	

void create_ethercat_command_thread(void)
{
	#if defined (CONFIG_SMP)
	
	cpu_set_t cpuset;
	int ret;
	
	#endif
	
	taskid_ethercat = task_create("ethercat_task", CONFIG_ETHERCAT_ETHERCAT_PRIORITY,
                       CONFIG_ETHERCAT_ETHERCAT_STACKSIZE, ethercat_task_routine,
                       NULL);
    if(taskid_ethercat < 0)
    {
		pipe_printf("Winfred Young create ethercat_task fail\n");
		
		return;
	}
	
	#if defined (CONFIG_SMP)
	
	CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset); 
    
    ret = nxsched_set_affinity(taskid_ethercat, sizeof(cpuset), &cpuset);
    if (ret < 0) 
    {
        pipe_printf("Winfred Young Failed to set CPU affinity for ethercat_task\n");
    }
    
    #endif 
}


int nsh_session(FAR struct console_stdio_s *pstate,
                int login, int argc, FAR char *argv[])
{
  FAR struct nsh_vtbl_s *vtbl;
  int ret = EXIT_FAILURE;
  int is_testrr = 0;
  int is_list = 0;
  int is_osperf = 0;
  int is_testbc = 0;
  int is_testbr = 0;
  int is_ethercat = 0;
  int is_testsmp = 0;
  int is_testsmp1 = 0;
  int is_testsl = 0;
  int pid;
  char * p_exec_cmd_str;
  
  DEBUGASSERT(pstate);
  vtbl = &pstate->cn_vtbl;

  /* Then enter the command line parsing loop */

  for (; ; )
    {

#if 1
		
      /* For the case of debugging the USB console...
       * dump collected USB trace data
       */

      /* Parse process the command */
      
      while (sem_wait(&g_cmd_ready_sem) < 0)
      {
		  ++ret;
		  
		  --ret;
	  };
	  
	  #ifdef RTONBOOT_DEBUG
  
			linux_printf("Winfred Young begin to process nsh command\n");
			
	  #endif		
	  
	  g_non_tty_debug = *((uint32_t *)g_exec_cmd_str);
	  
	  g_non_tty_cmd_result_bufptr = &non_tty_cmd_result_buf[0];
	  
	  g_non_tty_cmd_result_curpos = 0;

	  g_non_tty_cmd_result_bufoff = ((uint64_t)(g_non_tty_cmd_result_bufptr)) - ((uint64_t)CONFIG_RAM_START);
	  
	  p_exec_cmd_str = g_exec_cmd_str + 4;		
	  
	  strncpy(&rcmd_buf[0], p_exec_cmd_str, MAX_RCMD_LEN - 1);
	  
	  rcmd_buf[MAX_RCMD_LEN - 1] = 0;
	  
	  parse_argv(&rcmd_buf[0], &rargc, rargv, MAX_CMD_ARGV);
	  
	  is_testrr = 0;
	  is_list = 0;
	  is_osperf = 0;
	  is_testbc = 0;
	  is_testbr = 0;
	  is_ethercat = 0;
	  is_testsmp = 0;
	  is_testsmp1 = 0;
	  is_testsl = 0;
	  
	  
	  if(rargc >= 1)
	  {
		  if(!strcmp(rargv[0], "testrr"))
		  {
			  is_testrr = 1;
		  }
		  else if(!strcmp(rargv[0], "list"))
		  {
			  if(rargc < 2)
			  {
				  pipe_printf("Winfred Young Wrong argc need pid para\\n");
				  
				  continue;
			  }
			  else
			  {
				  pid = atoi(rargv[1]);
				  
				  if( (pid != -1) && (pid != -2) && (pid < 0) )
				  {
						pipe_printf("Winfred Young Wrong pid, pid must be -1, -2 or positive number\n");
				 
						continue;
				  }
			   }	  
			  
			   is_list = 1;
		  }
		  else if(!strcmp(rargv[0], "osperf"))
		  {
			  is_osperf = 1;
		  }	  
		  else if(!strcmp(rargv[0], "ethercat"))
		  {
			  is_ethercat = 1;
		  }	
		  else if(!strcmp(rargv[0], "testbc"))
		  {
			  is_testbc = 1;
		  }
		  else if(!strcmp(rargv[0], "testbr"))
		  {
			  is_testbr = 1;
		  }
		  else if(!strcmp(rargv[0], "testsmp"))
		  {
			  is_testsmp = 1;
		  }
		  else if(!strcmp(rargv[0], "testsmp1"))
		  {
			  is_testsmp1 = 1;
		  }
		  else if(!strcmp(rargv[0], "testsl"))
		  {
			  is_testsl = 1;
		  }
	  }
	  
	  if(is_testrr)
	  {
		  prepare_testrr_and_wake_up();	  
	  }
	  else if(is_list)
	  {
		  scan_list_find_this_pid_entry_and_list(pid);
		  
		  prepare_send_result_and_wake_up();
	  }	
	  else if(is_osperf)
	  {
		  g_non_tty_debug = 0;
		  
		  osperf(rargc, rargv);
		  
		  prepare_send_result_and_wake_up();
      }	
      else if(is_ethercat)
	  {
		  if(taskid_ethercat < 0)
		  {
			  create_ethercat_command_thread();
	      }
	      else
	      {
			  sem_post(&g_sem_ethercat_task);
		  }
      }
      else if(is_testbc)
      {
		  prepare_testbc_and_wake_up();
	  }
	  else if(is_testbr)
	  {
		  prepare_rtos_bulk_and_wake_up();
	  }	 
	  else if(is_testsmp)
	  {
		  pipe_printf("This is UP version, this command is invalid !!!\n");
		  
		  prepare_send_result_and_wake_up();
	  }
	  else if(is_testsmp1)
	  {
		  pipe_printf("This is UP version, this command is invalid !!!\n");
		  
		  prepare_send_result_and_wake_up();
	  }
	  else if(is_testsl)
	  {
		  pipe_printf("This is UP version, this command is invalid !!!\n");
		  
		  prepare_send_result_and_wake_up();
	  }	   
	  else
	  {
		  nsh_parse(vtbl, p_exec_cmd_str);
		  // nsh_update_prompt();
		  
		  prepare_send_result_and_wake_up();
	  }	  
      
#else

		rtos_printf("nsh session loop count is %llx\n", mycount);
		
		/*linux_printf("aaaaaaaaaa%llx%llx%llx%llx%llx%llx%llx%llx%llx%llxbbbbbbbbbb\n",
			mycount, mycount, mycount, mycount, mycount, mycount, mycount, mycount, mycount, mycount);*/
		
		usleep(1000L);
		
		++mycount;
      
#endif
      
    }
    
    

  return ret;
}
