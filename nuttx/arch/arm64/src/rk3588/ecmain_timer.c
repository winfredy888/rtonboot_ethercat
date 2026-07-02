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

#include "ecmain_timer.h"

#define ECMAIN_TIMER_BASE      0xfeae0080

#define ECMAIN_TIMER_IRQ 325

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

uint32_t g_ec_timer_base = ECMAIN_TIMER_BASE;

extern int linux_printf(const char *fmt, ...);

#define printf linux_printf

uint64_t ECMainRtcTimerContext = 0ULL;

static timer_event_t * ecmain_timer_list_head = NULL;

static void ecmain_rtimer_set_timeout( timer_event_t *obj );

static bool ecmain_rtimer_exists( timer_event_t *obj );

static timer_event_t master_thread_timer;

int is_enable = 0;

uint64_t last_master_us = 0;

extern uint64_t maincore_arm64_arch_timer_count(void);

uint64_t maincore_perf_cur_ns(void)
{
  unsigned long long temp;
  uint64_t old_cnt;
  uint64_t cur_cnt;
  
  old_cnt = 0;
  
  cur_cnt = maincore_arm64_arch_timer_count();
  
  temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
  
  return temp;
}

uint64_t maincore_get_delay(uint64_t old_cnt, uint64_t cur_cnt)
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

void ecmain_rtimer_init( timer_event_t *obj)
{
    obj->Timestamp = 0;
    obj->ReloadValue = 0;
    obj->IsStarted = false;
    obj->IsNext2Expire = false;
    obj->Next = NULL;
    nxsem_init( &(obj->ec_usleep_sem), 0, 0);
}

static void ecmain_rtimer_reload(void)
{
    // Start the next timer_list_head if it exists AND NOT running
    if( ( ecmain_timer_list_head != NULL ) && ( ecmain_timer_list_head->IsNext2Expire == false ) )
    {
        ecmain_rtimer_set_timeout( ecmain_timer_list_head );
    }
}

uint64_t ecmain_rtc_set_timer_context( void )
{
    ECMainRtcTimerContext = maincore_perf_cur_ns();
    
    return (ECMainRtcTimerContext);
}

/*!
 * \brief Gets the RTC timer reference
 *
 * \param none
 * \retval timerValue In ticks
 */
uint64_t ecmain_rtc_get_timer_context( void )
{
    return (ECMainRtcTimerContext);
}

uint32_t ecmain_rtc_get_timer_elapsed_time( void )
{  
  uint64_t calendarValue = maincore_perf_cur_ns();

  return ( calendarValue - ECMainRtcTimerContext ) ;
}

