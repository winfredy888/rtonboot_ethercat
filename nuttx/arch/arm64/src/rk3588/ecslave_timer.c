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

#include <nuttx/semaphore.h>

#include "ecslave_timer.h"

#define ECSLAVE_TIMER_BASE      0xfeae0040

#define ECSLAVE_TIMER_IRQ 323

#define TIMER_CTRL		0x10
#define TIMER_LOAD_COUNT0	0x00
#define TIMER_LOAD_COUNT1	0x04
#define TIMER_CURRENT_VALUE0	0x08
#define TIMER_CURRENT_VALUE1	0x0C
#define TIMER_INTSTATUS		0x18

#define MYBIT(x) (1<<(x))

#define TIMER_EN		MYBIT(0)
#define TIMER_UD		MYBIT(1)
#define TIMER_INT_EN		MYBIT(2)
#define TIMER_CLR_INT		MYBIT(0)

#define MIN_ALARM_DELAY 50

uint32_t g_ecslave_timer_base = ECSLAVE_TIMER_BASE;

extern int linux_printf(const char *fmt, ...);

#define printf linux_printf

uint64_t ECSlaveRtcTimerContext = 0ULL;

static timer_event_t * ecslave_timer_list_head = NULL;

static void ecslave_rtimer_set_timeout( timer_event_t *obj );

static bool ecslave_rtimer_exists( timer_event_t *obj );

static timer_event_t slave_thread_timer;

int is_slave_enable = 0;

uint64_t last_slave_us = 0;

/*extern uint64_t slavecore_arm64_arch_timer_count(void);*/

extern uint64_t maincore_arm64_arch_timer_count(void);

#define slavecore_arm64_arch_timer_count maincore_arm64_arch_timer_count

uint64_t slavecore_perf_cur_ns(void)
{
  unsigned long long temp;
  uint64_t old_cnt;
  uint64_t cur_cnt;
  
  old_cnt = 0;
  
  cur_cnt = slavecore_arm64_arch_timer_count();
  
  temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
  
  return temp;
}

uint64_t slavecore_get_delay(uint64_t old_cnt, uint64_t cur_cnt)
{
  unsigned long long temp;

  if(old_cnt >= cur_cnt)
  { 
		temp = ((unsigned long long)(old_cnt - cur_cnt) ) * 41;
  }	
  else
  {
		temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
  }	  
  
  return temp;
}

void ecslave_rtimer_init( timer_event_t *obj)
{
    obj->Timestamp = 0;
    obj->ReloadValue = 0;
    obj->IsStarted = false;
    obj->IsNext2Expire = false;
    obj->Next = NULL;
    nxsem_init( &(obj->ec_usleep_sem), 0, 0);
}

static void ecslave_rtimer_reload(void)
{
    // Start the next timer_list_head if it exists AND NOT running
    if( ( ecslave_timer_list_head != NULL ) && ( ecslave_timer_list_head->IsNext2Expire == false ) )
    {
        ecslave_rtimer_set_timeout( ecslave_timer_list_head );
    }
}

uint64_t ecslave_rtc_set_timer_context( void )
{
    ECSlaveRtcTimerContext = slavecore_perf_cur_ns();
    
    return (ECSlaveRtcTimerContext);
}

/*!
 * \brief Gets the RTC timer reference
 *
 * \param none
 * \retval timerValue In ticks
 */
uint64_t ecslave_rtc_get_timer_context( void )
{
    return (ECSlaveRtcTimerContext);
}

uint32_t ecslave_rtc_get_timer_elapsed_time( void )
{  
  uint64_t calendarValue = slavecore_perf_cur_ns();

  return ( calendarValue - ECSlaveRtcTimerContext ) ;
}

void ecslave_rtimer_start( timer_event_t *obj )
{
    uint32_t elapsedTime = 0;
    timer_event_t **target = &ecslave_timer_list_head;
    irqstate_t flags;

    flags = enter_critical_section();

    if( ( obj == NULL ) || ( ecslave_rtimer_exists( obj ) == true ) )
    {
        leave_critical_section(flags);
        
        return;
    }

    obj->Timestamp = obj->ReloadValue;
    obj->IsStarted = true;
    obj->IsNext2Expire = false;
    obj->Next = NULL;
    
    nxsem_init( &(obj->ec_usleep_sem), 0, 0);
    
    // The timer list is automatically sorted. The timer list head always contains the next timer to expire.
    if( ecslave_timer_list_head == NULL )
    {
        ecslave_rtc_set_timer_context( );
    }
    else
    {   
        elapsedTime = ecslave_rtc_get_timer_elapsed_time( );
        obj->Timestamp += elapsedTime;
        
        for(; *target; target = &((*target)->Next) )
        {
            if(((*target)->Timestamp > obj->Timestamp))
            {
                (*target)->IsNext2Expire = false;
                
                obj->Next = *target;
                
                break;
            }
        }
    }
    *target = obj; 
    
    // Start the next timer_list_head if it exists AND NOT running
    ecslave_rtimer_reload();
    
    leave_critical_section(flags);
}

