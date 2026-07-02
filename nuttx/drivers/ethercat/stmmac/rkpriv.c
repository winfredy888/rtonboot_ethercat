#include <nuttx/config.h>
#include <nuttx/spinlock.h>

#include "rkpriv.h"

#include "linux_phy.h"

#include "stmmac_config.h"

#include "stmmac.h"

#include "../../../arch/arm64/src/rk3568/heap_4.h"

#define REG_L(R)	(R##_l)
#define REG_H(R)	(R##_h)
#define READ_REG(REG)	((getreg32((u64)(REG_L(REG))) & 0xFFFF) | \
			((getreg32((u64)(REG_H(REG))) & 0xFFFF) << 16))
#define WRITE_REG(REG, VAL)	\
{\
	putreg32(((VAL) & 0xFFFF) | 0xFFFF0000, (u64)(REG_L(REG))); \
	putreg32((((VAL) & 0xFFFF0000) >> 16) | 0xFFFF0000, (u64)(REG_H(REG)));\
}

#define CLRBITS_LE32(REG, MASK)	WRITE_REG(REG, READ_REG(REG) & ~(MASK))
#define SETBITS_LE32(REG, MASK)	WRITE_REG(REG, READ_REG(REG) | (MASK))
#define CLRSETBITS_LE32(REG, MASK, VAL)	WRITE_REG(REG, \
				(READ_REG(REG) & ~(MASK)) | (VAL))
				
#define OFFSET_TO_BIT(bit)	(1UL << (bit))				


u64 GPIO_BASEADDR[MAX_GPIO_IDX] =
{
	0xfdd60000ULL,
	0xfe740000ULL,
	0xfe750000ULL,
	0xfe760000ULL,
	0xfe770000ULL
};

int rk_gpio_direction_output(int idx, unsigned int offset,
					  int value)
{
	struct rockchip_gpio_regs * regs;
	u64 base_addr;
	u32 mask;
	
	if( (idx < 0) || (idx >= MAX_GPIO_IDX) )
	{
		printk(KERN_ERR, "invalid gpio port def \n");
		
		return -1;
	}
	
	base_addr = GPIO_BASEADDR[idx];
	regs = (struct rockchip_gpio_regs *)(base_addr);
	mask = OFFSET_TO_BIT(offset);
	
	CLRSETBITS_LE32(&regs->swport_dr, mask, value ? mask : 0);
	SETBITS_LE32(&regs->swport_ddr, mask);

	return 0;
}

int rk_gpio_set_value(int idx, unsigned int offset,
					  int value)
{
	struct rockchip_gpio_regs * regs;
	u64 base_addr;
	u32 mask;
	
	if( (idx < 0) || (idx >= MAX_GPIO_IDX) )
	{
		printk(KERN_ERR, "invalid gpio port def \n");
		
		return -1;
	}
	
	base_addr = GPIO_BASEADDR[idx];
	regs = (struct rockchip_gpio_regs *)(base_addr);
	mask = OFFSET_TO_BIT(offset);

	CLRSETBITS_LE32(&regs->swport_dr, mask, value ? mask : 0);

	return 0;
}

u64 virt_to_phy(void * ptr)
{
	u64 temp = (u64)(ptr);
	
	temp -= CONFIG_RAM_START;
	
	temp += RTONBOOT_IMAGE_START_PHYADDR;
	
	return temp;
} 

void * phy_to_virt(u64 pa)
{
	void * va;
	
	pa -= RTONBOOT_IMAGE_START_PHYADDR;
	
	pa += CONFIG_RAM_START;
	
	va = (void *)(pa);
	
	return va;
}

