/*
 * Rockchip timer support
 *
 * Copyright (C) Daniel Lezcano <daniel.lezcano@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched_clock.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#define TIMER_NAME "ecmaintimer"

#define TIMER_LOAD_COUNT0	0x00
#define TIMER_LOAD_COUNT1	0x04
#define TIMER_CURRENT_VALUE0	0x08
#define TIMER_CURRENT_VALUE1	0x0C
#define TIMER_CONTROL_REG3288	0x10
#define TIMER_CONTROL_REG3399	0x1c
#define TIMER_INT_STATUS	0x18

#define TIMER_DISABLE		0x0
#define TIMER_ENABLE		0x1
#define TIMER_MODE_FREE_RUNNING			(0 << 1)
#define TIMER_MODE_USER_DEFINED_COUNT		(1 << 1)
#define TIMER_INT_UNMASK			(1 << 2)

struct rk_timer {
	void __iomem *base;
	void __iomem *ctrl;
	struct clk *clk;
	struct clk *pclk;
	u32 freq;
	int irq;
};

struct rk_clkevt {
	struct clock_event_device ce;
	struct rk_timer timer;
};

static struct rk_clkevt *rk_clkevt;
#ifndef MODULE
static struct rk_timer *rk_clksrc;
#endif

static inline struct rk_timer * ecmain_timer(struct clock_event_device *ce)
{
	return &container_of(ce, struct rk_clkevt, ce)->timer;
}

static inline void ecmain_timer_disable(struct rk_timer *timer)
{
	writel_relaxed(TIMER_DISABLE, timer->ctrl);
}

static inline void ecmain_timer_enable(struct rk_timer *timer, u32 flags)
{
	writel_relaxed(TIMER_ENABLE | flags, timer->ctrl);
}

static void ecmain_timer_update_counter(unsigned long cycles,
				    struct rk_timer *timer)
{
	writel_relaxed(cycles, timer->base + TIMER_LOAD_COUNT0);
	writel_relaxed(0, timer->base + TIMER_LOAD_COUNT1);
}

static void ecmain_timer_interrupt_clear(struct rk_timer *timer)
{
	writel_relaxed(1, timer->base + TIMER_INT_STATUS);
}

static inline int ecmain_timer_set_next_event(unsigned long cycles,
					  struct clock_event_device *ce)
{
	struct rk_timer *timer = ecmain_timer(ce);

	ecmain_timer_disable(timer);
	ecmain_timer_update_counter(cycles, timer);
	ecmain_timer_enable(timer, TIMER_MODE_USER_DEFINED_COUNT |
			       TIMER_INT_UNMASK);
	return 0;
}

static int ecmain_timer_shutdown(struct clock_event_device *ce)
{
	struct rk_timer *timer = ecmain_timer(ce);

	ecmain_timer_disable(timer);
	return 0;
}

static int ecmain_timer_set_periodic(struct clock_event_device *ce)
{
	struct rk_timer *timer = ecmain_timer(ce);

	ecmain_timer_disable(timer);
	ecmain_timer_update_counter(timer->freq / HZ - 1, timer);
	ecmain_timer_enable(timer, TIMER_MODE_FREE_RUNNING | TIMER_INT_UNMASK);
	return 0;
}

#if 0

static irqreturn_t ecmain_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *ce = dev_id;
	struct rk_timer *timer = ecmain_timer(ce);

	ecmain_timer_interrupt_clear(timer);

	if (clockevent_state_oneshot(ce))
		ecmain_timer_disable(timer);

	ce->event_handler(ce);

	return IRQ_HANDLED;
}

#endif

#if 0

#ifndef MODULE

static u64 notrace ecmain_timer_sched_read(void)
{
	return ~readl_relaxed(rk_clksrc->base + TIMER_CURRENT_VALUE0);
}

#endif

#endif

#if defined (CONFIG_RTONBOOT_SUPPORT)

extern int rtonboot_gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val,
			    bool force);

#endif

static int __init
ecmain_timer_probe(struct rk_timer *timer, struct device_node *np)
{
	struct clk *timer_clk;
	struct clk *pclk;
	int ret = -EINVAL, irq;
	u32 ctrl_reg = TIMER_CONTROL_REG3288;
	int err;

    struct resource res;

	timer->base = of_iomap(np, 0);
	if (!timer->base) {
		pr_err("Failed to get base address for '%s'\n", TIMER_NAME);
		return -ENXIO;
	}

	if (of_device_is_compatible(np, "rockchip,rk3399-ecmaintimer"))
		ctrl_reg = TIMER_CONTROL_REG3399;

	timer->ctrl = timer->base + ctrl_reg;

	pclk = of_clk_get_by_name(np, "pclk");
	if (IS_ERR(pclk)) {
		ret = PTR_ERR(pclk);
		pr_err("Failed to get pclk for '%s'\n", TIMER_NAME);
		goto out_unmap;
	}

	ret = clk_prepare_enable(pclk);
	if (ret) {
		pr_err("Failed to enable pclk for '%s'\n", TIMER_NAME);
		goto out_unmap;
	}
	timer->pclk = pclk;

	timer_clk = of_clk_get_by_name(np, "timer");
	if (IS_ERR(timer_clk)) {
		ret = PTR_ERR(timer_clk);
		pr_err("Failed to get timer clock for '%s'\n", TIMER_NAME);
		goto out_timer_clk;
	}

	ret = clk_prepare_enable(timer_clk);
	if (ret) {
		pr_err("Failed to enable timer clock\n");
		goto out_timer_clk;
	}
	timer->clk = timer_clk;

	timer->freq = clk_get_rate(timer_clk);

	irq = irq_of_parse_and_map(np, 0);
	if (!irq) {
		ret = -EINVAL;
		pr_err("Failed to map interrupts for '%s'\n", TIMER_NAME);
		goto out_irq;
	}
	timer->irq = irq;

        
	if (of_address_to_resource(np, 0, &res))
	    res.start = 0;

	ecmain_timer_interrupt_clear(timer);

	/*ecmain_timer_disable(timer);*/

	#if defined (CONFIG_RTONBOOT_SUPPORT)

		enable_irq(irq);

		(void)rtonboot_gic_set_affinity(irq_get_irq_data(irq), cpumask_of(CONFIG_RTONBOOT_CPUID), true);

		printk("Winfred enable ecmain dedicated irq %d \n", err);

	#endif

	return 0;

