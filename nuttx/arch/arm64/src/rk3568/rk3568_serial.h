#ifndef __ARCH_ARM64_SRC_RK3568_RK3568_SERIAL_H
#define __ARCH_ARM64_SRC_RK3568_RK3568_SERIAL_H

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

#ifdef CONFIG_ARCH_CHIP_RK3568

/* IRQ for RK3399 UART Rockchip_RK3399TRM_V1.4_Part1-20170408 page 17 */

#define RK3568_UART4_ADDR      0xfe680000
//#define RK3568_UART4_IRQ       120         /* RK3568 UART4 IRQ */
#define RK3568_UART4_IRQ       152         /* RK3568 UART4 IRQ */

#endif /* CONFIG_ARCH_CHIP_RK3568 */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#endif /* __ASSEMBLY__ */


#endif

