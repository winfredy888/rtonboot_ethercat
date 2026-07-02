#include <nuttx/config.h>

#include "arm64_macro.inc"

	.file    "rk3568_lowputc.S"
	
#define UART4_BASE_ADDRESS 0xfe680000

GTEXT(arm64_earlyprintinit)
SECTION_FUNC(text, arm64_earlyprintinit)
    ret  /* Do nothing because U-Boot has already initialized UART */

.macro early_uart_ready xb, wt
1:
    ldrh  \wt, [\xb, #0x14]      /* UART_LSR (Line Status Register) */
    tst   \wt, #0x20             /* Check THRE (TX Holding Register Empty) */
    b.eq  1b                     /* Wait for the UART to be ready (THRE=1) */
.endm

.macro early_uart_transmit xb, wt
    strb  \wt, [\xb]             /* UART_THR (Transmit Holding Register) */
.endm

GTEXT(arm64_lowputc)
SECTION_FUNC(text, arm64_lowputc)
    ldr   x15, =UART4_BASE_ADDRESS
    early_uart_ready x15, w2
    early_uart_transmit x15, w0
    ret

GTEXT(__asm_flush_dcache_range)    
SECTION_FUNC(text, __asm_flush_dcache_range)
	mrs	x3, ctr_el0
	lsr	x3, x3, #16
	and	x3, x3, #0xf
	mov	x2, #4
	lsl	x2, x2, x3		/* cache line size */

	/* x2 <- minimal cache line size in cache system */
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	dc	civac, x0	/* clean & invalidate data or unified cache */
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	ret
	
GTEXT(__asm_invalidate_dcache_range)
SECTION_FUNC(text, __asm_invalidate_dcache_range)	
	mrs	x3, ctr_el0
	ubfm	x3, x3, #16, #19
	mov	x2, #4
	lsl	x2, x2, x3		/* cache line size */

	/* x2 <- minimal cache line size in cache system */
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	dc	ivac, x0	/* invalidate data or unified cache */
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	isb
	ret	
	
    