void ecslave_rtc_stop_alarm( void )
{
	putreg32( 0, g_ecslave_timer_base + TIMER_CTRL);
}

void ecslave_rtimer_stop( timer_event_t *obj )
{
	irqstate_t flags;
	
    flags = enter_critical_section();
  
    timer_event_t **target = &ecslave_timer_list_head;
    
    // List is empty or the obj to stop does not exist
    if( ( ecslave_timer_list_head == NULL ) || ( obj == NULL ) )
    {
        leave_critical_section(flags);
        
        return;
    }
    
    obj->IsStarted = false;
    
    // Stop the Head and RTC Timeout
    if( ecslave_timer_list_head == obj ) 
    {
        // The head is already running
        if( ecslave_timer_list_head->IsNext2Expire == true ) 
        {
            ecslave_timer_list_head->IsNext2Expire = false;
            if( ecslave_timer_list_head->Next != NULL )
            {
                ecslave_timer_list_head = ecslave_timer_list_head->Next;
                ecslave_rtimer_set_timeout( ecslave_timer_list_head );
            }
            else
            {
                ecslave_rtc_stop_alarm( );
                
                ecslave_timer_list_head = NULL;
            }
                                        
            leave_critical_section(flags);
            
            return;
        }
    }
    
    // Stop an object within the list
    for(; *target; target = &((*target)->Next) )
    {
        if( *target == obj )
        {
            *target = obj->Next;
            break;
        }
    }
    
    leave_critical_section(flags);
}

static bool ecslave_rtimer_exists( timer_event_t *obj )
{
    timer_event_t* cur = ecslave_timer_list_head;

    while( cur != NULL )
    {
        if( cur == obj )
        {
            return true;
        }
        
        cur = cur->Next;
    }
    
    return false;
}

void ecslave_rtimer_reset( timer_event_t *obj )
{
    ecslave_rtimer_stop( obj );
    ecslave_rtimer_start( obj );
}

uint32_t ecslave_rtc_get_minimum_timeout( void )
{
    return( MIN_ALARM_DELAY );
}

void ecslave_rtimer_set_value( timer_event_t *obj, uint64_t us )
{
    uint64_t minValue = 0;
    uint64_t ns = 0;

	ecslave_rtimer_stop( obj );

    minValue = ecslave_rtc_get_minimum_timeout( );

    if( us < minValue )
    {
        us = minValue;
    }
    
    ns = (us * 1000ULL);

    obj->Timestamp = ns;
    obj->ReloadValue = ns;
}

static void ecslave_rtimer_set_timeout( timer_event_t *obj )
{
    uint64_t minTicks = ecslave_rtc_get_minimum_timeout( );
    uint64_t ns = 0;
    uint64_t cycles = 0;
    uint32_t load_count0, load_count1;
    
    obj->IsNext2Expire = true;
    
    ns = (minTicks * 1000ULL);

    // In case deadline too soon
    if( obj->Timestamp  < ( ecslave_rtc_get_timer_elapsed_time( ) + ns ) )
    {
        obj->Timestamp = ecslave_rtc_get_timer_elapsed_time( ) + ns;
    }
    
    putreg32( 0, g_ecslave_timer_base + TIMER_CTRL);
    
    cycles = (obj->Timestamp) / 41;
					
	load_count1 = cycles >> 32;
	load_count0 = cycles & 0xffffffffL;
	
	putreg32(load_count0, g_ecslave_timer_base + TIMER_LOAD_COUNT0);
    putreg32(load_count1, g_ecslave_timer_base + TIMER_LOAD_COUNT1);
  		
	putreg32(TIMER_CLR_INT, g_ecslave_timer_base + TIMER_INTSTATUS);
	putreg32( TIMER_EN | TIMER_INT_EN | TIMER_UD, g_ecslave_timer_base +  TIMER_CTRL );
}

