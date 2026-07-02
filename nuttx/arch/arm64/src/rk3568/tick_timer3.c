/****************************************************************************
 * arch/arm64/src/common/arm64_arch_timer.c
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
#include <debug.h>
#include <assert.h>
#include <stdio.h>

#include <nuttx/arch.h>
#include <arch/irq.h>
#include <arch/chip/chip.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/arch_alarm.h>

#include "../common/arm64_arch.h"
#include "../common/arm64_internal.h"
#include "../common/arm64_gic.h"
/*#include "../common/arm64_arch_timer.h"*/


#define TIMER_BASE      0xFE5F0060 

#define TIMER_CTRL		0x10
#define TIMER_LOAD_COUNT0	0x00
#define TIMER_LOAD_COUNT1	0x04
#define TIMER_CURRENT_VALUE0	0x08
#define TIMER_CURRENT_VALUE1	0x0C
#define TIMER_INTSTATUS		0x18

#define MYBIT(x) (1<<(x))

#define TIMER_EN		MYBIT(0)
#define TIMER_INT_EN		MYBIT(2)
#define TIMER_CLR_INT		MYBIT(0)

#define TIMER_IRQ 144

#define PERF_TIMER_IRQ 145

#define EC_TIMER_IRQ 146

#define COUNTER_FREQUENCY		24000000

#define CONFIG_ARM_TIMER_VIRTUAL_IRQ        (GIC_PPI_INT_BASE + 11)
#define ARM_ARCH_TIMER_IRQ     CONFIG_ARM_TIMER_VIRTUAL_IRQ

uint32_t g_base = TIMER_BASE;

uint64_t volatile jiffies = 0;

extern int linux_printf(const char *fmt, ...);

uint64_t tick_to_reach = 0ULL;

uint32_t g_dummy_timer_base = 0xFE5F00A0;

uint64_t g_cycle_per_tick = 0ULL;


/****************************************************************************
 * Private Types
 ****************************************************************************/

struct arm64_oneshot_lowerhalf_s
{
  /* This is the part of the lower half driver that is visible to the upper-
   * half client of the driver.  This must be the first thing in this
   * structure so that pointers to struct oneshot_lowerhalf_s are cast
   * compatible to struct arm64_oneshot_lowerhalf_s and vice versa.
   */

  struct oneshot_lowerhalf_s lh;      /* Common lower-half driver fields */

  /* Private lower half data follows */

