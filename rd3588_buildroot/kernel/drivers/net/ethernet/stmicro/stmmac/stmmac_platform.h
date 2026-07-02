/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
  Copyright (C) 2007-2009  STMicroelectronics Ltd


  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#ifndef __STMMAC_PLATFORM_H__
#define __STMMAC_PLATFORM_H__

#include "stmmac.h"

struct rk_priv_data {
	struct platform_device *pdev;
	phy_interface_t phy_iface;
	int bus_id;
	struct regulator *regulator;
	bool suspended;
	const struct rk_gmac_ops *ops;

	bool clk_enabled;
	bool clock_input;
	bool integrated_phy;

	struct clk *clk_mac;
	struct clk *gmac_clkin;
	struct clk *mac_clk_rx;
	struct clk *mac_clk_tx;
	struct clk *clk_mac_ref;
	struct clk *clk_mac_refout;
	struct clk *clk_mac_speed;
	struct clk *aclk_mac;
	struct clk *pclk_mac;
	struct clk *clk_phy;
	struct clk *pclk_xpcs;
	struct clk *clk_xpcs_eee;

	struct reset_control *phy_reset;

	int tx_delay;
	int rx_delay;

	struct regmap *grf;
	struct regmap *php_grf;
	struct regmap *xpcs;

	unsigned char otp_data;
	unsigned int bgs_increment;

	struct csu_clk *csu_aclk;
	struct csu_clk *csu_pclk;
};


struct plat_stmmacenet_data *
stmmac_probe_config_dt(struct platform_device *pdev, const char **mac);
void stmmac_remove_config_dt(struct platform_device *pdev,
			     struct plat_stmmacenet_data *plat);

int stmmac_get_platform_resources(struct platform_device *pdev,
				  struct stmmac_resources *stmmac_res);

int stmmac_pltfr_remove(struct platform_device *pdev);
extern const struct dev_pm_ops stmmac_pltfr_pm_ops;

static inline void *get_stmmac_bsp_priv(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	return priv->plat->bsp_priv;
}

#endif /* __STMMAC_PLATFORM_H__ */