int ecslave_timer_isr(int irq, void *regs, void *arg)
{
    timer_event_t** target = &ecslave_timer_list_head;
    timer_event_t* cur;
    
    putreg32( TIMER_CLR_INT, g_ecslave_timer_base + TIMER_INTSTATUS);
    
    if(!is_slave_enable)
    {
		return 0;
	}	

    uint32_t old =  ecslave_rtc_get_timer_context( );
    uint32_t now =  ecslave_rtc_set_timer_context( );
    // intentional wrap around
    uint32_t deltaContext = now - old; 

    // Update timeStamp based upon new Time Reference
    // because delta context should never exceed 2^32
    if( ecslave_timer_list_head != NULL )
    {
        for(; (*target)->Next; target = &((*target)->Next) )
        {
            if( (*target)->Next->Timestamp > deltaContext )
            {
                (*target)->Next->Timestamp -= deltaContext;
            }
            else
            {
                (*target)->Next->Timestamp = 0;
            }
        }

        // Execute immediately the alarm callback
        cur = ecslave_timer_list_head;
        ecslave_timer_list_head = ecslave_timer_list_head->Next;
        cur->IsStarted = false;
        
        nxsem_post( &(cur->ec_usleep_sem) );
    }

    // Remove all the expired object from the list
    while( ( ecslave_timer_list_head != NULL ) && ( ecslave_timer_list_head->Timestamp < ecslave_rtc_get_timer_elapsed_time( ) ) )
    {
        cur = ecslave_timer_list_head;
        ecslave_timer_list_head = ecslave_timer_list_head->Next;
        cur->IsStarted = false;
        
        nxsem_post( &(cur->ec_usleep_sem) );
    }

    // Start the next timer_list_head if it exists AND NOT running
    ecslave_rtimer_reload();
    
    return OK;
}

void ecslave_ec_timer_init(void)
{
	ECSlaveRtcTimerContext = 0ULL;
	
	g_ecslave_timer_base = ECSLAVE_TIMER_BASE;
	
	is_slave_enable = 0;

	last_slave_us = 0;
	
	ecslave_rtimer_init( &slave_thread_timer);
}	

uint64_t ecslave_get_ec_timer_count(void)
{
	uint32_t cur_count0, cur_count1;
	uint64_t tmp;
	
	cur_count0 = getreg32( g_ecslave_timer_base + TIMER_CURRENT_VALUE0 );
	
	cur_count1 = getreg32( g_ecslave_timer_base + TIMER_CURRENT_VALUE1 );
	
	tmp = cur_count0 + (((uint64_t)cur_count1) << 32);
	
	return tmp;
}

uint64_t ecslave_get_ec_timer_compare(void)
{
	uint32_t load_count0, load_count1;
	uint64_t tmp;
	
	load_count0 = getreg32( g_ecslave_timer_base + TIMER_LOAD_COUNT0 );
	
	load_count1 = getreg32( g_ecslave_timer_base + TIMER_LOAD_COUNT1 );
	
	tmp = load_count0 + (((uint64_t)load_count1) << 32);
	
	return tmp;
}

extern void append_sleep_lat(uint16_t idx, uint64_t ns);

void ecslave_timer_usleep(uint64_t us)
{
  /*uint64_t old_cnt;
  uint64_t cur_cnt;
  uint64_t temp;
 
  old_cnt = slavecore_arm64_arch_timer_count();*/
  
  if(!is_slave_enable)
  {
	  is_slave_enable = 1;
	  
	  /*up_enable_irq(ECSLAVE_TIMER_IRQ);*/
  }
  
  if(us != last_slave_us)
  {
		ecslave_rtimer_set_value( &slave_thread_timer, us);
		  
		last_slave_us = us;
  }
	  
  ecslave_rtimer_start( &slave_thread_timer );
  	  
  nxsem_wait( &(slave_thread_timer.ec_usleep_sem) );
  
  /*cur_cnt = slavecore_arm64_arch_timer_count();
  
    temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
    
    temp -= (us * 1000);
  
  append_sleep_lat(2, temp);*/
}

void ecslave_timer_irq_init(void)
{
	irq_attach(ECSLAVE_TIMER_IRQ, ecslave_timer_isr, NULL);
			
	/*up_disable_irq(ECSLAVE_TIMER_IRQ);*/
}	