  void *arg;                          /* Argument that is passed to the handler */
  uint64_t cycle_per_tick;            /* cycle per tick */
  oneshot_callback_t callback;        /* Internal handler that receives callback */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline void arm64_arch_timer_set_compare(uint64_t value)
{
	uint32_t load_count0, load_count1;
	
	load_count0 = (uint32_t)(value);
	load_count1 = (uint32_t)(value >> 32);
	
	putreg32( load_count0, g_base + TIMER_LOAD_COUNT0);
	
	putreg32( load_count1, g_base + TIMER_LOAD_COUNT1);
}

static inline uint64_t arm64_arch_timer_get_compare(void)
{
	uint32_t load_count0, load_count1;
	uint64_t tmp;
	
	load_count0 = getreg32( g_base + TIMER_LOAD_COUNT0 );
	
	load_count1 = getreg32( g_base + TIMER_LOAD_COUNT1 );
	
	tmp = load_count0 + (((uint64_t)load_count1) << 32);
	
	return tmp;
}

static inline void arm64_arch_timer_enable(bool enable)
{
  uint32_t value;

  value = getreg32(g_base +  TIMER_CTRL);

  if (enable)
  {
      value |= TIMER_EN;
  }
  else
  {
      value &= ~TIMER_EN;
  }
    
  putreg32( value, g_base +  TIMER_CTRL);
}

static inline void arm64_arch_timer_set_irq_mask(bool mask)
{
  uint32_t value;

  value = getreg32(g_base +  TIMER_CTRL);

  if (mask)
  {      
      value |= TIMER_INT_EN;
  }
  else
  {
      value &= ~TIMER_INT_EN;
  }

  putreg32( value, g_base +  TIMER_CTRL);
}

uint64_t arm64_arch_timer_count(void)
{
	uint32_t cur_count0, cur_count1;
	uint64_t tmp;
	
	cur_count0 = getreg32( g_base + TIMER_CURRENT_VALUE0 );
	
	cur_count1 = getreg32( g_base + TIMER_CURRENT_VALUE1 );
	
	tmp = cur_count0 + (((uint64_t)cur_count1) << 32);
	
	return tmp;
}

static inline uint64_t arm64_arch_timer_get_cntfrq(void)
{
	return COUNTER_FREQUENCY;
}

/****************************************************************************
 * Name: arm64_arch_timer_compare_isr
 *
 * Description:
 *   Common timer interrupt callback.  When any oneshot timer interrupt
 *   expires, this function will be called.  It will forward the call to
 *   the next level up.
 *
 * Input Parameters:
 *   oneshot - The state associated with the expired timer
 *
 * Returned Value:
 *   Always returns OK
 *
 ****************************************************************************/
 
static void arm64_start_next(struct arm64_oneshot_lowerhalf_s *priv)
{
	putreg32(0, g_base + TIMER_CTRL);

	/*arm64_arch_timer_set_compare(arm64_arch_timer_count() + priv->cycle_per_tick);*/
	
	arm64_arch_timer_set_compare(priv->cycle_per_tick);
  
	putreg32( TIMER_CLR_INT, g_base +  TIMER_INTSTATUS);
  
	putreg32( TIMER_EN | TIMER_INT_EN, g_base +  TIMER_CTRL );	
}

int volatile is_in_arch_isr = 0;

static int arm64_arch_timer_compare_isr(int irq, void *regs, void *arg)
{
  struct arm64_oneshot_lowerhalf_s *priv =
    (struct arm64_oneshot_lowerhalf_s *)arg;
    
  is_in_arch_isr = 1;
 
  putreg32( TIMER_CLR_INT, g_base + TIMER_INTSTATUS);
    
  ++jiffies;
  
  if(tick_to_reach)
  {
	  --tick_to_reach;
	  
	  if(!tick_to_reach)
	  {
			if (priv->callback)
			{
				/* Then perform the callback */

				priv->callback(&priv->lh, priv->arg);
				
				is_in_arch_isr = 0;
				
				return OK;
			}	
	  }		
  }
  
  arm64_start_next(priv);
  
  is_in_arch_isr = 0;

  return OK;
}

extern uint32_t g_perf_timer_base;

static int perf_timer_isr(int irq, void *regs, void *arg)
{
  putreg32( TIMER_CLR_INT, g_dummy_timer_base +  TIMER_INTSTATUS);

  return OK;
}

/****************************************************************************
 * Name: arm64_tick_max_delay
 *
 * Description:
 *   Determine the maximum delay of the one-shot timer (in microseconds)
 *
 * Input Parameters:
 *   lower   An instance of the lower-half oneshot state structure.  This
 *           structure must have been previously initialized via a call to
 *           oneshot_initialize();
 *   ticks   The location in which to return the maximum delay.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned
 *   on failure.
 *
 ****************************************************************************/

static int arm64_tick_max_delay(struct oneshot_lowerhalf_s *lower,
                                clock_t *ticks)
{
  DEBUGASSERT(ticks != NULL);

  *ticks = (clock_t)UINT64_MAX;

  return OK;
}

/****************************************************************************
 * Name: arm64_tick_cancel
 *
 * Description:
 *   Cancel the oneshot timer and return the time remaining on the timer.
 *
 *   NOTE: This function may execute at a high rate with no timer running (as
 *   when pre-emption is enabled and disabled).
 *
 * Input Parameters:
 *   lower   Caller allocated instance of the oneshot state structure.  This
 *           structure must have been previously initialized via a call to
 *           oneshot_initialize();
 *   ticks   The location in which to return the time remaining on the
 *           oneshot timer.
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  A call to up_timer_cancel() when
 *   the timer is not active should also return success; a negated errno
 *   value is returned on any failure.
 *
 ****************************************************************************/

static int arm64_tick_cancel(struct oneshot_lowerhalf_s *lower,
                             clock_t *ticks)
{
  struct arm64_oneshot_lowerhalf_s *priv =
    (struct arm64_oneshot_lowerhalf_s *)lower;

  DEBUGASSERT(priv != NULL && ticks != NULL);

  /* Disable int */

  /*arm64_arch_timer_set_irq_mask(false);*/

  return OK;
}

/****************************************************************************
 * Name: arm64_tick_start
 *
 * Description:
 *   Start the oneshot timer
 *
 * Input Parameters:
 *   lower    An instance of the lower-half oneshot state structure.  This
 *            structure must have been previously initialized via a call to
 *            oneshot_initialize();
 *   handler  The function to call when when the oneshot timer expires.
 *   arg      An opaque argument that will accompany the callback.
 *   ticks    Provides the duration of the one shot timer.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned
 *   on failure.
 *
 ****************************************************************************/

static int arm64_tick_start(struct oneshot_lowerhalf_s *lower,
                            oneshot_callback_t callback, void *arg,
                            clock_t ticks)
{
  struct arm64_oneshot_lowerhalf_s *priv =
    (struct arm64_oneshot_lowerhalf_s *)lower;

  DEBUGASSERT(priv != NULL && callback != NULL);

  /* Save the new handler and its argument */

  priv->callback = callback;
  priv->arg = arg;
  
  tick_to_reach = ticks;
  
  arm64_start_next(priv);

  return OK;
}

/****************************************************************************
 * Name: arm64_tick_current
 *
 * Description:
 *  Get the current time.
 *
 * Input Parameters:
 *   lower   Caller allocated instance of the oneshot state structure.  This
 *           structure must have been previously initialized via a call to
 *           oneshot_initialize();
 *   ticks   The location in which to return the current time.
 *
 * Returned Value:
 *   Zero (OK) is returned on success, a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

static int arm64_tick_current(struct oneshot_lowerhalf_s *lower,
                              clock_t *ticks)
{
  struct arm64_oneshot_lowerhalf_s * __attribute__((unused)) priv =
    (struct arm64_oneshot_lowerhalf_s *)lower;
  
  DEBUGASSERT(ticks != NULL);
  
  *ticks = jiffies; 
  
  return OK;
}

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct oneshot_operations_s g_oneshot_ops =
{
  .tick_start     = arm64_tick_start,
  .tick_current   = arm64_tick_current,
  .tick_max_delay = arm64_tick_max_delay,
  .tick_cancel    = arm64_tick_cancel,
};

/****************************************************************************
 * Name: oneshot_initialize
 *
 * Description:
 *   Initialize the oneshot timer and return a oneshot lower half driver
 *   instance.
 *
 * Returned Value:
 *   On success, a non-NULL instance of the oneshot lower-half driver is
 *   returned.  NULL is return on any failure.
 *
 ****************************************************************************/
 
extern int ec_timer_isr(int irq, void *regs, void *arg);

extern int dummy_ec_timer_isr(int irq, void *regs, void *arg);


void my_arm64_arch_timer_set_compare(uint64_t value)
{
  write_sysreg(value, cntv_cval_el0);
}

uint64_t my_arm64_arch_timer_get_compare(void)
{
  return read_sysreg(cntv_cval_el0);
}

void my_arm64_arch_timer_enable(bool enable)
{
  uint64_t value;

  value = read_sysreg(cntv_ctl_el0);

  if (enable)
    {
      value |= CNTV_CTL_ENABLE_BIT;
    }
  else
    {
      value &= ~CNTV_CTL_ENABLE_BIT;
    }

  write_sysreg(value, cntv_ctl_el0);
}

void my_arm64_arch_timer_set_irq_mask(bool mask)
{
  uint64_t value;

  value = read_sysreg(cntv_ctl_el0);

  if (mask)
    {
      value |= CNTV_CTL_IMASK_BIT;
    }
  else
    {
      value &= ~CNTV_CTL_IMASK_BIT;
    }

  write_sysreg(value, cntv_ctl_el0);
}

uint64_t my_arm64_arch_timer_count(void)
{
  return read_sysreg(cntvct_el0);
}

uint64_t my_arm64_arch_timer_get_cntfrq(void)
{
  return read_sysreg(cntfrq_el0);
}

/****************************************************************************
 * Name: arm64_arch_timer_compare_isr
 *
 * Description:
 *   Common timer interrupt callback.  When any oneshot timer interrupt
 *   expires, this function will be called.  It will forward the call to
 *   the next level up.
 *
 * Input Parameters:
 *   oneshot - The state associated with the expired timer
 *
 * Returned Value:
 *   Always returns OK
 *
 ****************************************************************************/

static int my_arm64_arch_timer_compare_isr(int irq, void *regs, void *arg)
{
  my_arm64_arch_timer_set_irq_mask(true);

  return OK;
}

static struct oneshot_lowerhalf_s *arm64_oneshot_initialize(void)
{
  struct arm64_oneshot_lowerhalf_s *priv;

