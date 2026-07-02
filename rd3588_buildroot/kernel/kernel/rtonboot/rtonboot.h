#ifndef _RTONBOOT_H
#define _RTONBOOT_H

#include "offset_common.h"

#define CONFIG_RTONBOOT_LOAD_ADDR 0xecc00000

#define RTONBOOT_SGI_ID 7

void rtonboot_gic_unmask_rtonboot_ipi(void);

void rtonboot_flush_dcache_range(unsigned long start, unsigned long stop);

#endif