void * dma_zalloc_coherent( size_t size, dma_addr_t *dma_handle, int idx)
{
	void * data;
	
	if(idx == TX_DESC_POOL_IDX)
	{
		data = GetTXDescPoolDataBegin();
		
		*dma_handle = (dma_addr_t)(virt_to_phy(data));
		
		memset(data, 0, size); 
		
		return data;
	}
	else if(idx == RX_DESC_POOL_IDX)
	{
		data = GetRXDescPoolDataBegin();
		
		*dma_handle = (dma_addr_t)(virt_to_phy(data));
		
		memset(data, 0, size); 
		
		return data;
	}	
	else
	{
		pr_err("invalid idx for call dma_zalloc_coherent \n");
		
		return NULL;
	}
}	

void dma_free_coherent(size_t size, void * va, dma_addr_t dma_handle, int idx)
{
}

int dma_mapping_error(dma_addr_t dma_addr)
{
	return dma_addr == 0;
}

extern void flush_dcache_range(unsigned long start, unsigned long stop);

extern void invalidate_dcache_range(unsigned long start, unsigned long stop);

dma_addr_t dma_map_single(void *cpu_addr, size_t size, enum dma_data_direction dir)
{
	unsigned long start;
    unsigned long stop;
    dma_addr_t dma_handle;
    
    if( (dir != DMA_TO_DEVICE) && (dir != DMA_FROM_DEVICE) )
    {
		pr_err("invalid dir for call dma_map_single\n");
		
		return 0;	
	}
    
    start = (unsigned long long)(cpu_addr);
				
	stop = start + size;
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(start, stop);
    
    dma_handle = (dma_addr_t)(virt_to_phy(cpu_addr));
    
	return dma_handle;
}

void dma_unmap_single(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir)
{
	void * va;
	unsigned long start;
    unsigned long stop;
	
	if( (dir != DMA_TO_DEVICE) && (dir != DMA_FROM_DEVICE) )
    {
		pr_err("invalid dir for call dma_map_single\n");
		
		return;	
	}
	
	if(dir == DMA_FROM_DEVICE)
    {
		va = phy_to_virt((u64)dma_addr);
	
		start = (unsigned long long)(va);
				
		stop = start + size;
	
		stop = MYALIGN(stop, 64);
	
		invalidate_dcache_range(start, stop);
	}
}	

void dma_sync_single_for_cpu(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir)
{
	void * va;
	unsigned long start;
    unsigned long stop;

	va = phy_to_virt((u64)dma_addr);
	
	start = (unsigned long long)(va);
				
	stop = start + size;
	
	stop = MYALIGN(stop, 64);
	
	invalidate_dcache_range(start, stop);
}	

void dma_sync_single_for_device(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir)
{
	void * va;
	unsigned long start;
    unsigned long stop;

	va = phy_to_virt((u64)dma_addr);
	
	start = (unsigned long long)(va);
				
	stop = start + size;
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(start, stop);
}

void get_sorted_start_stop(unsigned long long * p_start, unsigned long long * p_stop)
{
	unsigned long long start;
    unsigned long long stop;
    
	start = *(p_start);
	
	stop = *(p_stop);
	
	if(stop < start)
	{
		*(p_start) = stop;
		*(p_stop) = start;
	}
}	

void dma_sync_tx_desc_for_device(struct dma_desc * first, dma_addr_t tail)
{
	void * va_tail;
	unsigned long long start;
    unsigned long long stop;

	va_tail = phy_to_virt((u64)tail);
	
	start = (unsigned long long)(first);
	
	stop = (unsigned long long)(va_tail);
	
	get_sorted_start_stop(&start, &stop);
	
	start &= ~(64 - 1);
				
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(start, stop);
}

void dma_invalidate_whole_ring(void * head, int n, size_t single_size)
{
	unsigned long start;
    unsigned long stop;
    unsigned long align_start;
    size_t size;
    
	start = (unsigned long long)(head);
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	size = single_size * ((size_t)n);
	
	stop = (unsigned long long)(start + size);
	
	stop = MYALIGN(stop, 64);
	
	invalidate_dcache_range(align_start, stop);
}

