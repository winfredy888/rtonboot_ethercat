#include <linux/types.h>
#include <linux/slab.h>

#include "rtpub.h"

extern int g_netport_idx;

extern struct plat_stmmacenet_data * g_plat_dat;

extern struct stmmac_resources * g_stmmac_res;

extern unsigned char g_mac[];

int32_t get_prepare_kernel_max_content_size(uint32_t msgid)
{
	switch (msgid) 
	{
		case MSGID_TESTK3:
		
			return 8;
			
		case MSGID_START_EC:
		
			return 1024;
			
		case MSGID_RTETH_SEND:
		
		    return (SINGLE_RTETH_BUFFER_SIZE + 4);	
	};
	
	return (int32_t)(-1);		
}

uint8_t * kmalloc_sendmsg_content_in_kernel(uint32_t msglen)
{
	uint8_t * p_send_content = NULL;
	
	p_send_content = (uint8_t *)(kmalloc(msglen , GFP_KERNEL));
	if(!p_send_content)
	{
			printk("Winfred Young RTPUB: kmalloc_sendmsg_content_in_kernel fail\n");
					
			return NULL;
	}
	
	return p_send_content;
}

void prepare_ec_allpara_in_kernel(uint8_t * p_send_content)
{
	struct rtonboot_ec_allpapa * p_allpara = (struct rtonboot_ec_allpapa *)(p_send_content);
	struct rtonboot_ec_plat_stmmacenet_data * p_ec_plat_stmmacenet_data;
	int i;
	struct rtonboot_ec_stmmac_rxq_cfg * p_ec_stmmac_rxq_cfg;
	struct rtonboot_ec_stmmac_txq_cfg * p_ec_stmmac_txq_cfg;
	struct rk_priv_data * p_bsp_priv;
	
	memset(p_allpara, 0, sizeof(struct rtonboot_ec_allpapa));
	
	p_allpara->net_port_idx = (int32_t)(g_netport_idx);
	if(g_netport_idx < 0)
		return;
		
	if( (!(g_plat_dat)) || (!(g_stmmac_res)) )
	{
		p_allpara->is_null = 1;
		
		return;
	}	

		
	p_ec_plat_stmmacenet_data = &(p_allpara->ec_plat_stmmacenet_data);
	
	p_ec_plat_stmmacenet_data->bus_id = g_plat_dat->bus_id;
	p_ec_plat_stmmacenet_data->phy_addr = g_plat_dat->phy_addr;
	p_ec_plat_stmmacenet_data->interface = g_plat_dat->interface;	
	p_ec_plat_stmmacenet_data->phy_interface = (int32_t)(g_plat_dat->phy_interface);	
	p_ec_plat_stmmacenet_data->clk_csr = g_plat_dat->clk_csr;
	p_ec_plat_stmmacenet_data->has_gmac = g_plat_dat->has_gmac;
	p_ec_plat_stmmacenet_data->enh_desc = g_plat_dat->enh_desc;
	p_ec_plat_stmmacenet_data->tx_coe = g_plat_dat->tx_coe;
	p_ec_plat_stmmacenet_data->rx_coe = g_plat_dat->rx_coe;
	p_ec_plat_stmmacenet_data->bugged_jumbo = g_plat_dat->bugged_jumbo;
	p_ec_plat_stmmacenet_data->pmt = g_plat_dat->pmt;
	p_ec_plat_stmmacenet_data->force_sf_dma_mode = g_plat_dat->force_sf_dma_mode;
	p_ec_plat_stmmacenet_data->force_thresh_dma_mode = g_plat_dat->force_thresh_dma_mode;
	p_ec_plat_stmmacenet_data->riwt_off = g_plat_dat->riwt_off;
	p_ec_plat_stmmacenet_data->max_speed = g_plat_dat->max_speed;
	p_ec_plat_stmmacenet_data->maxmtu = g_plat_dat->maxmtu;
	p_ec_plat_stmmacenet_data->multicast_filter_bins = g_plat_dat->multicast_filter_bins;
	p_ec_plat_stmmacenet_data->unicast_filter_entries = g_plat_dat->unicast_filter_entries;
	p_ec_plat_stmmacenet_data->tx_fifo_size = g_plat_dat->tx_fifo_size;
	p_ec_plat_stmmacenet_data->rx_fifo_size = g_plat_dat->rx_fifo_size;
	p_ec_plat_stmmacenet_data->dma_tx_size = g_plat_dat->dma_tx_size;
	p_ec_plat_stmmacenet_data->dma_rx_size = g_plat_dat->dma_rx_size;
	p_ec_plat_stmmacenet_data->flow_ctrl = g_plat_dat->flow_ctrl;
	p_ec_plat_stmmacenet_data->addr64 = g_plat_dat->addr64;
	p_ec_plat_stmmacenet_data->rx_queues_to_use = g_plat_dat->rx_queues_to_use;
	p_ec_plat_stmmacenet_data->tx_queues_to_use = g_plat_dat->tx_queues_to_use;
	p_ec_plat_stmmacenet_data->rx_sched_algorithm = g_plat_dat->rx_sched_algorithm;
	p_ec_plat_stmmacenet_data->tx_sched_algorithm = g_plat_dat->tx_sched_algorithm;
	
	for(i = 0; i < MTL_MAX_RX_QUEUES; ++i)
	{
		p_ec_stmmac_rxq_cfg = &(p_allpara->ec_plat_stmmacenet_data.rx_queues_cfg[i]);
		
		p_ec_stmmac_rxq_cfg->mode_to_use = g_plat_dat->rx_queues_cfg[i].mode_to_use;		
		p_ec_stmmac_rxq_cfg->chan = g_plat_dat->rx_queues_cfg[i].chan;
		p_ec_stmmac_rxq_cfg->pkt_route = g_plat_dat->rx_queues_cfg[i].pkt_route;
		p_ec_stmmac_rxq_cfg->use_prio = g_plat_dat->rx_queues_cfg[i].use_prio;
		p_ec_stmmac_rxq_cfg->prio = g_plat_dat->rx_queues_cfg[i].prio;;
	}
	
	for(i = 0; i < MTL_MAX_TX_QUEUES; ++i)
	{
		p_ec_stmmac_txq_cfg = &(p_allpara->ec_plat_stmmacenet_data.tx_queues_cfg[i]);
		
		p_ec_stmmac_txq_cfg->weight = g_plat_dat->tx_queues_cfg[i].weight;
		p_ec_stmmac_txq_cfg->mode_to_use = g_plat_dat->tx_queues_cfg[i].mode_to_use;
		p_ec_stmmac_txq_cfg->send_slope = g_plat_dat->tx_queues_cfg[i].send_slope; 
		p_ec_stmmac_txq_cfg->idle_slope = g_plat_dat->tx_queues_cfg[i].idle_slope;
		p_ec_stmmac_txq_cfg->high_credit = g_plat_dat->tx_queues_cfg[i].high_credit;
		p_ec_stmmac_txq_cfg->low_credit = g_plat_dat->tx_queues_cfg[i].low_credit;
		p_ec_stmmac_txq_cfg->use_prio = g_plat_dat->tx_queues_cfg[i].use_prio;
		p_ec_stmmac_txq_cfg->prio = g_plat_dat->tx_queues_cfg[i].prio;
		p_ec_stmmac_txq_cfg->tbs_en = g_plat_dat->tx_queues_cfg[i].tbs_en;
	}
	
	p_ec_plat_stmmacenet_data->ptp_max_adj = g_plat_dat->ptp_max_adj;
	p_ec_plat_stmmacenet_data->has_gmac4 = g_plat_dat->has_gmac4;
	p_ec_plat_stmmacenet_data->has_sun8i = g_plat_dat->has_sun8i;
	p_ec_plat_stmmacenet_data->tso_en = g_plat_dat->tso_en;
	p_ec_plat_stmmacenet_data->rss_en = g_plat_dat->rss_en;
	p_ec_plat_stmmacenet_data->mac_port_sel_speed = g_plat_dat->mac_port_sel_speed;
	p_ec_plat_stmmacenet_data->en_tx_lpi_clockgating = g_plat_dat->en_tx_lpi_clockgating ;
	p_ec_plat_stmmacenet_data->rx_clk_runs_in_lpi = g_plat_dat->rx_clk_runs_in_lpi;
	p_ec_plat_stmmacenet_data->has_xgmac = g_plat_dat->has_xgmac;
	
	p_ec_plat_stmmacenet_data->vlan_fail_q_en = g_plat_dat->vlan_fail_q_en;
	p_ec_plat_stmmacenet_data->vlan_fail_q = g_plat_dat->vlan_fail_q;
	p_ec_plat_stmmacenet_data->eee_usecs_rate = g_plat_dat->eee_usecs_rate;
	p_ec_plat_stmmacenet_data->sph_disable = g_plat_dat->sph_disable;
	
	if(!(g_plat_dat->mdio_bus_data))
	{
		p_allpara->is_mdio_bus_data_null = 1;
		
		goto skip_mdio_bus_data;
	}
	
	
	if ( !(g_plat_dat->mdio_bus_data->irqs) )		
	{
		p_allpara->is_irqs_null = 1;
		
		goto skip_irqs; 
	}
		
	for(i = 0; i < PHY_MAX_ADDR; ++i)
	{
		p_allpara->irqs[i] = g_plat_dat->mdio_bus_data->irqs[i];
	}
	
	skip_irqs:
	
	p_allpara->ec_stmmac_mdio_bus_data.phy_mask = g_plat_dat->mdio_bus_data->phy_mask;
	p_allpara->ec_stmmac_mdio_bus_data.probed_phy_irq = g_plat_dat->mdio_bus_data->probed_phy_irq;
	p_allpara->ec_stmmac_mdio_bus_data.has_xpcs = g_plat_dat->mdio_bus_data->has_xpcs;
	p_allpara->ec_stmmac_mdio_bus_data.needs_reset = g_plat_dat->mdio_bus_data->needs_reset;	
		
	skip_mdio_bus_data:
	
	if(!(g_plat_dat->dma_cfg))
	{
		p_allpara->is_dma_cfg_null = 1;
		
		goto skip_dma_cfg;
	}	
		
	p_allpara->ec_stmmac_dma_cfg.pbl = g_plat_dat->dma_cfg->pbl;
	p_allpara->ec_stmmac_dma_cfg.txpbl = g_plat_dat->dma_cfg->txpbl;
	p_allpara->ec_stmmac_dma_cfg.rxpbl = g_plat_dat->dma_cfg->rxpbl;
	p_allpara->ec_stmmac_dma_cfg.pblx8 = g_plat_dat->dma_cfg->pblx8;
	p_allpara->ec_stmmac_dma_cfg.fixed_burst = g_plat_dat->dma_cfg->fixed_burst;
	p_allpara->ec_stmmac_dma_cfg.mixed_burst = g_plat_dat->dma_cfg->mixed_burst;
	p_allpara->ec_stmmac_dma_cfg.aal = g_plat_dat->dma_cfg->aal;
	p_allpara->ec_stmmac_dma_cfg.eame = g_plat_dat->dma_cfg->eame;
	
	skip_dma_cfg:
	
	if(!(g_plat_dat->axi))
	{
		p_allpara->is_axi_null = 1;
		
		goto skip_axi;
	}	
		
	p_allpara->ec_stmmac_axi.axi_lpi_en = g_plat_dat->axi->axi_lpi_en;
	p_allpara->ec_stmmac_axi.axi_xit_frm = g_plat_dat->axi->axi_xit_frm;
	p_allpara->ec_stmmac_axi.axi_wr_osr_lmt = g_plat_dat->axi->axi_wr_osr_lmt;
	p_allpara->ec_stmmac_axi.axi_rd_osr_lmt = g_plat_dat->axi->axi_rd_osr_lmt;
	p_allpara->ec_stmmac_axi.axi_kbbe = g_plat_dat->axi->axi_kbbe;
	
	for(i = 0; i < AXI_BLEN; ++i)
	{
		p_allpara->ec_stmmac_axi.axi_blen[i] = g_plat_dat->axi->axi_blen[i];
	}	
	
	p_allpara->ec_stmmac_axi.axi_fb = g_plat_dat->axi->axi_fb;
	p_allpara->ec_stmmac_axi.axi_mb = g_plat_dat->axi->axi_mb;
	p_allpara->ec_stmmac_axi.axi_rb = g_plat_dat->axi->axi_rb;	
		
	skip_axi:
	
	if(!(g_plat_dat->bsp_priv))
	{
		p_allpara->is_bsp_priv_null = 1;
		
		goto skip_bsp_priv;
	}	
		
	p_bsp_priv = (struct rk_priv_data *)(g_plat_dat->bsp_priv);
	
	p_allpara->ec_rk_priv_data.phy_iface = 	p_bsp_priv->phy_iface;
	p_allpara->ec_rk_priv_data.bus_id = p_bsp_priv->bus_id;
	p_allpara->ec_rk_priv_data.suspended = p_bsp_priv->suspended;
	p_allpara->ec_rk_priv_data.clk_enabled = p_bsp_priv->clk_enabled;
	p_allpara->ec_rk_priv_data.clock_input = p_bsp_priv->clock_input;
	p_allpara->ec_rk_priv_data.integrated_phy = p_bsp_priv->integrated_phy;
	p_allpara->ec_rk_priv_data.tx_delay = p_bsp_priv->tx_delay;
	p_allpara->ec_rk_priv_data.rx_delay = p_bsp_priv->rx_delay;
	p_allpara->ec_rk_priv_data.otp_data = p_bsp_priv->otp_data;
	p_allpara->ec_rk_priv_data.bgs_increment = p_bsp_priv->bgs_increment;
	
	skip_bsp_priv:
	
	p_allpara->ec_stmmac_resources.irq = g_stmmac_res->irq;
	p_allpara->ec_stmmac_resources.lpi_irq = g_stmmac_res->lpi_irq;
	p_allpara->ec_stmmac_resources.wol_irq = g_stmmac_res->wol_irq;
	
	memcpy( &(p_allpara->ec_stmmac_resources.mac[0]), &(g_mac[0]), 6);
	
	p_allpara->ec_stmmac_resources.mac[6] = 0;
	
	return;
}

