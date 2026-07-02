#ifndef _RTOSPUB_H_
#define _RTOSPUB_H_

#include <stdint.h>

#include "../../../../drivers/ethercat/master/patch.h"

#include "../../../../drivers/ethercat/stmmac/linux_stmmac.h"	

#include "../../../../drivers/ethercat/stmmac/stmmac.h"	

#include "../../../../drivers/ethercat/stmmac/rkpriv.h"	

#define EC_KERNEL_EXEC_TASK_PRIORITY 147
#define EC_KERNEL_EXEC_TASK_STACK_SIZE 32768

#define EC_KERNEL_EXEC_TASK_NAME "ec_kernel_exec"

#define MSGID_RTPUB_USER_BEGIN 9

#define MSGID_TESTK3 MSGID_RTPUB_USER_BEGIN
#define MSGID_LINUX_BULK_READY_MSG (MSGID_RTPUB_USER_BEGIN + 1)
#define MSGID_RTOS_BULK_READY_MSG (MSGID_RTPUB_USER_BEGIN + 2)

#define MSGID_START_EC (MSGID_RTPUB_USER_BEGIN + 3)
#define MSGID_CLEANUP_AFTER_EC (MSGID_RTPUB_USER_BEGIN + 4)

#define MSGID_EC_MASTER_READY (MSGID_RTPUB_USER_BEGIN + 5)
#define MSGID_START_EC_CYCLIC (MSGID_RTPUB_USER_BEGIN + 6)
#define MSGID_STOP_EC_CYCLIC (MSGID_RTPUB_USER_BEGIN + 7)

#define MSGID_EC_INTER_GET_MASTER_READY (MSGID_RTPUB_USER_BEGIN + 8)
#define MSGID_START_EC_INTER (MSGID_RTPUB_USER_BEGIN + 9)
#define MSGID_STOP_EC_INTER (MSGID_RTPUB_USER_BEGIN + 10)

#define MSGID_FOE_WRITE (MSGID_RTPUB_USER_BEGIN + 11)
#define MSGID_FOE_READ (MSGID_RTPUB_USER_BEGIN + 12)

#define MSGID_EC_CYCLIC_PERF (MSGID_RTPUB_USER_BEGIN + 13)

struct rtonboot_bulkpara {
	uint32_t offset;
	uint32_t len;
};

#define PHY_MAX_ADDR	32

struct rtonboot_ec_stmmac_mdio_bus_data {
	uint32_t phy_mask;
	
	int32_t probed_phy_irq;
#if 1
	int32_t reset_gpio, active_low;
	uint32_t delays[3];
#endif
};

struct rtonboot_ec_stmmac_dma_cfg {
	int32_t pbl;
	int32_t txpbl;
	int32_t rxpbl;
	bool pblx8;
	int32_t fixed_burst;
	int32_t mixed_burst;
	bool aal;
};

struct rtonboot_ec_stmmac_rxq_cfg {
	uint8_t mode_to_use;
	uint32_t chan;
	uint8_t pkt_route;
	bool use_prio;
	uint32_t prio;
};

struct rtonboot_ec_stmmac_txq_cfg {
	uint32_t weight;
	uint8_t mode_to_use;
	/* Credit Base Shaper parameters */
	uint32_t send_slope;
	uint32_t idle_slope;
	uint32_t high_credit;
	uint32_t low_credit;
	bool use_prio;
	uint32_t prio;
};

struct rtonboot_ec_stmmac_axi {
	bool axi_lpi_en;
	bool axi_xit_frm;
	uint32_t axi_wr_osr_lmt;
	uint32_t axi_rd_osr_lmt;
	bool axi_kbbe;
	uint32_t axi_blen[AXI_BLEN];
	bool axi_fb;
	bool axi_mb;
	bool axi_rb;
};

struct rtonboot_ec_rk_priv_data {
	int32_t phy_iface;
	int32_t bus_id;
	bool suspended;

	bool clk_enabled;
	bool clock_input;
	bool integrated_phy;

	int tx_delay;
	int rx_delay;
};