out_irq:
	clk_disable_unprepare(timer_clk);
out_timer_clk:
	clk_disable_unprepare(pclk);
out_unmap:
	iounmap(timer->base);

	return ret;
}

#if 0

static void __init ecmain_timer_cleanup(struct rk_timer *timer)
{
	clk_disable_unprepare(timer->clk);
	clk_disable_unprepare(timer->pclk);
	iounmap(timer->base);
}

#endif

static int __init ecmain_clkevt_init(struct device_node *np)
{
	struct clock_event_device *ce;
	int ret = -EINVAL;

	rk_clkevt = kzalloc(sizeof(struct rk_clkevt), GFP_KERNEL);
	if (!rk_clkevt) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ecmain_timer_probe(&rk_clkevt->timer, np);
	if (ret)
		goto out_probe;

	#if 1

	ce = &rk_clkevt->ce;
	ce->name = TIMER_NAME;
	ce->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT |
		       CLOCK_EVT_FEAT_DYNIRQ;
	ce->set_next_event = ecmain_timer_set_next_event;
	ce->set_state_shutdown = ecmain_timer_shutdown;
	ce->set_state_periodic = ecmain_timer_set_periodic;
	ce->irq = rk_clkevt->timer.irq;
	ce->cpumask = cpu_possible_mask;
	ce->rating = 250;
	
	ecmain_timer_set_periodic(ce);

	#else

	ret = request_irq(rk_clkevt->timer.irq, ecmain_timer_interrupt, IRQF_TIMER,
			  TIMER_NAME, ce);
	if (ret) {
		pr_err("Failed to initialize '%s': %d\n",
			TIMER_NAME, ret);
		goto out_irq;
	}

	clockevents_config_and_register(&rk_clkevt->ce,
					rk_clkevt->timer.freq, 1, UINT_MAX);

	#endif

	goto out_probe;

#if 0

out_irq:
	ecmain_timer_cleanup(&rk_clkevt->timer);

#endif

out_probe:
	kfree(rk_clkevt);
out:
	/* Leave rk_clkevt not NULL to prevent future init */
	/*rk_clkevt = ERR_PTR(ret);
	return ret;*/

	rk_clkevt = NULL;

	return -1;
}

#ifndef MODULE

static int __init ecmain_clksrc_init(struct device_node *np)
{
	int ret = -EINVAL;

	#if 0

	rk_clksrc = kzalloc(sizeof(struct rk_timer), GFP_KERNEL);
	if (!rk_clksrc) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ecmain_timer_probe(rk_clksrc, np);
	if (ret)
		goto out_probe;

	ecmain_timer_update_counter(UINT_MAX, rk_clksrc);
	ecmain_timer_enable(rk_clksrc, 0);

	ret = clocksource_mmio_init(rk_clksrc->base + TIMER_CURRENT_VALUE0,
		TIMER_NAME, rk_clksrc->freq, 250, 32,
		clocksource_mmio_readl_down);
	if (ret) {
		pr_err("Failed to register clocksource\n");
		goto out_clocksource;
	}

	sched_clock_register(ecmain_timer_sched_read, 32, rk_clksrc->freq);

	#endif

	ret = 0;

	return ret;

#if 0

out_clocksource:
	ecmain_timer_cleanup(rk_clksrc);
out_probe:
	kfree(rk_clksrc);
out:
	/* Leave rk_clksrc not NULL to prevent future init */
	rk_clksrc = ERR_PTR(ret);
	return ret;

#endif

}

#endif

static int __init ecmain_timer_init(struct device_node *np)
{
	if (!rk_clkevt)
		return ecmain_clkevt_init(np);

	#ifndef MODULE
	
	if (!rk_clksrc)
		return ecmain_clksrc_init(np);
		
	#endif	

	pr_err("Too many timer definitions for '%s'\n", TIMER_NAME);
	return -EINVAL;
}

TIMER_OF_DECLARE(rk3288_timer, "rockchip,rk3288-ecmaintimer", ecmain_timer_init);
TIMER_OF_DECLARE(rk3399_timer, "rockchip,rk3399-ecmaintimer", ecmain_timer_init);

#ifdef MODULE
static int __init ecmain_timer_driver_probe(struct platform_device *pdev)
{
	return ecmain_timer_init(pdev->dev.of_node);
}

static const struct of_device_id ecmain_timer_match_table[] = {
	{ .compatible = "rockchip,rk3288-ecmaintimer" },
	{ .compatible = "rockchip,rk3399-ecmaintimer" },
	{ /* sentinel */ },
};

static struct platform_driver ecmain_timer_driver = {
	.driver = {
		.name = TIMER_NAME,
		.of_match_table = ecmain_timer_match_table,
	},
};
module_platform_driver_probe(ecmain_timer_driver, ecmain_timer_driver_probe);

MODULE_LICENSE("GPL");
#endif
