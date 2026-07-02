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
#include "../common/arm64_arch_timer.h"

#define CONFIG_ARM_TIMER_VIRTUAL_IRQ        (GIC_PPI_INT_BASE + 11)
#define ARM_ARCH_TIMER_IRQ     CONFIG_ARM_TIMER_VIRTUAL_IRQ

void slavecore_arm64_arch_timer_set_compare(uint64_t value)
{
  write_sysreg(value, cntv_cval_el0);
}

uint64_t slavecore_arm64_arch_timer_get_compare(void)
{
  return read_sysreg(cntv_cval_el0);
}

void slavecore_arm64_arch_timer_enable(bool enable)
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

void slavecore_arm64_arch_timer_set_irq_mask(bool mask)
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

uint64_t slavecore_arm64_arch_timer_count(void)
{
  return read_sysreg(cntvct_el0);
}

uint64_t slavecore_arm64_arch_timer_get_cntfrq(void)
{
  return read_sysreg(cntfrq_el0);
}

static int slavecore_arm64_arch_timer_compare_isr(int irq, void *regs, void *arg)
{
  slavecore_arm64_arch_timer_set_irq_mask(true);

  return OK;
}

void slavecore_arm64_arch_timer_init(void)
{
	irq_attach(ARM_ARCH_TIMER_IRQ,
             slavecore_arm64_arch_timer_compare_isr, NULL);
             
	up_disable_irq(ARM_ARCH_TIMER_IRQ);
  
	slavecore_arm64_arch_timer_enable(true);
    
	slavecore_arm64_arch_timer_set_compare(0xffffffffffffffffULL);
  
	slavecore_arm64_arch_timer_set_irq_mask(false);
}	
