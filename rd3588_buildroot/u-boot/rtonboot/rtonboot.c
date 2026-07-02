#include <irq-generic.h>
#include <rk_timer_irq.h>
#include <asm/io.h>

#include <common.h>
#include <mapmem.h>

#include <blk.h>
#include <mmc.h>
#include <dm/device.h>

#include <boot_rkimg.h>

#include <asm/arch/rockchip_smccc.h>

#include <rtonboot/rtonboot.h>

#include "jihuoma.h"

#define LOADER_HARD_STR			"LOADER"

#define LOADER_MAGIC_SIZE		8
#define LOADER_HASH_SIZE		32

typedef struct tag_second_loader_hdr {
	unsigned char magic[LOADER_MAGIC_SIZE];
	unsigned int version;
	unsigned int reserved0;
	unsigned int loader_load_addr;		/* physical load addr ,default is 0x60000000 */
	unsigned int loader_load_size;		/* size in bytes */
	unsigned int crc32;			/* crc32 */
	unsigned int hash_len;			/* 20 or 32 , 0 is no hash */
	unsigned char hash[LOADER_HASH_SIZE];	/* sha */
	unsigned int js_hash;			/* js hsah */
	unsigned char reserved[1024-32-32-4];
	unsigned int sign_tag;			/* 0x4E474953, 'N' 'G' 'I' 'S' */
	unsigned int sign_len;			/* 256 */
	unsigned char rsa_hash[256];
	unsigned char reserved2[2048-1024-256-8];
} second_loader_hdr;				/* Size:2K */


#define RTONBOOT_PART_NAME "rtonboot"

int irq_exit = 0;
int seconds = 100;

static void timer_irq_handler(int irq, void *data)
{
	writel(TIMER_CLR_INT, TIMER_BASE + TIMER_INTSTATUS);
	printf("    Hello, this is timer isr: irq=%d\n", irq);
	
	--seconds;
	if(seconds <= 0)
	{
		irq_exit = 1;
	}		
}

int timer_interrupt_test(void)
{
	printf("Timer interrupt:\n");

	/* Disable before config */
	writel(0, TIMER_BASE + TIMER_CTRL);

	/* Configure 1s */
	writel(COUNTER_FREQUENCY, TIMER_BASE + TIMER_LOAD_COUNT0);
	writel(0, TIMER_BASE + TIMER_LOAD_COUNT1);
	writel(TIMER_CLR_INT, TIMER_BASE + TIMER_INTSTATUS);
	writel(TIMER_EN | TIMER_INT_EN, TIMER_BASE + TIMER_CTRL);

	/* Request irq */
	irq_install_handler(TIMER_IRQ, timer_irq_handler, NULL);
	irq_handler_enable(TIMER_IRQ);

	irq_exit = 0;
	seconds = 100;
	
	while (!irq_exit)
	{
	};

	irq_free_handler(TIMER_IRQ);

	return 0;
}

DECLARE_GLOBAL_DATA_PTR;

u8 * RTONBOOT_LOAD_PTR;

extern uint64_t RTOSCPU_STACK_TOP;
extern uint64_t RTOSCPU_ELX_STACK_TOP;

extern uint64_t RTOS_SLAVE_STACK_TOP;
extern uint64_t RTOS_SLAVE_ELX_STACK_TOP;

