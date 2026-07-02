#ifndef __RTONBOOT_H__
#define __RTONBOOT_H__

#include "offset_common.h"

#define CONFIG_RTOSCPU_STACKSIZE (128L * 1024L)

#define CONFIG_RTOSCPU_ELX_STACKSIZE (64L * 1024L)

#define CONFIG_RTOSSLAVE_STACKSIZE (128L * 1024L)

#define CONFIG_RTOSSLAVE_ELX_STACKSIZE (64L * 1024L)

#define CONFIG_RTOS_CPUID 1

#define CONFIG_DEBUG_ADDR 0xecc00108

#define CONFIG_RTONBOOT_PGTABLE_SIZE 0x20000

#define OFFSET_TO_FRAMEBUF0_PTR 0x310

#ifndef __ASSEMBLY__

typedef void (*PF_RTONBOOTENTRY)(void);  

extern uint64_t RTONBOOT_IMG_LOAD_ADDR;

extern u8 * RTOBOOT_LOAD_PTR;

extern int rtos_serial_init(void);

extern void rtos_serial_putc(int ch);

extern int rtos_serial_getc(void);

extern void rtos_serial_puts(const char *s);

extern int rtos_printf(const char *fmt, ...);

extern int rockchip_debugger_init(void);

extern int read_and_load_rtonboot_image(void);

extern int reserve_rtos_buffers(void);

extern int load_init_and_jump_to_rtonboot(void);

extern void jump_to_rtonboot(void);

#endif

#endif