extern void prepare_single_rteth_send_buffer(uint8_t * msgbuf, uint32_t * p_msglen);

uint8_t * prepare_sendmsg_content_in_kernel(uint32_t msgid, uint32_t * p_msglen)
{
	uint8_t * p_send_content = NULL;
	uint32_t msgdata_len;
	
	switch (msgid) 
	{
		case MSGID_TESTK3:
		
			*p_msglen = 8;
			
			p_send_content = kmalloc_sendmsg_content_in_kernel((*p_msglen));
			if(!p_send_content)
				return NULL;
				
			p_send_content[0] = 0x88;	
				
			break;	
			
		case MSGID_START_EC:
		
			*p_msglen = sizeof(struct rtonboot_ec_allpapa);
			
			p_send_content = kmalloc_sendmsg_content_in_kernel((*p_msglen));
			if(!p_send_content)
				return NULL;
				
			prepare_ec_allpara_in_kernel(p_send_content);	
				
			break;
			
		case MSGID_RTETH_SEND:
		
			p_send_content = kmalloc_sendmsg_content_in_kernel((*p_msglen));
			if(!p_send_content)
				return NULL;	
				
			msgdata_len = (*p_msglen) - 4;
				
			prepare_single_rteth_send_buffer(p_send_content + 4, &msgdata_len);
			
			(*p_msglen) = msgdata_len + 4;
			
			*((uint32_t *)p_send_content) = msgdata_len;	
				
			break;
	};
	
	return p_send_content;
}

