#ifndef __ARCH_ARM64_INCLUDE_RK3588_IRQ_H
#define __ARCH_ARM64_INCLUDE_RK3588_IRQ_H

#define GIC_IRQS_NR			(455)
#define GPIO_IRQS_NR			(5 * 32)


#define NR_IRQS  (GIC_IRQS_NR + GPIO_IRQS_NR)

#define A64_IRQ_SGI0            (0)   /* 0x0000 SGI 0 interrupt */
#define A64_IRQ_SGI1            (1)   /* 0x0004 SGI 1 interrupt */
#define A64_IRQ_SGI2            (2)   /* 0x0008 SGI 2 interrupt */
#define A64_IRQ_SGI3            (3)   /* 0x000C SGI 3 interrupt */
#define A64_IRQ_SGI4            (4)   /* 0x0010 SGI 4 interrupt */
#define A64_IRQ_SGI5            (5)   /* 0x0014 SGI 5 interrupt */
#define A64_IRQ_SGI6            (6)   /* 0x0018 SGI 6 interrupt */
#define A64_IRQ_SGI7            (7)   /* 0x001C SGI 7 interrupt */
#define A64_IRQ_SGI8            (8)   /* 0x0020 SGI 8 interrupt */
#define A64_IRQ_SGI9            (9)   /* 0x0024 SGI 9 interrupt */
#define A64_IRQ_SGI10           (10)  /* 0x0028 SGI 10 interrupt */
#define A64_IRQ_SGI11           (11)  /* 0x002C SGI 11 interrupt */
#define A64_IRQ_SGI12           (12)  /* 0x0030 SGI 12 interrupt */
#define A64_IRQ_SGI13           (13)  /* 0x0034 SGI 13 interrupt */
#define A64_IRQ_SGI14           (14)  /* 0x0038 SGI 14 interrupt */
#define A64_IRQ_SGI15           (15)  /* 0x003C SGI 15 interrupt */

#define A64_IRQ_UART0           (363)

#define A64_IRQ_TIMER3          (324)

#endif
