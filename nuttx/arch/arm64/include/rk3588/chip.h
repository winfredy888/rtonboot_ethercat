#ifndef __ARCH_ARM64_INCLUDE_RK3588_CHIP_H
#define __ARCH_ARM64_INCLUDE_RK3588_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Number of bytes in x kibibytes/mebibytes/gibibytes */

#define KB(x)           ((x) << 10)
#define MB(x)           (KB(x) << 10)
#define GB(x)           (MB(UINT64_C(x)) << 10)

#define CONFIG_GICD_BASE			0xfe600000
#define CONFIG_GICR_BASE			0xfe680000
#define CONFIG_GICC_BASE			0xfe600000
#define CONFIG_GICR_OFFSET        0x20000


#define CONFIG_RAMBANK1_ADDR      0xecc00000              
#define CONFIG_RAMBANK1_SIZE      0x01200000
#define CONFIG_DEVICEIO_BASEADDR  0xf0000000
#define CONFIG_DEVICEIO_SIZE      0x10000000

#define CONFIG_LOAD_BASE          0xffffffbebec00000

#define MPID_TO_CLUSTER_ID(mpid)  ((mpid) & ~0xff)

#define CONFIG_RTOS_CPUID 1

/****************************************************************************
 * Assembly Macros
 * ubfx   \xreg0, \xreg0, #8, #16
 ****************************************************************************/

#ifdef __ASSEMBLY__

.macro  get_cpu_id xreg0
	mrs    \xreg0, mpidr_el1
	ubfx   \xreg0, \xreg0, #0, #8
.endm

#endif /* __ASSEMBLY__ */


#endif
