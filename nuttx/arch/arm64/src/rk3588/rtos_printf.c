#include <nuttx/config.h>
#include <stdarg.h>
#include <stdio.h>

#include <debug.h>
#include <assert.h>

#include "arm64_internal.h"

typedef uint32_t u32;

#define RTOS_UART_BASE 0xfd890000

#define CONFIG_DEBUG_UART_BASE 0xFEB50000

u32 g_rtos_serial_base = RTOS_UART_BASE;

u32 g_linux_serial_base = CONFIG_DEBUG_UART_BASE;

#define UART_LSR_THRE	0x20		/* Xmit holding register empty */
#define UART_LSR_DR	0x01		/* Data ready */

#define CONFIG_16550_REGINCR 4

#define UART_LSR_INCR          5

#define UART_RBR_INCR          0  /* (DLAB =0) Receiver Buffer Register */
#define UART_THR_INCR          0  /* (DLAB =0) Transmit Holding Register */

u32 g_lsr_offset = ( ((u32)UART_LSR_INCR) * ((u32)CONFIG_16550_REGINCR) );

u32 g_rbr_offset = ( ((u32)UART_RBR_INCR) * ((u32)CONFIG_16550_REGINCR) );

u32 g_thr_offset = ( ((u32)UART_THR_INCR) * ((u32)CONFIG_16550_REGINCR) );

void rtos_serial_putc(int ch)
{
	u32 status;
	
	status = getreg32( g_rtos_serial_base + g_lsr_offset);

	while (!(status & UART_LSR_THRE))
	{
		status = getreg32( g_rtos_serial_base + g_lsr_offset);
	};

	if (ch == '\n')
	{
		putreg8('\r', g_rtos_serial_base + g_thr_offset);
		
		status = getreg32( g_rtos_serial_base + g_lsr_offset);

		while (!(status & UART_LSR_THRE))
		{
			status = getreg32( g_rtos_serial_base + g_lsr_offset);
		};
	}
	
	putreg8(ch, g_rtos_serial_base + g_thr_offset);
}

int rtos_serial_getc(void)
{
	u32 status;
	unsigned char ch;
	
	status = getreg32( g_rtos_serial_base + g_lsr_offset);

	while (!(status & UART_LSR_DR))
	{
		status = getreg32( g_rtos_serial_base + g_lsr_offset);
	};

	ch = getreg8( g_rtos_serial_base +  g_rbr_offset );
	
	return ch;
}

void rtos_serial_puts(const char *s)
{
	while (*s)
		rtos_serial_putc(*s++);
}

#define likely(x)       (x)

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int i;

	i = vsnprintf(buf, size, fmt, args);

	if (likely(i < size))
		return i;
	if (size != 0)
		return size - 1;
	return 0;
}

int rtos_printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[512];

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
	va_end(args);

	/* Print the string */
	rtos_serial_puts(printbuffer);
	return i;
}

void linux_serial_putc(int ch)
{
	u32 status;
	
	status = getreg32( g_linux_serial_base + g_lsr_offset);

	while (!(status & UART_LSR_THRE))
	{
		status = getreg32( g_linux_serial_base + g_lsr_offset);
	};

	if (ch == '\n')
	{
		putreg8('\r', g_linux_serial_base + g_thr_offset);
		
		status = getreg32( g_linux_serial_base + g_lsr_offset);

		while (!(status & UART_LSR_THRE))
		{
			status = getreg32( g_linux_serial_base + g_lsr_offset);
		};
	}
	
	putreg8(ch, g_linux_serial_base + g_thr_offset);
}

int linux_serial_getc(void)
{
	u32 status;
	unsigned char ch;
	
	status = getreg32( g_linux_serial_base + g_lsr_offset);

	while (!(status & UART_LSR_DR))
	{
		status = getreg32( g_linux_serial_base + g_lsr_offset);
	};

	ch = getreg8( g_linux_serial_base +  g_rbr_offset );
	
	return ch;
}

void linux_serial_puts(const char *s)
{
	while (*s)
		linux_serial_putc(*s++);
}

int linux_printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[512];

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
	va_end(args);

	/* Print the string */
	linux_serial_puts(printbuffer);
	return i;
}

int linux_vprintf(const char *fmt, va_list ap)
{
	uint i;
	char printbuffer[512];
	
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, ap);
	
	linux_serial_puts(printbuffer);
	
	return i;
}

void testit(void)
{
	char * ptest;
	uint64_t test_val;

	ptest = (char *)((void *)0xffffffbebec00000);

	ptest += 0x100;
	
	*((uint64_t *)ptest) = 0x9999;

	test_val = *((uint64_t *)ptest);
	
	rtos_printf("8888888888888888888888888 %llx\n", test_val);
}

void test2(uint64_t regval)
{
	rtos_printf("7777777777777777777777777777777777777777777\n");
}