  tmrinfo("oneshot_initialize\n");

  /* Allocate an instance of the lower half driver */

  priv = (struct arm64_oneshot_lowerhalf_s *)
    kmm_zalloc(sizeof(struct arm64_oneshot_lowerhalf_s));

  if (priv == NULL)
  {
      tmrerr("ERROR: Failed to initialized state structure\n");

      return NULL;
  }

  /* Initialize the lower-half driver structure */

  priv->lh.ops = &g_oneshot_ops;
  /*priv->cycle_per_tick = arm64_arch_timer_get_cntfrq() / TICK_PER_SEC;*/
  
  priv->cycle_per_tick = (24390L * 1000L) / TICK_PER_SEC;
  
  g_cycle_per_tick = priv->cycle_per_tick;
  
  tmrinfo("cycle_per_tick %" PRIu64 "\n", priv->cycle_per_tick);
  
  tick_to_reach = 0ULL;

  /* Attach handler */

  irq_attach(TIMER_IRQ,
             arm64_arch_timer_compare_isr, priv);

  /* Enable int */

  up_enable_irq(TIMER_IRQ);
  
  irq_attach(PERF_TIMER_IRQ,
			ec_timer_isr, NULL);
			
  up_disable_irq(PERF_TIMER_IRQ);
  
  irq_attach(EC_TIMER_IRQ,
			perf_timer_isr, NULL);
			