void update_start_and_stop(u64 * p_start, u64 * p_stop, u64 begin, u64 end)
{
	u64 start;
	u64 stop;
	
	start = *(p_start);
	stop = *(p_stop);
	
	if(!start)
	{
		start = begin;
	}
	else if(begin < start)
	{
		start = begin;
	}	
	
	if(!stop)
	{
		stop = end;
	}
	else if(end > stop)
	{
		stop = end;
	}	
	
	*(p_start) = start;
	*(p_stop) = stop;
}	

void dma_invalidate_for_tx_clean(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q, int budget)
{
	unsigned int entry, count = 0;
	struct dma_desc * p;	
	int single_size;
	u64 start;
    u64 stop;
    u64 begin;
    u64 end;
    unsigned long align_start;
    
	entry = tx_q->dirty_tx;
	
	start = 0;
	stop = 0;
	
	if (priv->extend_desc)
	{
		single_size = sizeof(struct dma_extended_desc); 
	}	
	else
	{
		single_size = sizeof(struct dma_desc);
	}
	
	while ((entry != tx_q->cur_tx) && (count < budget)) 
	{
		if (priv->extend_desc)
		{
			p = (struct dma_desc *)(tx_q->dma_etx + entry);
		}	
		else
		{
			p = tx_q->dma_tx + entry;
		}
		
		begin = (u64)(p);
		end = begin + single_size;
		
		update_start_and_stop(&start, &stop, begin, end);
			
		++count;
		
		entry = STMMAC_GET_ENTRY(entry, DMA_TX_SIZE);
	};
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	stop = MYALIGN(stop, 64);
	
	invalidate_dcache_range(align_start, stop);
}

void dma_flush_for_tx_clean(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q, 
	unsigned int begin_entry, unsigned int end_entry)
{
	unsigned int entry;
	int single_size;
	u64 start;
    u64 stop;
    u64 begin;
    u64 end;
    struct dma_desc * p;
    unsigned long align_start;
    int count = 0;
    
    start = 0;
	stop = 0;
    
    if (priv->extend_desc)
	{
		single_size = sizeof(struct dma_extended_desc); 
	}	
	else
	{
		single_size = sizeof(struct dma_desc);
	}
	
	entry = begin_entry;
	
	while( (entry != end_entry) ) 
	{
		if (priv->extend_desc)
		{
			p = (struct dma_desc *)(tx_q->dma_etx + entry);
		}	
		else
		{
			p = tx_q->dma_tx + entry;
		}
		
		begin = (u64)(p);
		end = begin + single_size;
		
		update_start_and_stop(&start, &stop, begin, end);
		
		++count;
		
		entry = STMMAC_GET_ENTRY(entry, DMA_TX_SIZE);
	};
	
	if(!count)
		return;
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(align_start, stop);
}

void dma_flush_for_whole_tx_ring(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q)
{
	struct dma_desc * head;
	size_t single_size;
	size_t size;
	unsigned long start;
    unsigned long stop;
    unsigned long align_start;
	
	if (priv->extend_desc)
	{
		single_size = sizeof(struct dma_extended_desc); 
		
		head = &tx_q->dma_etx[0].basic;
	}	
	else
	{
		single_size = sizeof(struct dma_desc);
		
		head = &tx_q->dma_tx[0];
	}
	
	size = single_size * ((size_t)DMA_TX_SIZE);
	
	start = (unsigned long long)(head);
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	stop = (unsigned long long)(start + size);
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(align_start, stop);
}

void dma_flush_for_all_tx_ring(struct stmmac_priv *priv)
{
	u32 tx_queue_cnt = priv->plat->tx_queues_to_use;
	u32 queue;
	
	for (queue = 0; queue < tx_queue_cnt; queue++) {
		struct stmmac_tx_queue *tx_q = &priv->tx_queue[queue];
		
		dma_flush_for_whole_tx_ring(priv, tx_q);
	}	
}	