struct rtonboot_ec_plat_stmmacenet_data {
	int32_t bus_id;
	int32_t phy_addr;
	int32_t interface;	
	int32_t clk_csr;
	int32_t has_gmac;
	int32_t enh_desc;
	int32_t tx_coe;
	int32_t rx_coe;
	int32_t bugged_jumbo;
	int32_t pmt;
	int32_t force_sf_dma_mode;
	int32_t force_thresh_dma_mode;
	int32_t riwt_off;
	int32_t max_speed;
	int32_t maxmtu;
	int32_t multicast_filter_bins;
	int32_t unicast_filter_entries;
	int32_t tx_fifo_size;
	int32_t rx_fifo_size;
	uint32_t rx_queues_to_use;
	uint32_t tx_queues_to_use;
	uint8_t rx_sched_algorithm;
	uint8_t tx_sched_algorithm;
	struct rtonboot_ec_stmmac_rxq_cfg rx_queues_cfg[MTL_MAX_RX_QUEUES];
	struct rtonboot_ec_stmmac_txq_cfg tx_queues_cfg[MTL_MAX_TX_QUEUES];
	int32_t has_gmac4;
	bool has_sun8i;
	bool tso_en;
	int32_t mac_port_sel_speed;
	bool en_tx_lpi_clockgating;
	int32_t has_xgmac;
};

struct rtonboot_ec_stmmac_resources {
	char mac[7];
	int wol_irq;
	int lpi_irq;
	int irq;
};

struct rtonboot_ec_allpapa {
	int32_t net_port_idx;
	
	bool is_null;
	
	bool is_mdio_bus_data_null;
	
	bool is_dma_cfg_null;
	
	bool is_axi_null;
	
	bool is_bsp_priv_null;
	
	bool is_irqs_null;
	
	struct rtonboot_ec_plat_stmmacenet_data ec_plat_stmmacenet_data;
	
	int32_t irqs[PHY_MAX_ADDR];
	
	struct rtonboot_ec_stmmac_mdio_bus_data ec_stmmac_mdio_bus_data;
	
	struct rtonboot_ec_stmmac_dma_cfg ec_stmmac_dma_cfg;
	
	struct rtonboot_ec_stmmac_axi ec_stmmac_axi;
	
	struct rtonboot_ec_rk_priv_data ec_rk_priv_data;
	
	struct rtonboot_ec_stmmac_resources ec_stmmac_resources;
};

struct rtonboot_ec_ready_status {
	uint16_t is_ready;
	uint16_t status;
};

struct rtonboot_start_ec_cyclic {
	uint16_t is_start;
	uint16_t status;
};

struct rtonboot_stop_ec_cyclic {
	uint16_t is_stop;
	uint16_t status;
};

struct rtonboot_start_ec_inter {
	uint16_t is_start;
	uint16_t status;
};

struct rtonboot_stop_ec_inter {
	uint16_t is_stop;
	uint16_t status;
};

struct rtonboot_foe_write_para {
	uint32_t file_size;
};

struct rtonboot_foe_read_para {
	uint32_t file_size;
};

struct rtonboot_ec_cyclic_perf {
	uint64_t period_min_ns;
	uint64_t period_max_ns;
	uint64_t exec_min_ns;
	uint64_t exec_max_ns;
	uint64_t latency_min_ns;
	uint64_t latency_max_ns;
};

typedef void (*resp_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv);
	
typedef void (*end_callack_func_t)(uint32_t msgid, uint32_t msglen, uint8_t * p_content, 
	void * priv);
	
typedef void (*prepare_send_callack_func_t)(uint32_t msgid, uint32_t * p_msglen, uint8_t * p_content, 
	uint32_t max_content_size, void * priv);

void rtospub_set_resp_callback(uint32_t dst_msgid, uint64_t * p_resp_cb, uint64_t * p_resp_cb_priv);

void rtospub_set_end_callback(uint32_t dst_msgid, uint64_t * p_end_cb, uint64_t * p_end_cb_priv);

void rtospub_set_prepare_callback(uint32_t dst_msgid, prepare_send_callack_func_t * p_cb, void ** p_cb_priv);

extern int prepare_send_and_wake_up(uint32_t msgid);

void prepare_rtos_bulk_and_wake_up(void);

#endif