  up_disable_irq(EC_TIMER_IRQ);
  
  /* Start timer */
  arm64_arch_timer_enable(true);
  
  irq_attach(ARM_ARCH_TIMER_IRQ,
             my_arm64_arch_timer_compare_isr, NULL);

  /* Enable int */

  up_disable_irq(ARM_ARCH_TIMER_IRQ);
  
  my_arm64_arch_timer_enable(true);
    
  my_arm64_arch_timer_set_compare(0xffffffffffffffffULL);
  
  my_arm64_arch_timer_set_irq_mask(false);

  tmrinfo("oneshot_initialize ok %p \n", &priv->lh);

  return &priv->lh;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize the system timer
 *   interrupt.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint64_t freq;

  freq = arm64_arch_timer_get_cntfrq();
  tmrinfo("%s: cp15 timer(s) running at %" PRIu64 ".%" PRIu64 "MHz\n",
          __func__, freq / 1000000, (freq / 10000) % 100);

  up_alarm_set_lowerhalf(arm64_oneshot_initialize());
}

#ifdef CONFIG_SMP
/****************************************************************************
 * Function:  arm64_arch_timer_secondary_init
 *
 * Description:
 *   This function is called during start-up to initialize the system timer
 *   interrupt for smp.
 *
 * Notes:
 * The origin design for ARMv8-A timer is assigned private timer to
 * every PE(CPU core), the ARM_ARCH_TIMER_IRQ is a PPI so it's
 * should be enable at every core.
 *
 * But for NuttX, it's design only for primary core to handle timer
 * interrupt and call nxsched_process_timer at timer tick mode.
 * So we need only enable timer for primary core
 *
 * IMX6 use GPT which is a SPI rather than generic timer to handle
 * timer interrupt
 ****************************************************************************/

void arm64_arch_timer_secondary_init(void)
{
//#ifdef CONFIG_SCHED_TICKLESS
#if 0
  tmrinfo("arm64_arch_timer_secondary_init\n");

  /* Enable int */

  up_enable_irq(TIMER_IRQ);

  /* Start timer */

  arm64_arch_timer_enable(true);
#endif
}
#endif

