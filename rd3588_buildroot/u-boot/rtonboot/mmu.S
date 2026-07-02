#include "asm.h"
#include "mmu.h"
#include "asm_macros.h"
#include "vm.h"

.extern testit
.extern testswi
.extern testfail
.extern testreg
.extern MASTER_MAIR_VAL;
.extern MASTER_TCR_FLAGS;
.extern MASTER_TTBR0_EL1;
.extern MASTER_TTBR1_EL1;



/*
 * Register use:
 *  x0-x3   Arguments
 *  x9-x15  Scratch
 *  x19-x28 Globals
 */
tmp                     .req x9

.globl begin_init_mmu
begin_init_mmu:

    bl      arm64_elX_to_el1

    /* disable EL1 FPU traps */
    mov     tmp, #(0b11<<20)
    msr     cpacr_el1, tmp

    mrs     tmp, sctlr_el1
    orr     tmp, tmp, #(1<<12) /* Enable icache */
    orr     tmp, tmp, #(1<<2)  /* Enable dcache/ucache */
    orr     tmp, tmp, #(1<<3)  /* Enable Stack Alignment Check EL1 */
    orr     tmp, tmp, #(1<<4)  /* Enable Stack Alignment Check EL0 */
    bic     tmp, tmp, #(1<<1)  /* Disable Alignment Checking for EL1 EL0 */
    msr     sctlr_el1, tmp

    /* Invalidate TLB */
    tlbi    vmalle1is
    dsb     sy
    isb

    /* Initialize Memory Attribute Indirection Register */
    adr     x5, MASTER_MAIR_VAL
    ldr     x6, [x5]
    msr     mair_el1, x6

    adr     x5,  MASTER_TCR_FLAGS
    ldr     x6,  [x5]
    msr     tcr_el1, x6

    isb

    /* Write ttbr with phys addr of the translation table */

    adr     x5, MASTER_TTBR0_EL1
    ldr     x6, [x5]
    msr     ttbr0_el1, x6

    adr     x5, MASTER_TTBR1_EL1
    ldr     x6, [x5]
    msr     ttbr1_el1, x6
    isb

    /* Set VBAR to the virtual address of the trampoline VBAR */
    adr     tmp, trampoline_vbar
    msr     vbar_el1, tmp
    isb

    /* Read SCTLR */
    mrs     tmp, sctlr_el1

    /* Turn on the MMU */
    orr     tmp, tmp, #0x1

    /*
     * Write back SCTLR. This instruction will cause an exception when fetching
     * the following instruction, as the PC will contain an unmapped physical
     * address. This will be handled by the trampoline VBAR which will branch
     * to that instruction's virtual address.
     */
    msr     sctlr_el1, tmp

.Lmmu_on_pc:

    isb

    /* Disable the trampoline VBAR */
    msr     vbar_el1, xzr
    isb

    /* Invalidate TLB */
    tlbi    vmalle1
    dsb     sy
    isb
    
    /*ldr     x5, =debug_vaddr
	ldr     x6, [x5]
	mov		x7, #0x9999
	str		x7, [x6]
	
    tlbi    vmalle1
    dsb     sy
    isb*/ 

    adr     x5, IMAGE_VADDR_BASE
    ldr     x6, [x5]
    blr	    x6

    b       .
    

.set	MMU_SHARED, (begin_init_mmu + 0x300)

.org(MMU_SHARED)

.globl	IMAGE_VADDR_BASE
IMAGE_VADDR_BASE:
	.quad	CONFIG_IMAGE_VADDR_BASE

/*.globl	MASTER_MAIR_VAL
MASTER_MAIR_VAL:
	.quad	0

.globl	MASTER_TCR_FLAGS
MASTER_TCR_FLAGS:
	.quad	0

.globl	MASTER_TTBR0_EL1
MASTER_TTBR0_EL1:
	.quad	0

.globl	MASTER_TTBR1_EL1
MASTER_TTBR1_EL1:
	.quad	0*/

.globl	testreg
testreg:
	.quad	0

.globl	debug_vaddr
debug_vaddr:
	.quad	CONFIG_DEBUG_VADDR_BASE

.globl	debug_paddr
debug_paddr:
	.quad	0xecc00100
	
.section .text.boot.vectab
LOCAL_FUNCTION(trampoline_vbar)
.p2align 11
.org 0x00
    wfe
    b       .-4

.org 0x80
    wfe
    b       .-4

.org 0x100
    wfe
    b       .-4

.org 0x180
    wfe
    b       .-4

    /* exception vector for synchronous exceptions from current EL -> current EL */
.org 0x200
    b       .Lmmu_on_pc

.org 0x280
    wfe
    b       .-4

.org 0x300
    wfe
    b       .-4

.org 0x380
    wfe
    b       .-4

.org 0x400
    wfe
    b       .-4

.org 0x480
    wfe
    b       .-4

.org 0x500
    wfe
    b       .-4

.org 0x580
    wfe
    b       .-4

.org 0x600
    wfe
    b       .-4

.org 0x680
    wfe
    b       .-4

.org 0x700
    wfe
    b       .-4

.org 0x780
    wfe
    b       .-4
    
END_FUNCTION(trampoline_vbar)

.section .text.boot.vectab_slave
LOCAL_FUNCTION(trampoline_vbar_slave)
.p2align 11
.org 0x00
    wfe
    b       .-4

.org 0x80
    wfe
    b       .-4

.org 0x100
    wfe
    b       .-4

.org 0x180
    wfe
    b       .-4

    /* exception vector for synchronous exceptions from current EL -> current EL */
.org 0x200
    b       .Lmmu_on_pc_slave

.org 0x280
    wfe
    b       .-4

.org 0x300
    wfe
    b       .-4

.org 0x380
    wfe
    b       .-4

.org 0x400
    wfe
    b       .-4

.org 0x480
    wfe
    b       .-4

.org 0x500
    wfe
    b       .-4

.org 0x580
    wfe
    b       .-4

.org 0x600
    wfe
    b       .-4

.org 0x680
    wfe
    b       .-4

.org 0x700
    wfe
    b       .-4

.org 0x780
    wfe
    b       .-4
    
END_FUNCTION(trampoline_vbar_slave)


FUNCTION(arch_clean_cache_range)
    cache_range_op dc cvac         // clean cache to PoC by MVA
    ret
END_FUNCTION(arch_clean_cache_range)