void ecmain_rtimer_start( timer_event_t *obj )
{
    uint32_t elapsedTime = 0;
    timer_event_t **target = &ecmain_timer_list_head;
    irqstate_t flags;

    flags = enter_critical_section();

    if( ( obj == NULL ) || ( ecmain_rtimer_exists( obj ) == true ) )
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
    if( ecmain_timer_list_head == NULL )
    {
        ecmain_rtc_set_timer_context( );
    }
    else
    {   
        elapsedTime = ecmain_rtc_get_timer_elapsed_time( );
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
    ecmain_rtimer_reload();
    
    leave_critical_section(flags);
}

void ecmain_rtc_stop_alarm( void )
{
	putreg32( 0, g_ec_timer_base + TIMER_CTRL);
}

void ecmain_rtimer_stop( timer_event_t *obj )
{
	irqstate_t flags;
	
    flags = enter_critical_section();
  
    timer_event_t **target = &ecmain_timer_list_head;
    
    // List is empty or the obj to stop does not exist
    if( ( ecmain_timer_list_head == NULL ) || ( obj == NULL ) )
    {
        leave_critical_section(flags);
        
        return;
    }
    
    obj->IsStarted = false;
    
    // Stop the Head and RTC Timeout
    if( ecmain_timer_list_head == obj ) 
    {
        // The head is already running
        if( ecmain_timer_list_head->IsNext2Expire == true ) 
        {
            ecmain_timer_list_head->IsNext2Expire = false;
            if( ecmain_timer_list_head->Next != NULL )
            {
                ecmain_timer_list_head = ecmain_timer_list_head->Next;
                ecmain_rtimer_set_timeout( ecmain_timer_list_head );
            }
            else
            {
                ecmain_rtc_stop_alarm( );
                
                ecmain_timer_list_head = NULL;
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

static bool ecmain_rtimer_exists( timer_event_t *obj )
{
    timer_event_t* cur = ecmain_timer_list_head;

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

void ecmain_rtimer_reset( timer_event_t *obj )
{
    ecmain_rtimer_stop( obj );
    ecmain_rtimer_start( obj );
}

uint32_t ecmain_rtc_get_minimum_timeout( void )
{
    return( MIN_ALARM_DELAY );
}

void ecmain_rtimer_set_value( timer_event_t *obj, uint64_t us )
{
    uint64_t minValue = 0;
    uint64_t ns = 0;

	ecmain_rtimer_stop( obj );

    minValue = ecmain_rtc_get_minimum_timeout( );

    if( us < minValue )
    {
        us = minValue;
    }
    
    ns = (us * 1000ULL);

    obj->Timestamp = ns;
    obj->ReloadValue = ns;
}

static void ecmain_rtimer_set_timeout( timer_event_t *obj )
{
    uint64_t minTicks = ecmain_rtc_get_minimum_timeout( );
    uint64_t ns = 0;
    uint64_t cycles = 0;
    uint32_t load_count0, load_count1;
    
    obj->IsNext2Expire = true;
    
    ns = (minTicks * 1000ULL);

    // In case deadline too soon
    if( obj->Timestamp  < ( ecmain_rtc_get_timer_elapsed_time( ) + ns ) )
    {
        obj->Timestamp = ecmain_rtc_get_timer_elapsed_time( ) + ns;
    }
    
    putreg32( 0, g_ec_timer_base + TIMER_CTRL);
    
    cycles = (obj->Timestamp) / 41;
					
	load_count1 = cycles >> 32;
	load_count0 = cycles & 0xffffffffL;
	
	putreg32(load_count0, g_ec_timer_base + TIMER_LOAD_COUNT0);
    putreg32(load_count1, g_ec_timer_base + TIMER_LOAD_COUNT1);
  		
	putreg32(TIMER_CLR_INT, g_ec_timer_base + TIMER_INTSTATUS);
	putreg32( TIMER_EN | TIMER_INT_EN | TIMER_UD, g_ec_timer_base +  TIMER_CTRL );
}

int ecmain_timer_isr(int irq, void *regs, void *arg)
{
    timer_event_t** target = &ecmain_timer_list_head;
    timer_event_t* cur;
    
    putreg32( TIMER_CLR_INT, g_ec_timer_base + TIMER_INTSTATUS);
    
    if(!is_enable)
    {
		return 0;
	}	

    uint32_t old =  ecmain_rtc_get_timer_context( );
    uint32_t now =  ecmain_rtc_set_timer_context( );
    // intentional wrap around
    uint32_t deltaContext = now - old; 

    // Update timeStamp based upon new Time Reference
    // because delta context should never exceed 2^32
    if( ecmain_timer_list_head != NULL )
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
        cur = ecmain_timer_list_head;
        ecmain_timer_list_head = ecmain_timer_list_head->Next;
        cur->IsStarted = false;
        
        nxsem_post( &(cur->ec_usleep_sem) );
    }

    // Remove all the expired object from the list
    while( ( ecmain_timer_list_head != NULL ) && ( ecmain_timer_list_head->Timestamp < ecmain_rtc_get_timer_elapsed_time( ) ) )
    {
        cur = ecmain_timer_list_head;
        ecmain_timer_list_head = ecmain_timer_list_head->Next;
        cur->IsStarted = false;
        
        nxsem_post( &(cur->ec_usleep_sem) );
    }

    // Start the next timer_list_head if it exists AND NOT running
    ecmain_rtimer_reload();
    
    return OK;
}

void ecmain_ec_timer_init(void)
{
	ecmain_rtimer_init( &master_thread_timer);
}	

uint64_t ecmain_get_ec_timer_count(void)
{
	uint32_t cur_count0, cur_count1;
	uint64_t tmp;
	
	cur_count0 = getreg32( g_ec_timer_base + TIMER_CURRENT_VALUE0 );
	
	cur_count1 = getreg32( g_ec_timer_base + TIMER_CURRENT_VALUE1 );
	
	tmp = cur_count0 + (((uint64_t)cur_count1) << 32);
	
	return tmp;
}

uint64_t ecmain_get_ec_timer_compare(void)
{
	uint32_t load_count0, load_count1;
	uint64_t tmp;
	
	load_count0 = getreg32( g_ec_timer_base + TIMER_LOAD_COUNT0 );
	
	load_count1 = getreg32( g_ec_timer_base + TIMER_LOAD_COUNT1 );
	
	tmp = load_count0 + (((uint64_t)load_count1) << 32);
	
	return tmp;
}

extern void append_sleep_lat(uint16_t idx, uint64_t ns);

void ecmain_timer_usleep(uint64_t us)
{
	/*uint64_t old_cnt;
  uint64_t cur_cnt;
  uint64_t temp;
 
  old_cnt = maincore_arm64_arch_timer_count();*/
 
  if(!is_enable)
  {
	  is_enable = 1;
	  
	  /*up_enable_irq(ECMAIN_TIMER_IRQ);*/
  }
  
  if(us != last_master_us)
  {
		ecmain_rtimer_set_value( &master_thread_timer, us);
		  
		last_master_us = us;
  }
	  
  ecmain_rtimer_start( &master_thread_timer );
	  
  nxsem_wait( &(master_thread_timer.ec_usleep_sem) );
  
  /*cur_cnt = maincore_arm64_arch_timer_count();
  
  temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
    
  temp -= (us * 1000);
  
  append_sleep_lat(1, temp);*/
}

void ecmain_timer_irq_init(void)
{
	irq_attach(ECMAIN_TIMER_IRQ, ecmain_timer_isr, NULL);
			
	/*up_disable_irq(ECMAIN_TIMER_IRQ);*/
}	