void dma_flush_for_whole_rx_ring(struct stmmac_priv *priv, struct stmmac_rx_queue * rx_q)
{
	struct dma_desc * head;
	size_t single_size;
	size_t size;
	unsigned long start;
    unsigned long stop;
    unsigned long align_start;
	
	if (priv->extend_desc)
	{
		single_size = sizeof(struct dma_extended_desc); 
		
		head = &((rx_q->dma_erx)->basic);
	}	
	else
	{
		single_size = sizeof(struct dma_desc);
		
		head = rx_q->dma_rx;
	}
	
	size = single_size * ((size_t)DMA_RX_SIZE);
	
	start = (unsigned long long)(head);
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	stop = (unsigned long long)(start + size);
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(align_start, stop);
}

void dma_flush_for_all_rx_ring(struct stmmac_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	int queue;
	
	for (queue = 0; queue < rx_count; queue++) {
		struct stmmac_rx_queue *rx_q = &priv->rx_queue[queue];
		
		dma_flush_for_whole_rx_ring(priv, rx_q);
	}		
}

void dma_flush_both_rx_and_tx(struct stmmac_priv *priv)
{
	dma_flush_for_all_tx_ring(priv);
	
	dma_flush_for_all_rx_ring(priv);
}	

void dma_flush_for_rx_refill(struct stmmac_priv *priv, struct stmmac_rx_queue *rx_q , 
	unsigned int begin_entry, unsigned int end_entry)
{
	unsigned int entry;
	int single_size;
	int count = 0;
	u64 start;
    u64 stop;
    u64 begin;
    u64 end;
    struct dma_desc * p;
    unsigned long align_start;
    
    start = 0;
	stop = 0;
    
    if (priv->extend_desc)
	{
		single_size = sizeof(struct dma_extended_desc); 
	}	
	else
	{
		single_size = sizeof(struct dma_desc);
	}
	
	entry = begin_entry;
	
	while( (entry != end_entry) ) 
	{
		if (priv->extend_desc)
			p = (struct dma_desc *)(rx_q->dma_erx + entry);
		else
			p = rx_q->dma_rx + entry;
			
		begin = (u64)(p);
		end = begin + single_size;
		
		update_start_and_stop(&start, &stop, begin, end);
		
		++count;
		
		entry = STMMAC_GET_ENTRY(entry, DMA_RX_SIZE);	
	};	
	
	if(!count)
		return;
	
	align_start = start;
	
	align_start &= ~(64 - 1);
	
	stop = MYALIGN(stop, 64);
	
	flush_dcache_range(align_start, stop);
}

void dma_invalidate_for_rx_single(struct dma_desc * head, int single_size)
{
	u64 start;
    u64 stop;
    
	start = (u64)(head);
	
	stop = start + single_size;
	
	invalidate_dcache_range(start, stop);
}	

unsigned long __ffs(unsigned long word)
{
	int num = 0;

#if (MYBITS_PER_LONG == 64)
	if ((word & 0xffffffff) == 0) {
		num += 32;
		word >>= 32;
	}
#endif
	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx;

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx])
			return min(idx * BITS_PER_LONG + __ffs(addr[idx]), size);
	}

	return size;
}

unsigned long find_next_bit(unsigned long *addr, unsigned long size, unsigned long offset)
{
        unsigned long *p = addr + (offset >> 6);
        unsigned long result = offset & ~63UL;
        unsigned long tmp;

        if (offset >= size)
                return size;
        size -= result;
        offset &= 63UL;
        if (offset) {
                tmp = *(p++);
                tmp &= (~0UL << offset);
                if (size < 64)
                        goto found_first;
                if (tmp)
                        goto found_middle;
                size -= 64;
                result += 64;
        }
        while (size & ~63UL) {
                if ((tmp = *(p++)))
                        goto found_middle;
                result += 64;
                size -= 64;
        }
        if (!size)
                return result;
        tmp = *p;

found_first:
        tmp &= (~0UL >> (64 - size));
        if (tmp == 0UL)        /* Are any bits set? */
                return result + size; /* Nope. */
found_middle:
        return result + __ffs(tmp);
}