int reserve_rtos_buffers(void)
{
	ulong addr;

	addr = gd->relocaddr;

	RTOSCPU_STACK_TOP = addr;

	addr -= CONFIG_RTOSCPU_STACKSIZE;

    RTOSCPU_ELX_STACK_TOP = addr + CONFIG_RTOSCPU_ELX_STACKSIZE;
    
    RTOS_SLAVE_STACK_TOP = addr;
    
    addr -= CONFIG_RTOSSLAVE_STACKSIZE;
    
    RTOS_SLAVE_ELX_STACK_TOP = addr + CONFIG_RTOSSLAVE_ELX_STACKSIZE;

	addr -= CONFIG_RTONBOOT_RUNBUF_SIZE;

	addr &= ~(0x200000 - 1);

	gd->relocaddr = addr;

	RTONBOOT_IMG_LOAD_ADDR = addr;
	
	printf("Winfred Young rtonboot reserve buffers stack top is %llx elx stack top is %llx \n ", 
		RTOSCPU_STACK_TOP, RTOSCPU_ELX_STACK_TOP);
		
	printf("Winfred Young rtonboot reserve buffers slave stack top is %llx slave elx stack top is %llx \n ", 
		RTOS_SLAVE_STACK_TOP, RTOS_SLAVE_ELX_STACK_TOP);	

	printf("Winfred Young rtonboot reserve buffers image load addr is %llx\n ", RTONBOOT_IMG_LOAD_ADDR);

	return 0;
}

void testone(void)
{
	rtos_serial_puts("11111111111111111111111111111111111111\n");
}	

extern void rtoscpu_entry(void);

extern void rtos_slave_entry(void);

int make_rtoscpu_on(void)
{
	int ret;
	
	ret = psci_cpu_on(CONFIG_RTOS_CPUID, (unsigned long)rtoscpu_entry);
	if (ret) 
	{
		printf("Winfred Young make_rtoscpu_on failed ret is %d\n", ret);

		return ret;
	}
	
	printf("Winfred Young make_rtoscpu_on OK\n");

	return 0;
}

#if (ENABLE_JIHUOMA > 0)

uint32_t get_mmc_from_blk_desc(struct blk_desc *desc) 
{
	uint32_t value;
	
    if (desc->if_type != IF_TYPE_MMC) 
    {
        printf("Winfred Young Not an MMC device!\n");
        
        return 0;
    }

    struct mmc * mmc = find_mmc_device(desc->devnum);
    if(!mmc)
	{
		printf("Winfred Young cannot find MMC device!\n");
		
		return 0;
	}
	
	value = ( ( (mmc->cid[3] & 0xffff0000) >> 16 ) | ( (mmc->cid[2] & 0xffff) << 16 ) );	

	return value;
}	

extern void do_transform(uint32_t value);

extern uint8_t g_dummyText[];

#endif
	
