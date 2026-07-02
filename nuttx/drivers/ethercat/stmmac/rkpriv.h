#ifndef __RKPRIV_H__
#define __RKPRIV_H__

#include <nuttx/config.h>

#include "stmmac_config.h"

#include "linux_stmmac.h"

#include "hwif.h"

#include "stmmac.h"

struct nuttx_bsp_priv {
	int phy_iface;
	int bus_id;
	bool suspended;

	bool clk_enabled;
	bool clock_input;
	bool integrated_phy;

	int tx_delay;
	int rx_delay;

	u64 grf_base_addr;
	u64 xpcs_base_addr;
	
	unsigned char otp_data;
	uint32_t bgs_increment;
};

struct gpio_port_def {
	int idx;
	unsigned int offset;
};

#define MAX_GPIO_IDX 5

struct rockchip_gpio_regs {
	u32 swport_dr_l;                        /* ADDRESS OFFSET: 0x0000 */
	u32 swport_dr_h;                        /* ADDRESS OFFSET: 0x0004 */
	u32 swport_ddr_l;                       /* ADDRESS OFFSET: 0x0008 */
	u32 swport_ddr_h;                       /* ADDRESS OFFSET: 0x000c */
	u32 int_en_l;                           /* ADDRESS OFFSET: 0x0010 */
	u32 int_en_h;                           /* ADDRESS OFFSET: 0x0014 */
	u32 int_mask_l;                         /* ADDRESS OFFSET: 0x0018 */
	u32 int_mask_h;                         /* ADDRESS OFFSET: 0x001c */
	u32 int_type_l;                         /* ADDRESS OFFSET: 0x0020 */
	u32 int_type_h;                         /* ADDRESS OFFSET: 0x0024 */
	u32 int_polarity_l;                     /* ADDRESS OFFSET: 0x0028 */
	u32 int_polarity_h;                     /* ADDRESS OFFSET: 0x002c */
	u32 int_bothedge_l;                     /* ADDRESS OFFSET: 0x0030 */
	u32 int_bothedge_h;                     /* ADDRESS OFFSET: 0x0034 */
	u32 debounce_l;                         /* ADDRESS OFFSET: 0x0038 */
	u32 debounce_h;                         /* ADDRESS OFFSET: 0x003c */
	u32 dbclk_div_en_l;                     /* ADDRESS OFFSET: 0x0040 */
	u32 dbclk_div_en_h;                     /* ADDRESS OFFSET: 0x0044 */
	u32 dbclk_div_con;                      /* ADDRESS OFFSET: 0x0048 */
	u32 reserved004c;                       /* ADDRESS OFFSET: 0x004c */
	u32 int_status;                         /* ADDRESS OFFSET: 0x0050 */
	u32 reserved0054;                       /* ADDRESS OFFSET: 0x0054 */
	u32 int_rawstatus;                      /* ADDRESS OFFSET: 0x0058 */
	u32 reserved005c;                       /* ADDRESS OFFSET: 0x005c */
	u32 port_eoi_l;                         /* ADDRESS OFFSET: 0x0060 */
	u32 port_eoi_h;                         /* ADDRESS OFFSET: 0x0064 */
	u32 reserved0068[2];                    /* ADDRESS OFFSET: 0x0068 */
	u32 ext_port;                           /* ADDRESS OFFSET: 0x0070 */
	u32 reserved0074;                       /* ADDRESS OFFSET: 0x0074 */
	u32 ver_id;                             /* ADDRESS OFFSET: 0x0078 */
};

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

# define MYBITS_PER_LONG 64

int rk_gpio_direction_output(int idx, unsigned int offset,
					  int value);
					  
int rk_gpio_set_value(int idx, unsigned int offset,
					  int value);					  
					  
u64 virt_to_phy(void * ptr);

void * phy_to_virt(u64 pa);

void * dma_zalloc_coherent(size_t size, dma_addr_t *dma_handle, int idx);

void dma_free_coherent(size_t size, void * va, dma_addr_t dma_handle, int idx);

int dma_mapping_error(dma_addr_t dma_addr);

dma_addr_t dma_map_single(void *cpu_addr, size_t size, enum dma_data_direction dir);

void dma_unmap_single(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir);

void dma_sync_single_for_cpu(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir);

void dma_sync_single_for_device(dma_addr_t dma_addr, size_t size, enum dma_data_direction dir);

void dma_sync_tx_desc_for_device(struct dma_desc * first, dma_addr_t tail);

void dma_invalidate_whole_ring(void * head, int n, size_t single_size);

void dma_invalidate_for_tx_clean(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q, int budget);

void dma_flush_for_tx_clean(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q, 
	unsigned int begin_entry, unsigned int end_entry);
	
void dma_flush_for_rx_refill(struct stmmac_priv *priv, struct stmmac_rx_queue *rx_q , 
	unsigned int begin_entry, unsigned int end_entry);	
	
void dma_flush_for_whole_tx_ring(struct stmmac_priv *priv, struct stmmac_tx_queue *tx_q);

void dma_flush_for_whole_rx_ring(struct stmmac_priv *priv, struct stmmac_rx_queue * rx_q);

void dma_flush_for_all_tx_ring(struct stmmac_priv *priv);

void dma_flush_for_all_rx_ring(struct stmmac_priv *priv);

void dma_flush_both_rx_and_tx(struct stmmac_priv *priv);

void dma_invalidate_for_rx_single(struct dma_desc * head, int single_size);

extern unsigned long find_first_bit(const unsigned long *addr, unsigned long size);

extern unsigned long find_next_bit(unsigned long *addr, unsigned long size, unsigned long offset);

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))



#endif
