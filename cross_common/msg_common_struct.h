#ifndef _MSG_COMMON_STRUCT_H_
#define _MSG_COMMON_STRUCT_H_

#ifdef __cplusplus 
extern "C" {
#endif

struct rtonboot_foe_write_para {
	int32_t master_idx;
	uint32_t file_size;
};

struct rtonboot_sii_write_para {
	int32_t master_idx;
	uint32_t file_size;
};

struct rtonboot_foe_read_para {
	uint32_t file_size;
};

struct rtonboot_sotest_para {
	uint32_t file_size;
};

struct send_result_msg_content {
	uint32_t msgid;
	uint32_t non_tty_debug;
	uint32_t non_tty_cmd_result_bufoff;
	uint32_t non_tty_cmd_result_buflen;
};

struct rtonboot_test_slave {
	uint64_t magic;
};

struct testrr_msg_content {
	uint32_t msgid;
};

struct testbc_msg_content {
	uint32_t msgid;
};

struct testk3_msg_content {
	uint32_t msgid;
};

struct rtonboot_ec_cyclic_perf {
	uint64_t period_min_ns;
	uint64_t period_max_ns;
	uint64_t exec_min_ns;
	uint64_t exec_max_ns;
	uint64_t latency_min_ns;
	uint64_t latency_max_ns;
};

#define SINGLE_RTETH_BUFFER_SIZE 1500

#define PHY_MAX_ADDR	32

struct rtonboot_ec_stmmac_mdio_bus_data {
	uint32_t phy_mask;
	
	int32_t probed_phy_irq;
/*#if 1
	int32_t reset_gpio, active_low;
	uint32_t delays[3];
#endif*/

	uint32_t has_xpcs;
	bool needs_reset;
};

struct rtonboot_ec_stmmac_dma_cfg {
	int32_t pbl;
	int32_t txpbl;
	int32_t rxpbl;
	bool pblx8;
	int32_t fixed_burst;
	int32_t mixed_burst;
	bool aal;
	bool eame;
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
	int32_t tbs_en;
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

	int32_t tx_delay;
	int32_t rx_delay;
	
	unsigned char otp_data;
	uint32_t bgs_increment;
};


struct rtonboot_ec_plat_stmmacenet_data {
	int32_t bus_id;
	int32_t phy_addr;
	int32_t interface;
	int32_t phy_interface;	
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
	int32_t dma_tx_size;
	int32_t dma_rx_size;
	int32_t flow_ctrl;
	uint32_t addr64;
	uint32_t rx_queues_to_use;
	uint32_t tx_queues_to_use;
	uint8_t rx_sched_algorithm;
	uint8_t tx_sched_algorithm;
	struct rtonboot_ec_stmmac_rxq_cfg rx_queues_cfg[MTL_MAX_RX_QUEUES];
	struct rtonboot_ec_stmmac_txq_cfg tx_queues_cfg[MTL_MAX_TX_QUEUES];
	int32_t ptp_max_adj;
	int32_t has_gmac4;
	bool has_sun8i;
	bool tso_en;
	int rss_en;
	int32_t mac_port_sel_speed;
	bool en_tx_lpi_clockgating;
	bool rx_clk_runs_in_lpi;
	int32_t has_xgmac;
	bool vlan_fail_q_en;
	unsigned char vlan_fail_q;
	uint32_t eee_usecs_rate;
	bool sph_disable;
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

struct rtonboot_start_simpletest {
	uint16_t is_start;
	uint16_t status;
};

struct rtonboot_stop_simpletest {
	uint16_t is_stop;
	uint16_t status;
};

struct rtonboot_start_motorctl {
	uint16_t is_start;
	uint16_t status;
};

struct rtonboot_stop_motorctl {
	uint16_t is_stop;
	uint16_t status;
};

struct rtonboot_start_motorpp {
	uint16_t is_start;
	uint16_t status;
};

struct rtonboot_stop_motorpp {
	uint16_t is_stop;
	uint16_t status;
};

struct rtonboot_motorpp_update_pos {
	int32_t newpos;
};



#ifdef __cplusplus 
}
#endif

#endif
