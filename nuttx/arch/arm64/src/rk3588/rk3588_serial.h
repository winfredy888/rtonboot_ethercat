#ifndef __ARCH_ARM64_SRC_RK3588_RK3588_SERIAL_H
#define __ARCH_ARM64_SRC_RK3588_RK3588_SERIAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include "arm64_internal.h"
#include "arm64_gic.h"

#ifndef __ASSEMBLY__

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef CONFIG_ARCH_CHIP_RK3588

/* IRQ for RK3399 UART Rockchip_RK3399TRM_V1.4_Part1-20170408 page 17 */

#define RK3588_UART0_ADDR      0xfd890000
#define RK3588_UART0_IRQ       363         /* RK3588 UART0 IRQ */

#endif /* CONFIG_ARCH_CHIP_RK3568 */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#endif /* __ASSEMBLY__ */


#endif

