/****************************************************************************
 * apps/system/nsh/nsh_main.c
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

#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/boardctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nshlib/nshlib.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_main
 *
 * Description:
 *   This is the main logic for the case of the NSH task.  It will perform
 *   one-time NSH initialization and start an interactive session on the
 *   current console device.
 *
 ****************************************************************************/
 
extern sem_t g_cmd_ready_sem; 
extern int rtos_printf(const char *fmt, ...);
extern int linux_printf(const char *fmt, ...);

extern sem_t g_foe_write_sem;
 
int main(int argc, FAR char *argv[])
{
  struct sched_param param;
  int ret = 0;
  
  rtos_printf("Winfred Young Initializing semaphore to 0\n");
  sem_init(&g_cmd_ready_sem, 0, 0);
  
  sem_init(&g_foe_write_sem, 0, 0);

  /* Check the task priority that we were started with */

  sched_getparam(0, &param);
  if (param.sched_priority != CONFIG_SYSTEM_NSH_PRIORITY)
    {
      /* If not then set the priority to the configured priority */

      param.sched_priority = CONFIG_SYSTEM_NSH_PRIORITY;
      sched_setparam(0, &param);
    }
    
    #if defined (CONFIG_SMP)
    
    cpu_set_t cpuset;
    
    CPU_ZERO(&cpuset);          
    CPU_SET(0, &cpuset);

    ret = nxsched_set_affinity(0, sizeof(cpuset), &cpuset);
    if (ret < 0) {
        linux_printf("Winfred Young Failed to set CPU affinity for nsh_main\n");
    } else {
        linux_printf("Winfred Young nsh_main is now bound to CPU0 \n");
    }
    
    #endif

  /* Initialize the NSH library */

  nsh_initialize();

#ifdef CONFIG_NSH_CONSOLE

  /* If the serial console front end is selected, run it on this thread */
  
  /*rtos_printf("Winfred Young call nsh_consolemain\n");*/

  ret = nsh_consolemain(argc, argv);

  /* nsh_consolemain() should not return.  So if we get here, something
   * is wrong.
   */

  dprintf(STDERR_FILENO, "ERROR: nsh_consolemain() returned: %d\n", ret);
  ret = 1;
#endif

  return ret;
}