void test_k3_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	printk("Winfred Young RTPUB: test_k3_callback called %x\n", (*p_content) );
}

void start_ec_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	printk("Winfred Young RTPUB: start_ec_callback called\n");
}

void rteth_send_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	printk("Winfred Young RTPUB: rteth_send_callback called\n");
}

extern void rteth_rx_packet(void * data, int len);

void rteth_recv_callback(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	uint32_t pkt_len;
	
	pkt_len = *((uint32_t *)(p_content));
	
	printk("Winfred Young RTPUB: rteth_recv_callback called pkt_len is %d\n", pkt_len);
	
	rteth_rx_packet( (void *)(p_content + 4), pkt_len);
}

void set_kernel_callback_according_msgid(uint32_t msgid, uint64_t * p_callback, uint64_t * p_priv)
{
	switch (msgid) 
	{
		case MSGID_TESTK3:
		
			*(p_callback) = (u64)(&test_k3_callback);
			
			*(p_priv) = 0;
				
			break;
			
		case MSGID_START_EC:
		
			*(p_callback) = (u64)(&start_ec_callback);
			
			*(p_priv) = 0;
			
			break;
			
		case MSGID_RTETH_SEND:
		
			*(p_callback) = (u64)(&rteth_send_callback);
			
			*(p_priv) = 0;
			
			break;
			
		case MSGID_RTETH_RECV:
		
			*(p_callback) = (u64)(&rteth_recv_callback);
			
			*(p_priv) = 0;
			
			break;	
	};
}		