int read_and_load_rtonboot_image(void)
{
	struct blk_desc *dev_desc;
	disk_partition_t part;
    int ret;
	int part_num;
	u32 blocks;
	u32 sector;
	u8 * ptr;
	struct tag_second_loader_hdr * p_hdr;
	ulong imgsize;
	int blkcnt = 4;
	
	#if (ENABLE_JIHUOMA > 0)
	
		uint32_t value;
		u8 * ptr_ciper_serial;
		unsigned long start;
		unsigned long stop;
		
	#endif	
	
	RTONBOOT_LOAD_PTR = (u8 *)(map_sysmem(RTONBOOT_IMG_LOAD_ADDR, CONFIG_RTONBOOT_RUNBUF_SIZE));

	dev_desc = rockchip_get_bootdev();
	if (!dev_desc)
	{
		printf("winfred : failed to get blk desc\n");

	    return -ENODEV;
	}
	
	#if (ENABLE_JIHUOMA > 0)
	
		value = get_mmc_from_blk_desc(dev_desc);
	
		if(value)
		{
			do_transform(value);
		}
		
	#endif		

	part_num = part_get_info_by_name(dev_desc, RTONBOOT_PART_NAME, &part);
	if (part_num < 0) 
	{
		printf("winfred %s: Can't find part: %s\n", __func__, RTONBOOT_PART_NAME);

		return -1;
	}

	sector = part.start;
	ptr = RTONBOOT_LOAD_PTR;

	ret = blk_dread(dev_desc, sector, blkcnt, ptr);
	if (ret != blkcnt) 
	{
		printf("winfred : failed to read header\n");
		return -EIO;
	}

	p_hdr = (struct tag_second_loader_hdr *)ptr;

	if (memcmp(p_hdr->magic, LOADER_HARD_STR, 6)) 
	{
		printf("Winfred Young bad magic for rtonboot img header\n");

		return  -1;		
	}

	imgsize = p_hdr->loader_load_size;
	if(imgsize > CONFIG_RTONBOOT_RUNBUF_SIZE)
	{
		printf("Winfred Young rtonboot image size is too large\n");

		return -1;
	}

	blocks = DIV_ROUND_UP(imgsize, RK_BLK_SIZE);
	sector = part.start + blkcnt;
	ptr = RTONBOOT_LOAD_PTR;

	ret = blk_dread(dev_desc, sector, blocks, ptr);
	if (ret != blocks) 
	{
		printf("winfred : failed to read rtonboot bin\n");

		return -EIO;
	}
	
	#if 1

	DSB;
	ISB;

	#endif

	//flush_dcache_all();
	
    #if 1
    
	imgsize = blocks * RK_BLK_SIZE;

	flush_dcache_range(RTONBOOT_IMG_LOAD_ADDR, (RTONBOOT_IMG_LOAD_ADDR + imgsize));

	#endif
	
	#if (ENABLE_JIHUOMA > 0)
	
		if(value)
		{
			ptr_ciper_serial = RTONBOOT_LOAD_PTR + OFFSET_TO_RTONBOOT_SHARE_BUFFER 
				+ USER_SHARED_BUF_FIXVAR_SIZE - 16;
			
			memcpy(ptr_ciper_serial, &g_dummyText[0], 16);	
	    
			start = RTONBOOT_IMG_LOAD_ADDR + OFFSET_TO_RTONBOOT_SHARE_BUFFER 
				+ USER_SHARED_BUF_FIXVAR_SIZE - 64;
			
			stop = start + 64;	
		
			flush_dcache_range(start, stop);
		}
	#endif	

	printf("winfred read rtonboot image OK at %llx \n", (long long unsigned int)RTONBOOT_LOAD_PTR);
	
	(void)(make_rtoscpu_on());
		
	return 0;
}

int load_init_and_jump_to_rtonboot(void)
{
	/* char * ptest; */
	int i = 0;
	/* uint64_t test_val; */
	PF_RTONBOOTENTRY pf_rtonbootentry __attribute__ ((unused));

	rtos_printf("Winfred Young load_init_and_jump_to_rtonboot jump to %llx\n", RTONBOOT_IMG_LOAD_ADDR);

	/*ptest = (char *)((void *)RTONBOOT_IMG_LOAD_ADDR);

	ptest += 0x108;

	test_val = *((uint64_t *)ptest);

	rtos_printf("Winfred Young load_init_and_jump_to_rtonboot test_val is %llx\n", test_val);*/

    /*pf_rtonbootentry = (PF_RTONBOOTENTRY)((void *)RTONBOOT_IMG_LOAD_ADDR);*/

	jump_to_rtonboot();

	while(1)
    {
		++i;

		--i;
	};

	return 0;
}

void test_mmu(void)
{
	char * ptest;
	uint64_t test_val; 

	ptest = (char *)((void *)RTONBOOT_IMG_LOAD_ADDR);

	ptest += 0x100;

	test_val = *((uint64_t *)ptest);

	printf("Winfred Young test_mmu test_val is %llx \n", test_val);
}


#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

static struct mm_region winfred_rk3588_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0xf0000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0xf0000000UL,
		.phys = 0xf0000000UL,
		.size = 0x10000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		.virt = 0x100000000UL,
		.phys = 0x100000000UL,
		.size = 0x700000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0x900000000,
		.phys = 0x900000000,
		.size = 0x150000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},  {
		/* List terminator */
		0,
	}
};

struct mm_region * winfred_mem_map = winfred_rk3588_mem_map;

static struct mm_region rtonboot_rk3588_mem_map[] = {
	{
		.virt = 0xffffffbebec00000ULL,
		.phys = 0xecc00000UL,
		.size = 0x1200000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region * rtonboot_mem_map = rtonboot_rk3588_mem_map;

#endif


