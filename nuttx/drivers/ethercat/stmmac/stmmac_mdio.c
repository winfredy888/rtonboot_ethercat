/*******************************************************************************
  STMMAC Ethernet Driver -- MDIO bus implementation
  Provides Bus interface for MII registers

  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Carl Shaw <carl.shaw@st.com>
  Maintainer: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

/*#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/slab.h>*/

#include <nuttx/config.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>

#include <stdio.h>

#include "stmmac_config.h"

#include "linux_stmmac.h"

#include "linux_phy.h"

#include "rkpriv.h"

#include "dwxgmac2.h"
#include "stmmac.h"

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002
#define MII_DATA_MASK GENMASK(15, 0)

/* GMAC4 defines */
#define MII_GMAC4_GOC_SHIFT		2
#define MII_GMAC4_REG_ADDR_SHIFT	16
#define MII_GMAC4_WRITE			(1 << MII_GMAC4_GOC_SHIFT)
#define MII_GMAC4_READ			(3 << MII_GMAC4_GOC_SHIFT)

/* XGMAC defines */
#define MII_XGMAC_SADDR			BIT(18)
#define MII_XGMAC_CMD_SHIFT		16
#define MII_XGMAC_WRITE			(1 << MII_XGMAC_CMD_SHIFT)
#define MII_XGMAC_READ			(3 << MII_XGMAC_CMD_SHIFT)
#define MII_XGMAC_BUSY			BIT(22)
#define MII_XGMAC_MAX_C22ADDR		3
#define MII_XGMAC_C22P_MASK		GENMASK(MII_XGMAC_MAX_C22ADDR, 0)
#define MII_XGMAC_PA_SHIFT		16
#define MII_XGMAC_DA_SHIFT		21

#define MII_DEVADDR_C45_SHIFT	16

#define MII_GMAC4_C45E			BIT(1)

#define MII_REGADDR_C45_MASK	GENMASK(15, 0)

void phy_state_machine(void * arg);

extern struct phy_driver realtek_drv;

#define READL_POLL_TIMEOUT 30000
static int stmmac_xgmac2_c45_format(struct stmmac_priv *priv, int phyaddr,
				    int phyreg, u32 *hw_addr)
{
	u32 tmp;

	/* Set port as Clause 45 */
	tmp = getreg32((u64)(priv->ioaddr + XGMAC_MDIO_C22P));
	tmp &= ~BIT(phyaddr);
	putreg32(tmp, (u64)(priv->ioaddr + XGMAC_MDIO_C22P));

	*hw_addr = (phyaddr << MII_XGMAC_PA_SHIFT) | (phyreg & 0xffff);
	*hw_addr |= (phyreg >> MII_DEVADDR_C45_SHIFT) << MII_XGMAC_DA_SHIFT;
	return 0;
}

static int stmmac_xgmac2_c22_format(struct stmmac_priv *priv, int phyaddr,
				    int phyreg, u32 *hw_addr)
{
	/*unsigned int mii_data = priv->hw->mii.data;*/
	u32 tmp;

	/* HW does not support C22 addr >= 4 */
	if (phyaddr > MII_XGMAC_MAX_C22ADDR)
		return -ENODEV;

	/* Set port as Clause 22 */
	tmp = getreg32((u64)(priv->ioaddr + XGMAC_MDIO_C22P));
	tmp &= ~MII_XGMAC_C22P_MASK;
	tmp |= BIT(phyaddr);
	putreg32(tmp, (u64)(priv->ioaddr + XGMAC_MDIO_C22P));

	*hw_addr = (phyaddr << MII_XGMAC_PA_SHIFT) | (phyreg & 0x1f);
	return 0;
}

static int stmmac_xgmac2_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_driver_s *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 tmp, addr, value = MII_XGMAC_BUSY;
	int ret;

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_data, tmp,
			       !(tmp & MII_XGMAC_BUSY), 100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}
	if (phyreg & MII_ADDR_C45) {
		phyreg &= ~MII_ADDR_C45;

		ret = stmmac_xgmac2_c45_format(priv, phyaddr, phyreg, &addr);
		if (ret)
			goto err_disable_clks;
	} else {
		ret = stmmac_xgmac2_c22_format(priv, phyaddr, phyreg, &addr);
		if (ret)
			goto err_disable_clks;

		value |= MII_XGMAC_SADDR;
	}

	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	value |= MII_XGMAC_SADDR | MII_XGMAC_READ;

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_data, tmp,
			       !(tmp & MII_XGMAC_BUSY), 100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}

	/* Set the MII address register to read */
	putreg32(addr, (u64)(priv->ioaddr + mii_address));
	putreg32(value, (u64)(priv->ioaddr + mii_data));

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_data, tmp,
			       !(tmp & MII_XGMAC_BUSY), 100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}

	/* Read the data from the MII data register */
	ret = (int)getreg32((u64)(priv->ioaddr + mii_data)) & GENMASK(15, 0);
err_disable_clks:
	
	return ret;
}

static int stmmac_xgmac2_mdio_write(struct mii_bus *bus, int phyaddr,
				    int phyreg, u16 phydata)
{
	struct net_driver_s *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 addr, tmp, value = MII_XGMAC_BUSY;
	int ret;

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_data, tmp,
			       !(tmp & MII_XGMAC_BUSY), 100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}
	if (phyreg & MII_ADDR_C45) {
		phyreg &= ~MII_ADDR_C45;

		ret = stmmac_xgmac2_c45_format(priv, phyaddr, phyreg, &addr);
		if (ret)
			goto err_disable_clks;
	} else {
		ret = stmmac_xgmac2_c22_format(priv, phyaddr, phyreg, &addr);
		if (ret)
			goto err_disable_clks;

		value |= MII_XGMAC_SADDR;
	}

	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	value |= phydata;
	value |= MII_XGMAC_WRITE;

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_data, tmp,
			       !(tmp & MII_XGMAC_BUSY), 100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}

	/* Set the MII address register to write */
	putreg32(addr, (u64)(priv->ioaddr + mii_address));
	putreg32(value, (u64)(priv->ioaddr + mii_data));

	/* Wait until any existing MII operation is complete */
	ret = readl_poll_timeout(priv->ioaddr + mii_data, tmp,
				  !(tmp & MII_XGMAC_BUSY), 100, 10000);

err_disable_clks:
	
	return ret;
}

/**
 * stmmac_mdio_read
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr
 * @phyreg: MII reg
 * Description: it reads data from the MII register from within the phy device.
 * For the 7111 GMAC, we must set the bit 0 in the MII address register while
 * accessing the PHY registers.
 * Fortunately, it seems this has no drawback for the 7109 MAC.
 */
static int stmmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_driver_s *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 v;
	int data = 0;
	u32 value = MII_BUSY;

	value |= (phyaddr << priv->hw->mii.addr_shift)
		& priv->hw->mii.addr_mask;
	value |= (phyreg << priv->hw->mii.reg_shift) & priv->hw->mii.reg_mask;
	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	if (priv->plat->has_gmac4) {
		value |= MII_GMAC4_READ;
		if (phyreg & MII_ADDR_C45) {
			value |= MII_GMAC4_C45E;
			value &= ~priv->hw->mii.reg_mask;
			value |= ((phyreg >> MII_DEVADDR_C45_SHIFT) <<
			       priv->hw->mii.reg_shift) &
			       priv->hw->mii.reg_mask;

			data |= (phyreg & MII_REGADDR_C45_MASK) <<
				MII_GMAC4_REG_ADDR_SHIFT;
		}
	}
	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000)) {
		data = -EBUSY;
		goto err_disable_clks;
	}

	putreg32(data, (u64)(priv->ioaddr + mii_data));
	putreg32(value, (u64)(priv->ioaddr + mii_address));

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000)) {
		data = -EBUSY;
		goto err_disable_clks;
	}

	/* Read the data from the MII data register */
	data = (int)getreg32((u64)(priv->ioaddr + mii_data)) & MII_DATA_MASK;

err_disable_clks:
	
	return data;
}

/**
 * stmmac_mdio_write
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr
 * @phyreg: MII reg
 * @phydata: phy data
 * Description: it writes the data into the MII register from within the device.
 */
static int stmmac_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			     u16 phydata)
{
	struct net_driver_s * ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 v;
	u32 value = MII_BUSY;
	int ret, data = phydata;

	value |= (phyaddr << priv->hw->mii.addr_shift)
		& priv->hw->mii.addr_mask;
	value |= (phyreg << priv->hw->mii.reg_shift) & priv->hw->mii.reg_mask;

	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	if (priv->plat->has_gmac4) {
		value |= MII_GMAC4_WRITE;
		if (phyreg & MII_ADDR_C45) {
			value |= MII_GMAC4_C45E;
			value &= ~priv->hw->mii.reg_mask;
			value |= ((phyreg >> MII_DEVADDR_C45_SHIFT) <<
			       priv->hw->mii.reg_shift) &
			       priv->hw->mii.reg_mask;

			data |= (phyreg & MII_REGADDR_C45_MASK) <<
				MII_GMAC4_REG_ADDR_SHIFT;
		}
	} else {
		value |= MII_WRITE;
	}

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000)) {
		ret = -EBUSY;
		goto err_disable_clks;
	}

	/* Set the MII address register to write */
	putreg32(phydata, (u64)(priv->ioaddr + mii_data));
	putreg32(value, (u64)(priv->ioaddr + mii_address));

	/* Wait until any existing MII operation is complete */
	ret = readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
				 100, 10000);
err_disable_clks:
	
	return ret;
}

struct gpio_port_def PHY_RESET_GPIO_DEF[] = {
	
	{
		.idx = 3,
		.offset = 25,
	},	
	
	{
		.idx = 3,
		.offset = 9,
	},
	
	{
		.idx = -1,
		.offset = 0,
	},
};

int active_low_array[] =
{
	1,
	1,
	-1
};

int delays_array[2][3] =
{
	{0, 20000, 100000},
	{0, 20000, 100000}
};

/**
 * stmmac_mdio_reset
 * @bus: points to the mii_bus structure
 * Description: reset the MII bus
 */
int stmmac_mdio_reset(struct mii_bus *bus)
{	
/*#if IS_ENABLED(CONFIG_STMMAC_PLATFORM)*/

#if 0

#if 1
	struct net_driver_s * ndev = bus->priv;
	
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned int mii_address = priv->hw->mii.addr;
	struct stmmac_mdio_bus_data *data = priv->plat->mdio_bus_data;
	struct stmmac_netdev_priv * nuttx_netdev_priv = netdev_rk_priv(ndev);

/*#ifdef CONFIG_OF*/
#if 0
	if (priv->device->of_node) {
		if (data->reset_gpio < 0) {
			struct device_node *np = priv->device->of_node;

			if (!np)
				return 0;

			data->reset_gpio = of_get_named_gpio(np,
						"snps,reset-gpio", 0);
			if (data->reset_gpio < 0)
				return 0;

			data->active_low = of_property_read_bool(np,
						"snps,reset-active-low");
			of_property_read_u32_array(np,
				"snps,reset-delays-us", data->delays, 3);

			if (devm_gpio_request(priv->device, data->reset_gpio,
					      "mdio-reset"))
				return 0;
		}

		gpio_direction_output(data->reset_gpio,
				      data->active_low ? 1 : 0);
		if (data->delays[0])
			msleep(DIV_ROUND_UP(data->delays[0], 1000));

		gpio_set_value(data->reset_gpio, data->active_low ? 0 : 1);
		if (data->delays[1])
			msleep(DIV_ROUND_UP(data->delays[1], 1000));

		gpio_set_value(data->reset_gpio, data->active_low ? 1 : 0);
		if (data->delays[2])
			msleep(DIV_ROUND_UP(data->delays[2], 1000));
	}
	
#else	
	
	struct gpio_port_def * p_def = &(PHY_RESET_GPIO_DEF[nuttx_netdev_priv->netport_idx]);
	
	int * p_delay = &(delays_array[0][nuttx_netdev_priv->netport_idx]);
	
	data->delays[0] = p_delay[0];
	data->delays[1] = p_delay[1];
	data->delays[2] = p_delay[2];
	
	data->active_low = active_low_array[nuttx_netdev_priv->netport_idx];

	data->reset_gpio = (p_def->idx) * 32 + p_def->offset;
	
	rk_gpio_direction_output(p_def->idx, p_def->offset, data->active_low ? 1 : 0);
	
	if (data->delays[0])
		msleep(DIV_ROUND_UP(data->delays[0], 1000));

	rk_gpio_set_value(p_def->idx, p_def->offset, data->active_low ? 0 : 1);
	
	if (data->delays[1])
		msleep(DIV_ROUND_UP(data->delays[1], 1000));

	rk_gpio_set_value(p_def->idx, p_def->offset, data->active_low ? 1 : 0);
	if (data->delays[2])
		msleep(DIV_ROUND_UP(data->delays[2], 1000));

#endif

	if (data->phy_reset) {
		netdev_dbg(ndev, "stmmac_mdio_reset: calling phy_reset\n");
		data->phy_reset(priv->plat->bsp_priv);
	}

	/* This is a workaround for problems with the STE101P PHY.
	 * It doesn't complete its reset until at least one clock cycle
	 * on MDC, so perform a dummy mdio read. To be updated for GMAC4
	 * if needed.
	 */
	if (!priv->plat->has_gmac4)
	{
		putreg32(0, (u64)(priv->ioaddr + mii_address));
	}	
		
#endif

#endif
    
	return 0;
}

struct mii_bus *mdiobus_alloc_size(size_t size)
{
	struct mii_bus *bus;
	size_t aligned_size = ALIGN(sizeof(*bus), NETDEV_ALIGN);
	size_t alloc_size;
	int i;

	/* If we alloc extra space, it should be aligned */
	if (size)
		alloc_size = aligned_size + size;
	else
		alloc_size = sizeof(*bus);

	bus = kmm_malloc(alloc_size);
	if (!bus)
		return NULL;
		
    memset(bus, 0, alloc_size);

	/*bus->state = MDIOBUS_ALLOCATED;*/

	/* Initialise the interrupts to polling */
	for (i = 0; i < PHY_MAX_ADDR; i++)
		bus->irq[i] = PHY_POLL;

	return bus;
}

struct mii_bus *mdiobus_alloc(void)
{
	return mdiobus_alloc_size(0);
}

void mdiobus_free(struct mii_bus *bus)
{
	kmm_free(bus);
}

struct phy_device *phy_device_create(struct mii_bus *bus, int addr, int phy_id)
{
	struct phy_device *dev;

	/* We allocate the device, and initialize the default values */
	dev = kmm_malloc(sizeof(*dev) /*, GFP_KERNEL*/);
	if (!dev)
		return ERR_PTR(-ENOMEM);
		
	memset(dev, 0, sizeof(*dev));

	dev->speed = SPEED_UNKNOWN;
	dev->duplex = DUPLEX_UNKNOWN;
	dev->pause = 0;
	dev->asym_pause = 0;
	dev->link = 0;
	dev->interface = PHY_INTERFACE_MODE_GMII;

	dev->autoneg = AUTONEG_ENABLE;

	dev->phy_id = phy_id;
	dev->addr = addr;
	dev->irq = bus->irq[addr];

	dev->state = PHY_DOWN;
	
	nxmutex_init(&dev->lock);
	
	work_init(&dev->state_queue, phy_state_machine, (void *)(dev));

	return dev;
}

void phy_device_free(struct phy_device *phydev)
{
	struct net_driver_s *ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_netdev_priv * nuttx_netdev_priv = netdev_rk_priv(ndev);
	
	if(phydev)
	{
		kmm_free(phydev);
	}
	
	nuttx_netdev_priv->phydev = NULL;
}

int rk_phy_addr[MAX_NETPORT_NUM] =
{1};

#define RTL8211F_PHY_ID 0x001cc916

/**
 * stmmac_mdio_register
 * @ndev: net device structure
 * Description: it registers the MII bus
 */
int stmmac_mdio_register(struct net_driver_s *ndev)
{
	int err = 0;
	struct mii_bus *new_bus;
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct stmmac_mdio_bus_data *mdio_bus_data = priv->plat->mdio_bus_data;
	struct stmmac_netdev_priv * nuttx_netdev_priv = netdev_rk_priv(ndev);
	/*struct device_node *mdio_node = priv->plat->mdio_node;
	struct device *dev = ndev->dev.parent;*/
	int addr;
	/*int found, max_addr;*/
	
	struct phy_device * phydev;

	if (!mdio_bus_data)
		return 0;

	new_bus = mdiobus_alloc();
	if (!new_bus)
		return -ENOMEM;

	if (mdio_bus_data->irqs)
		memcpy(new_bus->irq, mdio_bus_data->irqs, sizeof(new_bus->irq));

#ifdef CONFIG_OF
	if (priv->device->of_node)
		mdio_bus_data->reset_gpio = -1;
#endif

	strcpy(new_bus->name, "ec_stmmac");

	if (priv->plat->has_xgmac) {
		new_bus->read = &stmmac_xgmac2_mdio_read;
		new_bus->write = &stmmac_xgmac2_mdio_write;

		/* Right now only C22 phys are supported */
		//max_addr = MII_XGMAC_MAX_C22ADDR + 1;

		/* Check if DT specified an unsupported phy addr */
		if (priv->plat->phy_addr > MII_XGMAC_MAX_C22ADDR)
			netdev_err(ndev, "Unsupported phy_addr (max=%d)\n",
					MII_XGMAC_MAX_C22ADDR);
	} else {
		new_bus->read = &stmmac_mdio_read;
		new_bus->write = &stmmac_mdio_write;
		//max_addr = PHY_MAX_ADDR;
	}

	/*Winfred Young need recheck*/
	
	/*if (mdio_bus_data->has_xpcs) {
		priv->hw->xpcs = mdio_xpcs_get_ops();
		if (!priv->hw->xpcs) {
			err = -ENODEV;
			goto bus_register_fail;
		}
	}*/
	
	new_bus->reset = &stmmac_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x",
		 &(new_bus->name[0]), priv->plat->bus_id);
	new_bus->priv = ndev;
	
	new_bus->phy_mask = mdio_bus_data->phy_mask;
	/*new_bus->parent = priv->device;*/

    #if 0
    
    err = of_mdiobus_register(new_bus, mdio_node);
	if (err != 0) {
		netdev_err(ndev, "Cannot register the MDIO bus\n");
		goto bus_register_fail;
	}
	/* Looks like we need a dummy read for XGMAC only and C45 PHYs */
	if (priv->plat->has_xgmac)
		stmmac_xgmac2_mdio_read(new_bus, 0, MII_ADDR_C45);

	if (priv->plat->phy_node || mdio_node)
		goto bus_register_done;

	found = 0;
	for (addr = 0; addr < max_addr; addr++) {
		struct phy_device *phydev = mdiobus_get_phy(new_bus, addr);

		if (!phydev)
			continue;

		/*
		 * If an IRQ was provided to be assigned after
		 * the bus probe, do it here.
		 */
		if (!mdio_bus_data->irqs &&
		    (mdio_bus_data->probed_phy_irq > 0)) {
			new_bus->irq[addr] = mdio_bus_data->probed_phy_irq;
			phydev->irq = mdio_bus_data->probed_phy_irq;
		}

		/*
		 * If we're going to bind the MAC to this PHY bus,
		 * and no PHY number was provided to the MAC,
		 * use the one probed here.
		 */
		if (priv->plat->phy_addr == -1)
			priv->plat->phy_addr = addr;

		phy_attached_info(phydev);
		found = 1;
	}

	if (!found && !mdio_node) {
		netdev_warn(ndev, "No PHY found\n");
		mdiobus_unregister(new_bus);
		mdiobus_free(new_bus);
		return -ENODEV;
	}
	
	#else
	
	addr = rk_phy_addr[nuttx_netdev_priv->netport_idx];
	
	if (!mdio_bus_data->irqs &&
		    (mdio_bus_data->probed_phy_irq > 0)) 
	{
		new_bus->irq[addr] = mdio_bus_data->probed_phy_irq;
	}
	
	phydev = phy_device_create(new_bus, addr, RTL8211F_PHY_ID);
	if(IS_ERR(phydev))
	{
		netdev_err(ndev, "phy_device_create fail\n");
		
		goto bus_register_fail;
	}
	
	if (!mdio_bus_data->irqs &&
		    (mdio_bus_data->probed_phy_irq > 0)) 
	{
		phydev->irq = mdio_bus_data->probed_phy_irq;
	}
	
	phydev->priv = (void *)(ndev);
	
	nuttx_netdev_priv->phydev = phydev;

	nxmutex_init(&new_bus->mdio_lock);
	
	#endif

/*bus_register_done:*/
	priv->mii = new_bus;
	
	/*stmmac_mdio_reset(new_bus);*/

	return 0;

bus_register_fail:
	mdiobus_free(new_bus);
	
	return err;
}

/**
 * stmmac_mdio_unregister
 * @ndev: net device structure
 * Description: it unregisters the MII bus
 */
int stmmac_mdio_unregister(struct net_driver_s *ndev)
{
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct stmmac_netdev_priv * nuttx_netdev_priv = netdev_rk_priv(ndev);
	struct phy_device * phydev;

	if (!priv->mii)
		return 0;
		
	if(nuttx_netdev_priv)
	{			
		phydev = (struct phy_device *)(nuttx_netdev_priv->phydev);
	
		if(phydev)
		{
			phy_device_free(phydev);
		}
	}		

	/*mdiobus_unregister(priv->mii);*/
	
	priv->mii->priv = NULL;
	mdiobus_free(priv->mii);
	priv->mii = NULL;

	return 0;
}

static inline int genphy_no_soft_reset(struct phy_device *phydev)
{
	return 0;
}

int __mdiobus_read(struct mii_bus *bus, int addr, u32 regnum)
{
	int retval;

	retval = bus->read(bus, addr, regnum);

	return retval;
}

int mdiobus_read(struct mii_bus *bus, int addr, u32 regnum)
{
	int retval;

	nxmutex_lock(&bus->mdio_lock);
	retval = __mdiobus_read(bus, addr, regnum);
	nxmutex_unlock(&bus->mdio_lock);

	return retval;
}

int phy_read(struct phy_device *phydev, u32 regnum)
{
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	return mdiobus_read(priv->mii, phydev->addr, regnum);
}

int genphy_config_init(struct phy_device *phydev)
{
	int val;
	u32 features;

	features = (SUPPORTED_TP | SUPPORTED_MII
			| SUPPORTED_AUI | SUPPORTED_FIBRE |
			SUPPORTED_BNC | SUPPORTED_Pause | SUPPORTED_Asym_Pause);

	/* Do we support autonegotiation? */
	val = phy_read(phydev, MII_BMSR);
	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;

	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);
		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
	}

	phydev->supported &= features;
	phydev->advertising &= features;

	return 0;
}

int genphy_aneg_done(struct phy_device *phydev)
{
	int retval = phy_read(phydev, MII_BMSR);

	return (retval < 0) ? retval : (retval & BMSR_ANEGCOMPLETE);
}

 int __phy_read(struct phy_device *phydev, u32 regnum)
{
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	return __mdiobus_read(priv->mii, phydev->addr, regnum);
}

int __mdiobus_write(struct mii_bus *bus, int addr, u32 regnum, u16 val)
{
	int err;

	err = bus->write(bus, addr, regnum, val);

	return err;
}

int __mdiobus_modify_changed(struct mii_bus *bus, int addr, u32 regnum,
			     u16 mask, u16 set)
{
	int new, ret;

	ret = __mdiobus_read(bus, addr, regnum);
	if (ret < 0)
		return ret;

	new = (ret & ~mask) | set;
	if (new == ret)
		return 0;

	ret = __mdiobus_write(bus, addr, regnum, new);

	return ret < 0 ? ret : 1;
}

int __phy_write(struct phy_device *phydev, u32 regnum, u16 val)
{
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	return __mdiobus_write(priv->mii, phydev->addr, regnum,
			       val);
}

int __phy_modify(struct phy_device *phydev, u32 regnum, u16 mask, u16 set)
{
	int ret;

	ret = __phy_read(phydev, regnum);
	if (ret < 0)
		return ret;

	ret = __phy_write(phydev, regnum, (ret & ~mask) | set);

	return ret < 0 ? ret : 0;
}

int phy_modify(struct phy_device *phydev, u32 regnum, u16 mask, u16 set)
{
	int ret;
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);

	nxmutex_lock(&priv->mii->mdio_lock);
	ret = __phy_modify(phydev, regnum, mask, set);
	nxmutex_unlock(&priv->mii->mdio_lock);

	return ret;
}

int __phy_modify_changed(struct phy_device *phydev, u32 regnum,
				       u16 mask, u16 set)
{
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	return __mdiobus_modify_changed(priv->mii, phydev->addr,
					regnum, mask, set);
}

int genphy_loopback(struct phy_device *phydev, bool enable)
{
	return phy_modify(phydev, MII_BMCR, BMCR_LOOPBACK,
			  enable ? BMCR_LOOPBACK : 0);
}

static struct phy_driver __attribute__((unused)) genphy_driver = {
	.phy_id		= 0xffffffff,
	.phy_id_mask	= 0xffffffff,
	.name		= "Generic PHY",
	.soft_reset	= genphy_no_soft_reset,
	.config_init	= genphy_config_init,
	.features	= PHY_GBIT_FEATURES | SUPPORTED_MII |
			  SUPPORTED_AUI | SUPPORTED_FIBRE |
			  SUPPORTED_BNC,
	.aneg_done	= genphy_aneg_done,
	.set_loopback   = genphy_loopback,
};

bool phy_interrupt_is_valid(struct phy_device *phydev)
{
	return phydev->irq != PHY_POLL && phydev->irq != PHY_IGNORE_INTERRUPT;
}

void phy_device_reset(struct phy_device *phydev, int value)
{
	/*struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	
	struct stmmac_priv *priv = netdev_priv(ndev);
	
	priv->mii->reset(priv->mii);*/
}

int __set_phy_supported(struct phy_device *phydev, u32 max_speed)
{
	switch (max_speed) {
	case SPEED_10:
		phydev->supported &= ~PHY_100BT_FEATURES;
		/* fall through */
	case SPEED_100:
		phydev->supported &= ~PHY_1000BT_FEATURES;
		break;
	case SPEED_1000:
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

int phy_probe(struct phy_device * phydev)
{
	struct phy_driver *phydrv = (struct phy_driver *)(phydev->drv);
	int err = 0;	
	
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(ndev);
	int max_speed = priv->plat->max_speed;

	/* Disable the interrupt if the PHY doesn't support it
	 * but the interrupt is still a valid one
	 */
	if (!(phydrv->flags & PHY_HAS_INTERRUPT) &&
	    phy_interrupt_is_valid(phydev))
		phydev->irq = PHY_POLL;

	nxmutex_lock(&phydev->lock);

	/* Start out supporting everything. Eventually,
	 * a controller will attach, and may modify one
	 * or both of these values
	 */
	phydev->supported = phydrv->features;
	
	/*of_set_phy_supported(phydev);*/
	
	__set_phy_supported(phydev, max_speed);
	
	phydev->advertising = phydev->supported;

	/* Get the EEE modes we want to prohibit. We will ask
	 * the PHY stop advertising these mode later on
	 */
	/*of_set_phy_eee_broken(phydev);*/

	/* The Pause Frame bits indicate that the PHY can support passing
	 * pause frames. During autonegotiation, the PHYs will determine if
	 * they should allow pause frames to pass.  The MAC driver should then
	 * use that result to determine whether to enable flow control via
	 * pause frames.
	 *
	 * Normally, PHY drivers should not set the Pause bits, and instead
	 * allow phylib to do that.  However, there may be some situations
	 * (e.g. hardware erratum) where the driver wants to set only one
	 * of these bits.
	 */
	if (phydrv->features & (SUPPORTED_Pause | SUPPORTED_Asym_Pause)) {
		phydev->supported &= ~(SUPPORTED_Pause | SUPPORTED_Asym_Pause);
		phydev->supported |= phydrv->features &
				     (SUPPORTED_Pause | SUPPORTED_Asym_Pause);
	} else {
		phydev->supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
	}

	/* Set the state to READY by default */
	phydev->state = PHY_READY;

	/*phy_device_reset(phydev, 0);*/

	nxmutex_unlock(&phydev->lock);

	return err;
}

void phy_remove(struct phy_device * phydev)
{
	cancel_delayed_work_sync(&phydev->state_queue);

	nxmutex_lock(&phydev->lock);
	phydev->state = PHY_DOWN;
	nxmutex_unlock(&phydev->lock);
	
	phy_device_reset(phydev, 1);

	phydev->drv = NULL;
}

int phy_set_bits(struct phy_device *phydev, u32 regnum, u16 val)
{
	return phy_modify(phydev, regnum, 0, val);
}

int phy_poll_reset(struct phy_device *phydev)
{
	/* Poll until the reset bit clears (50ms per retry == 0.6 sec) */
	unsigned int retries = 12;
	int ret;

	do {
		msleep(50);
		ret = phy_read(phydev, MII_BMCR);
		if (ret < 0)
			return ret;
	} while (ret & BMCR_RESET && --retries);
	if (ret & BMCR_RESET)
		return -ETIMEDOUT;

	/* Some chips (smsc911x) may still need up to another 1ms after the
	 * BMCR_RESET bit is cleared before they are usable.
	 */
	msleep(1);
	return 0;
}

int genphy_soft_reset(struct phy_device *phydev)
{
	int ret;

	ret = phy_set_bits(phydev, MII_BMCR, BMCR_RESET);
	if (ret < 0)
		return ret;

	ret = phy_poll_reset(phydev);
	
	return ret;
}

int phy_init_hw(struct phy_device *phydev)
{
	int ret = 0;

	/* Deassert the reset signal */
	phy_device_reset(phydev, 0);

	if (!phydev->drv || !phydev->drv->config_init)
		return 0;

	if (phydev->drv->soft_reset)
		ret = phydev->drv->soft_reset(phydev);
	else
		ret = genphy_soft_reset(phydev);	

	if (ret < 0)
		return ret;

	return phydev->drv->config_init(phydev);
}

void phy_detach(struct phy_device *phydev)
{
	/* Assert the reset signal */
	phy_device_reset(phydev, 1);
}

void phy_prepare_link(struct phy_device *phydev,
			     void (*handler)(struct net_driver_s *))
{
	phydev->adjust_link = handler;
}

bool phy_polling_mode(struct phy_device *phydev)
{
	return phydev->irq == PHY_POLL;
}

int genphy_update_link(struct phy_device *phydev)
{
	int status;

	/* The link state is latched low so that momentary link
	 * drops can be detected. Do not double-read the status
	 * in polling mode to detect such short link drops.
	 */
	if (!phy_polling_mode(phydev)) {
		status = phy_read(phydev, MII_BMSR);
		if (status < 0)
			return status;
	}

	/* Read link and autonegotiation status */
	status = phy_read(phydev, MII_BMSR);
	if (status < 0)
		return status;

	if ((status & BMSR_LSTATUS) == 0)
		phydev->link = 0;
	else
		phydev->link = 1;

	return 0;
}

u32 mii_stat1000_to_ethtool_lpa_t(u32 lpa)
{
	u32 result = 0;

	if (lpa & LPA_1000HALF)
		result |= ADVERTISED_1000baseT_Half;
	if (lpa & LPA_1000FULL)
		result |= ADVERTISED_1000baseT_Full;

	return result;
}

u32 mii_adv_to_ethtool_adv_t(u32 adv)
{
	u32 result = 0;

	if (adv & ADVERTISE_10HALF)
		result |= ADVERTISED_10baseT_Half;
	if (adv & ADVERTISE_10FULL)
		result |= ADVERTISED_10baseT_Full;
	if (adv & ADVERTISE_100HALF)
		result |= ADVERTISED_100baseT_Half;
	if (adv & ADVERTISE_100FULL)
		result |= ADVERTISED_100baseT_Full;
	if (adv & ADVERTISE_PAUSE_CAP)
		result |= ADVERTISED_Pause;
	if (adv & ADVERTISE_PAUSE_ASYM)
		result |= ADVERTISED_Asym_Pause;

	return result;
}

u32 mii_lpa_to_ethtool_lpa_t(u32 lpa)
{
	u32 result = 0;

	if (lpa & LPA_LPACK)
		result |= ADVERTISED_Autoneg;

	return result | mii_adv_to_ethtool_adv_t(lpa);
}

int genphy_read_status(struct phy_device *phydev)
{
	int adv;
	int err;
	int lpa;
	int lpagb = 0;
	int common_adv;
	int common_adv_gb = 0;

	/* Update the link, but return if there was an error */
	err = genphy_update_link(phydev);
	if (err)
		return err;

	phydev->lp_advertising = 0;

	if (AUTONEG_ENABLE == phydev->autoneg) {
		if (phydev->supported & (SUPPORTED_1000baseT_Half
					| SUPPORTED_1000baseT_Full)) {
			lpagb = phy_read(phydev, MII_STAT1000);
			if (lpagb < 0)
				return lpagb;

			adv = phy_read(phydev, MII_CTRL1000);
			if (adv < 0)
				return adv;

			if (lpagb & LPA_1000MSFAIL) {
				if (adv & CTL1000_ENABLE_MASTER)
					netdev_err( (struct net_driver_s *)(phydev->priv), "Master/Slave resolution failed, maybe conflicting manual settings?\n");
				else
					netdev_err( (struct net_driver_s *)(phydev->priv), "Master/Slave resolution failed\n");
				return -ENOLINK;
			}

			phydev->lp_advertising =
				mii_stat1000_to_ethtool_lpa_t(lpagb);
			common_adv_gb = lpagb & adv << 2;
		}

		lpa = phy_read(phydev, MII_LPA);
		if (lpa < 0)
			return lpa;

		phydev->lp_advertising |= mii_lpa_to_ethtool_lpa_t(lpa);

		adv = phy_read(phydev, MII_ADVERTISE);
		if (adv < 0)
			return adv;

		common_adv = lpa & adv;

		phydev->speed = SPEED_10;
		phydev->duplex = DUPLEX_HALF;
		phydev->pause = 0;
		phydev->asym_pause = 0;

		if (common_adv_gb & (LPA_1000FULL | LPA_1000HALF)) {
			phydev->speed = SPEED_1000;

			if (common_adv_gb & LPA_1000FULL)
				phydev->duplex = DUPLEX_FULL;
		} else if (common_adv & (LPA_100FULL | LPA_100HALF)) {
			phydev->speed = SPEED_100;

			if (common_adv & LPA_100FULL)
				phydev->duplex = DUPLEX_FULL;
		} else
			if (common_adv & LPA_10FULL)
				phydev->duplex = DUPLEX_FULL;

		if (phydev->duplex == DUPLEX_FULL) {
			phydev->pause = lpa & LPA_PAUSE_CAP ? 1 : 0;
			phydev->asym_pause = lpa & LPA_PAUSE_ASYM ? 1 : 0;
		}
	} else {
		int bmcr = phy_read(phydev, MII_BMCR);

		if (bmcr < 0)
			return bmcr;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;

		if (bmcr & BMCR_SPEED1000)
			phydev->speed = SPEED_1000;
		else if (bmcr & BMCR_SPEED100)
			phydev->speed = SPEED_100;
		else
			phydev->speed = SPEED_10;

		phydev->pause = 0;
		phydev->asym_pause = 0;
	}

	return 0;
}


int phy_read_status(struct phy_device *phydev)
{
	if (!phydev->drv)
		return -EIO;

	if (phydev->drv->read_status)
		return phydev->drv->read_status(phydev);
	else
		return genphy_read_status(phydev);
}

void phy_link_up(struct phy_device *phydev)
{
	/*phydev->phy_link_change(phydev, true, true);
	phy_led_trigger_change_speed(phydev);*/
	
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	phydev->adjust_link(dev);
	
	if (get_ecdev(priv)) {
		ecdev_set_link(get_ecdev(priv), true);
	}
}

static void phy_link_down(struct phy_device *phydev, bool do_carrier)
{
	/*phydev->phy_link_change(phydev, false, do_carrier);
	phy_led_trigger_change_speed(phydev);*/
	
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	phydev->adjust_link(dev);
	
	if (get_ecdev(priv)) {
		ecdev_set_link(get_ecdev(priv), false);
	}
}

int phy_aneg_done(struct phy_device *phydev)
{
	if (phydev->drv && phydev->drv->aneg_done)
		return phydev->drv->aneg_done(phydev);

	return genphy_aneg_done(phydev);
}

static const struct phy_setting settings[] = {
	{
		.speed = SPEED_10000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	},
	{
		.speed = SPEED_10000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	},
	{
		.speed = SPEED_10000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
	},
	{
		.speed = SPEED_2500,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_HALF,
		.bit = ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
	},
	{
		.speed = SPEED_100,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	},
	{
		.speed = SPEED_100,
		.duplex = DUPLEX_HALF,
		.bit = ETHTOOL_LINK_MODE_100baseT_Half_BIT,
	},
	{
		.speed = SPEED_10,
		.duplex = DUPLEX_FULL,
		.bit = ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	},
	{
		.speed = SPEED_10,
		.duplex = DUPLEX_HALF,
		.bit = ETHTOOL_LINK_MODE_10baseT_Half_BIT,
	},
};

const struct phy_setting *
phy_lookup_setting(int speed, int duplex, const unsigned long *mask,
		   size_t maxbit, bool exact)
{
	const struct phy_setting *p, *match = NULL, *last = NULL;
	int i;

	for (i = 0, p = settings; i < ARRAY_SIZE(settings); i++, p++) {
		if (p->bit < maxbit && test_bit(p->bit, mask)) {
			last = p;
			if (p->speed == speed && p->duplex == duplex) {
				/* Exact match for speed and duplex */
				match = p;
				break;
			} else if (!exact) {
				if (!match && p->speed <= speed)
					/* Candidate */
					match = p;

				if (p->speed < speed)
					break;
			}
		}
	}

	if (!match && !exact)
		match = last;

	return match;
}

const struct phy_setting *
phy_find_valid(int speed, int duplex, u32 supported)
{
	unsigned long mask = supported;

	return phy_lookup_setting(speed, duplex, &mask, BITS_PER_LONG, false);
}

void phy_sanitize_settings(struct phy_device *phydev)
{
	const struct phy_setting *setting;
	u32 features = phydev->supported;

	/* Sanitize settings based on PHY capabilities */
	if ((features & SUPPORTED_Autoneg) == 0)
		phydev->autoneg = AUTONEG_DISABLE;

	setting = phy_find_valid(phydev->speed, phydev->duplex, features);
	if (setting) {
		phydev->speed = setting->speed;
		phydev->duplex = setting->duplex;
	} else {
		/* We failed to find anything (no supported speeds?) */
		phydev->speed = SPEED_UNKNOWN;
		phydev->duplex = DUPLEX_UNKNOWN;
	}
}

int genphy_config_eee_advert(struct phy_device *phydev)
{
	#if 0
	
	int broken = phydev->eee_broken_modes;
	int old_adv, adv;

	/* Nothing to disable */
	if (!broken)
		return 0;

	/* If the following call fails, we assume that EEE is not
	 * supported by the phy. If we read 0, EEE is not advertised
	 * In both case, we don't need to continue
	 */
	adv = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV);
	if (adv <= 0)
		return 0;

	old_adv = adv;
	adv &= ~broken;

	/* Advertising remains unchanged with the broken mask */
	if (old_adv == adv)
		return 0;

	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV, adv);

	return 1;
	
	#else
	
	return 0;
	
	#endif
}

int genphy_setup_forced(struct phy_device *phydev)
{
	u16 ctl = 0;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (SPEED_1000 == phydev->speed)
		ctl |= BMCR_SPEED1000;
	else if (SPEED_100 == phydev->speed)
		ctl |= BMCR_SPEED100;

	if (DUPLEX_FULL == phydev->duplex)
		ctl |= BMCR_FULLDPLX;

	return phy_modify(phydev, MII_BMCR,
			  ~(BMCR_LOOPBACK | BMCR_ISOLATE | BMCR_PDOWN), ctl);
}

u32 ethtool_adv_to_mii_adv_t(u32 ethadv)
{
	u32 result = 0;

	if (ethadv & ADVERTISED_10baseT_Half)
		result |= ADVERTISE_10HALF;
	if (ethadv & ADVERTISED_10baseT_Full)
		result |= ADVERTISE_10FULL;
	if (ethadv & ADVERTISED_100baseT_Half)
		result |= ADVERTISE_100HALF;
	if (ethadv & ADVERTISED_100baseT_Full)
		result |= ADVERTISE_100FULL;
	if (ethadv & ADVERTISED_Pause)
		result |= ADVERTISE_PAUSE_CAP;
	if (ethadv & ADVERTISED_Asym_Pause)
		result |= ADVERTISE_PAUSE_ASYM;

	return result;
}

int mdiobus_write(struct mii_bus *bus, int addr, u32 regnum, u16 val)
{
	int err;

	nxmutex_lock(&bus->mdio_lock);
	err = __mdiobus_write(bus, addr, regnum, val);
	nxmutex_unlock(&bus->mdio_lock);

	return err;
}

int phy_write(struct phy_device *phydev, u32 regnum, u16 val)
{
	struct net_driver_s * dev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(dev);
	
	return mdiobus_write(priv->mii, phydev->addr, regnum, val);
}

u32 ethtool_adv_to_mii_ctrl1000_t(u32 ethadv)
{
	u32 result = 0;

	if (ethadv & ADVERTISED_1000baseT_Half)
		result |= ADVERTISE_1000HALF;
	if (ethadv & ADVERTISED_1000baseT_Full)
		result |= ADVERTISE_1000FULL;

	return result;
}

int genphy_restart_aneg(struct phy_device *phydev)
{
	/* Don't isolate the PHY if we're negotiating */
	return phy_modify(phydev, MII_BMCR, BMCR_ISOLATE,
			  BMCR_ANENABLE | BMCR_ANRESTART);
}

int genphy_config_advert(struct phy_device *phydev)
{
	u32 advertise;
	int oldadv, adv, bmsr;
	int err, changed = 0;

	/* Only allow advertising what this PHY supports */
	phydev->advertising &= phydev->supported;
	advertise = phydev->advertising;

	/* Setup standard advertisement */
	adv = phy_read(phydev, MII_ADVERTISE);
	if (adv < 0)
		return adv;

	oldadv = adv;
	adv &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4 | ADVERTISE_PAUSE_CAP |
		 ADVERTISE_PAUSE_ASYM);
	adv |= ethtool_adv_to_mii_adv_t(advertise);

	if (adv != oldadv) {
		err = phy_write(phydev, MII_ADVERTISE, adv);

		if (err < 0)
			return err;
		changed = 1;
	}

	bmsr = phy_read(phydev, MII_BMSR);
	if (bmsr < 0)
		return bmsr;

	/* Per 802.3-2008, Section 22.2.4.2.16 Extended status all
	 * 1000Mbits/sec capable PHYs shall have the BMSR_ESTATEN bit set to a
	 * logical 1.
	 */
	if (!(bmsr & BMSR_ESTATEN))
		return changed;

	/* Configure gigabit if it's supported */
	adv = phy_read(phydev, MII_CTRL1000);
	if (adv < 0)
		return adv;

	oldadv = adv;
	adv &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

	if (phydev->supported & (SUPPORTED_1000baseT_Half |
				 SUPPORTED_1000baseT_Full)) {
		adv |= ethtool_adv_to_mii_ctrl1000_t(advertise);
	}

	if (adv != oldadv)
		changed = 1;

	err = phy_write(phydev, MII_CTRL1000, adv);
	if (err < 0)
		return err;

	return changed;
}

int genphy_config_aneg(struct phy_device *phydev)
{
	int err, changed;

	changed = genphy_config_eee_advert(phydev);

	if (AUTONEG_ENABLE != phydev->autoneg)
		return genphy_setup_forced(phydev);

	err = genphy_config_advert(phydev);
	if (err < 0) /* error */
		return err;

	changed |= err;

	if (changed == 0) {
		/* Advertisement hasn't changed, but maybe aneg was never on to
		 * begin with?  Or maybe phy was isolated?
		 */
		int ctl = phy_read(phydev, MII_BMCR);

		if (ctl < 0)
			return ctl;

		if (!(ctl & BMCR_ANENABLE) || (ctl & BMCR_ISOLATE))
			changed = 1; /* do restart aneg */
	}

	/* Only restart aneg if we are advertising something different
	 * than we were before.
	 */
	if (changed > 0)
		return genphy_restart_aneg(phydev);

	return 0;
}

int phy_config_aneg(struct phy_device *phydev)
{
	if (phydev->drv->config_aneg)
		return phydev->drv->config_aneg(phydev);

	return genphy_config_aneg(phydev);
}

void phy_trigger_machine(struct phy_device *phydev, bool sync)
{
	if (sync)
		cancel_delayed_work_sync(&phydev->state_queue);
	else
		cancel_delayed_work(&phydev->state_queue);
		
	queue_delayed_work(&phydev->state_queue, 0);
}

int phy_start_aneg_priv(struct phy_device *phydev, bool sync)
{
	bool trigger = 0;
	int err;

	if (!phydev->drv)
		return -EIO;

	nxmutex_lock(&phydev->lock);

	if (AUTONEG_DISABLE == phydev->autoneg)
		phy_sanitize_settings(phydev);

	/* Invalidate LP advertising flags */
	phydev->lp_advertising = 0;

	err = phy_config_aneg(phydev);
	if (err < 0)
		goto out_unlock;

	if (phydev->state != PHY_HALTED) {
		if (AUTONEG_ENABLE == phydev->autoneg) {
			phydev->state = PHY_AN;
			phydev->link_timeout = PHY_AN_TIMEOUT;
		} else {
			phydev->state = PHY_FORCING;
			phydev->link_timeout = PHY_FORCE_TIMEOUT;
		}
	}

	/* Re-schedule a PHY state machine to check PHY status because
	 * negotiation may already be done and aneg interrupt may not be
	 * generated.
	 */
	if (!phy_polling_mode(phydev) && phydev->state == PHY_AN) {
		err = phy_aneg_done(phydev);
		if (err > 0) {
			trigger = true;
			err = 0;
		}
	}

out_unlock:
	nxmutex_unlock(&phydev->lock);

	if (trigger)
		phy_trigger_machine(phydev, sync);

	return err;
}

void phy_error(struct phy_device *phydev)
{
	nxmutex_lock(&phydev->lock);
	phydev->state = PHY_HALTED;
	nxmutex_unlock(&phydev->lock);

	phy_trigger_machine(phydev, false);
}

#define PHY_STATE_STR(_state)			\
	case PHY_##_state:			\
		return __stringify(_state);	\

static const char *phy_state_to_str(enum phy_state st)
{
	switch (st) {
	PHY_STATE_STR(DOWN)
	PHY_STATE_STR(STARTING)
	PHY_STATE_STR(READY)
	PHY_STATE_STR(PENDING)
	PHY_STATE_STR(UP)
	PHY_STATE_STR(AN)
	PHY_STATE_STR(RUNNING)
	PHY_STATE_STR(NOLINK)
	PHY_STATE_STR(FORCING)
	PHY_STATE_STR(CHANGELINK)
	PHY_STATE_STR(HALTED)
	PHY_STATE_STR(RESUMING)
	}

	return NULL;
}


void phy_state_machine(void * arg)
{
	struct work_struct *work = (struct work_struct *)arg;
	struct phy_device *phydev = (struct phy_device *)(work->data);
	bool needs_aneg = false;
	#if 0
	
	bool do_suspend = false;
	
	#endif
	
	enum phy_state old_state;
	int err = 0;
	int old_link;
	
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	
	nxmutex_lock(&phydev->lock);
	
	old_state = phydev->state;

	switch (phydev->state) {
	case PHY_DOWN:
	case PHY_STARTING:
	case PHY_READY:
	case PHY_PENDING:
		break;
	case PHY_UP:
		needs_aneg = true;

		phydev->link_timeout = PHY_AN_TIMEOUT;

		break;
	case PHY_AN:
		err = phy_read_status(phydev);
		if (err < 0)
			break;
			
		/* If the link is down, give up on negotiation for now */
		if (!phydev->link) {
			phydev->state = PHY_NOLINK;
			phy_link_down(phydev, true);
			break;
		}

		/* Check if negotiation is done.  Break if there's an error */
		err = phy_aneg_done(phydev);
		if (err < 0)
			break;

		/* If AN is done, we're running */
		if (err > 0) {
			phydev->state = PHY_RUNNING;
			phy_link_up(phydev);
		} else if (0 == phydev->link_timeout--)
			needs_aneg = true;
		break;
	case PHY_NOLINK:
		if (!phy_polling_mode(phydev))
			break;

		err = phy_read_status(phydev);
		if (err)
			break;

		if (phydev->link) {
			if (AUTONEG_ENABLE == phydev->autoneg) {
				err = phy_aneg_done(phydev);
				if (err < 0)
					break;

				if (!err) {
					phydev->state = PHY_AN;
					phydev->link_timeout = PHY_AN_TIMEOUT;
					break;
				}
			}
			phydev->state = PHY_RUNNING;
			phy_link_up(phydev);
		}
		break;
	case PHY_FORCING:
		err = genphy_update_link(phydev);
		if (err)
			break;

		if (phydev->link) {
			phydev->state = PHY_RUNNING;
			phy_link_up(phydev);
		} else {
			if (0 == phydev->link_timeout--)
				needs_aneg = true;
			phy_link_down(phydev, false);
		}
		break;
	case PHY_RUNNING:
		/* Only register a CHANGE if we are polling and link changed
		 * since latest checking.
		 */
		if (phy_polling_mode(phydev)) {
			old_link = phydev->link;
			err = phy_read_status(phydev);
			if (err)
				break;

			if (old_link != phydev->link)
				phydev->state = PHY_CHANGELINK;
		}
		/*
		 * Failsafe: check that nobody set phydev->link=0 between two
		 * poll cycles, otherwise we won't leave RUNNING state as long
		 * as link remains down.
		 */
		if (!phydev->link && phydev->state == PHY_RUNNING) {
			phydev->state = PHY_CHANGELINK;
			netdev_err(ndev, "no link in PHY_RUNNING\n");
		}
		break;
	case PHY_CHANGELINK:
		err = phy_read_status(phydev);
		if (err)
			break;

		if (phydev->link) {
			phydev->state = PHY_RUNNING;
			phy_link_up(phydev);
		} else {
			phydev->state = PHY_NOLINK;
			phy_link_down(phydev, true);
		}
		break;
	case PHY_HALTED:
	
		#if 1
		
		if (phydev->link) {
			phydev->link = 0;
			phy_link_down(phydev, true);
			/*do_suspend = true;*/
		}
		
		#endif
		
		break;
	case PHY_RESUMING:
	
	    #if 1
	    
		if (AUTONEG_ENABLE == phydev->autoneg) {
			err = phy_aneg_done(phydev);
			if (err < 0)
				break;

			/* err > 0 if AN is done.
			 * Otherwise, it's 0, and we're  still waiting for AN
			 */
			if (err > 0) {
				err = phy_read_status(phydev);
				if (err)
					break;

				if (phydev->link) {
					phydev->state = PHY_RUNNING;
					phy_link_up(phydev);
				} else	{
					phydev->state = PHY_NOLINK;
					phy_link_down(phydev, false);
				}
			} else {
				phydev->state = PHY_AN;
				phydev->link_timeout = PHY_AN_TIMEOUT;
			}
		} else {
			err = phy_read_status(phydev);
			if (err)
				break;

			if (phydev->link) {
				phydev->state = PHY_RUNNING;
				phy_link_up(phydev);
			} else	{
				phydev->state = PHY_NOLINK;
				phy_link_down(phydev, false);
			}
		}
		
		#endif
		
		break;
	}

	nxmutex_unlock(&phydev->lock);

	if (needs_aneg)
	{
		err = phy_start_aneg_priv(phydev, false);
	}	
	#if 0	
	else if (do_suspend)
		phy_suspend(phydev);
	#endif	

	if (err < 0)
		phy_error(phydev);

	if (old_state != phydev->state)
		netdev_dbg(ndev, "PHY state change %s -> %s\n",
			   phy_state_to_str(old_state),
			   phy_state_to_str(phydev->state));

	/* Only re-schedule a PHY state machine change if we are polling the
	 * PHY, if PHY_IGNORE_INTERRUPT is set, then we will be moving
	 * between states from phy_mac_interrupt()
	 */
	if (phy_polling_mode(phydev))
	{
		queue_delayed_work(&phydev->state_queue,
				   PHY_STATE_TIME * TICK_PER_SEC);			   
	}			   
}



void phy_start_machine(struct phy_device *phydev)
{
	queue_delayed_work(&phydev->state_queue, TICK_PER_SEC);
	
	/*queue_delayed_work(system_power_efficient_wq, &phydev->state_queue, HZ);*/
}

int phy_attach_direct(struct net_driver_s * dev, struct phy_device *phydev,
		      u32 flags, phy_interface_t interface)
{
	int err;

	/* Assume that if there is no driver, that it doesn't
	 * exist, and we should use the genphy driver.
	 */
	phydev->drv = &realtek_drv;
		
	phydev->drv->probe = phy_probe;
		
	phydev->drv->remove = phy_remove;
 
	err = phydev->drv->probe(phydev);
	if (err)
		goto error_module_put;

	phydev->dev_flags = flags;

	phydev->interface = interface;

	phydev->state = PHY_READY;

	/* Do initial configuration here, now that
	 * we have certain key parameters
	 * (dev_flags and interface)
	 */
	err = phy_init_hw(phydev);
	if (err)
		goto error;

	return err;

error:
	/* phy_detach() does all of the cleanup below */
	phy_detach(phydev);
	return err;

error_module_put:

	return err;
}

int phy_connect_direct(struct net_driver_s *dev, struct phy_device *phydev,
		       void (*handler)(struct net_driver_s *),
		       phy_interface_t interface)
{
	int rc;

	if (!dev)
		return -EINVAL;

	rc = phy_attach_direct(dev, phydev, phydev->dev_flags, interface);
	if (rc)
		return rc;

	phy_prepare_link(phydev, handler);
	phy_start_machine(phydev);
	
	#if 0
	
	if (phydev->irq > 0)
		phy_start_interrupts(phydev);
		
	#endif	

	return 0;
}

struct phy_device *phy_connect(struct net_driver_s * dev, const char *bus_id,
			       void (*handler)(struct net_driver_s *),
			       phy_interface_t interface)
{
	struct stmmac_netdev_priv * nuttx_netdev_priv = netdev_rk_priv(dev);
	struct phy_device *phydev;
	int rc = 0;

	/* Search the list of PHY devices on the mdio bus for the
	 * PHY with the requested name
	 */
	phydev = (nuttx_netdev_priv->phydev);

	rc = phy_connect_direct(dev, phydev, handler, interface);
	if (rc)
		return ERR_PTR(rc);

	return phydev;
}

void phy_stop_machine(struct phy_device *phydev)
{
	cancel_delayed_work_sync(&phydev->state_queue);

	nxmutex_lock(&phydev->lock);
	if (phydev->state > PHY_UP && phydev->state != PHY_HALTED)
		phydev->state = PHY_UP;
	nxmutex_unlock(&phydev->lock);
}

void phy_disconnect(struct phy_device *phydev)
{
	/*if (phydev->irq > 0)
		phy_stop_interrupts(phydev);*/

	phy_stop_machine(phydev);

	phydev->adjust_link = NULL;

	phy_detach(phydev);
}

#define ATTACHED_FMT "attached PHY driver [%s] (irq=%s)"

void phy_attached_print(struct phy_device *phydev, const char *fmt, ...)
{
	const char *drv_name = phydev->drv ? phydev->drv->name : "unbound";
	char *irq_str;
	char irq_num[8];

	switch(phydev->irq) {
	case PHY_POLL:
		irq_str = "POLL";
		break;
	case PHY_IGNORE_INTERRUPT:
		irq_str = "IGNORE";
		break;
	default:
		snprintf(irq_num, sizeof(irq_num), "%d", phydev->irq);
		irq_str = irq_num;
		break;
	}


	if (!fmt) {
		pr_info(ATTACHED_FMT "\n",
			 drv_name, irq_str);
	} else {
		va_list ap;

		pr_info(ATTACHED_FMT,
			 drv_name, irq_str);

		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

void phy_attached_info(struct phy_device *phydev)
{
	phy_attached_print(phydev, NULL);
}

void phy_stop(struct phy_device *phydev)
{
	nxmutex_lock(&phydev->lock);

	if (PHY_HALTED == phydev->state)
		goto out_unlock;

	/*if (phy_interrupt_is_valid(phydev))
		phy_disable_interrupts(phydev);*/

	phydev->state = PHY_HALTED;

out_unlock:
	nxmutex_unlock(&phydev->lock);

	/* Cannot call flush_scheduled_work() here as desired because
	 * of rtnl_lock(), but PHY_HALTED shall guarantee phy_change()
	 * will not reenable interrupts.
	 */
}

const char *phy_speed_to_str(int speed)
{
	switch (speed) {
	case SPEED_10:
		return "10Mbps";
	case SPEED_100:
		return "100Mbps";
	case SPEED_1000:
		return "1Gbps";
	case SPEED_2500:
		return "2.5Gbps";
	case SPEED_5000:
		return "5Gbps";
	case SPEED_10000:
		return "10Gbps";
	case SPEED_14000:
		return "14Gbps";
	case SPEED_20000:
		return "20Gbps";
	case SPEED_25000:
		return "25Gbps";
	case SPEED_40000:
		return "40Gbps";
	case SPEED_50000:
		return "50Gbps";
	case SPEED_56000:
		return "56Gbps";
	case SPEED_100000:
		return "100Gbps";
	case SPEED_UNKNOWN:
		return "Unknown";
	default:
		return "Unsupported (update phy-core.c)";
	}
}

const char *phy_duplex_to_str(unsigned int duplex)
{
	if (duplex == DUPLEX_HALF)
		return "Half";
	if (duplex == DUPLEX_FULL)
		return "Full";
	if (duplex == DUPLEX_UNKNOWN)
		return "Unknown";
	return "Unsupported (update phy-core.c)";
}

void phy_print_status(struct phy_device *phydev)
{
	struct net_driver_s *ndev = (struct net_driver_s *)(phydev->priv);
	
	if (phydev->link) {
		netdev_info(ndev,
			"Link is Up - %s/%s - flow control %s\n",
			phy_speed_to_str(phydev->speed),
			phy_duplex_to_str(phydev->duplex),
			phydev->pause ? "rx/tx" : "off");
	} else	{
		netdev_info(ndev, "Link is Down\n");
	}
}

void phy_start(struct phy_device *phydev)
{
	/*int err = 0;*/

	nxmutex_lock(&phydev->lock);

	switch (phydev->state) {
	case PHY_STARTING:
		phydev->state = PHY_PENDING;
		break;
	case PHY_READY:
		phydev->state = PHY_UP;
		break;
	case PHY_HALTED:
	
		#if 0
		
		/* if phy was suspended, bring the physical link up again */
		__phy_resume(phydev);

		/* make sure interrupts are re-enabled for the PHY */
		if (phy_interrupt_is_valid(phydev)) {
			err = phy_enable_interrupts(phydev);
			if (err < 0)
				break;
		}*/
		
		#endif

		phydev->state = PHY_RESUMING;
		
		break;
	default:
		break;
	}
	nxmutex_unlock(&phydev->lock);

	phy_trigger_machine(phydev, true);
}

void mmd_phy_indirect(struct mii_bus *bus, int phy_addr, int devad,
			     u16 regnum)
{
	/* Write the desired MMD Devad */
	__mdiobus_write(bus, phy_addr, MII_MMD_CTRL, devad);

	/* Write the desired MMD register address */
	__mdiobus_write(bus, phy_addr, MII_MMD_DATA, regnum);

	/* Select the Function : DATA with no post increment */
	__mdiobus_write(bus, phy_addr, MII_MMD_CTRL,
			devad | MII_MMD_CTRL_NOINCR);
}

int phy_read_mmd(struct phy_device *phydev, int devad, u32 regnum)
{
	int val;
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct mii_bus * bus = priv->mii;

	if (regnum > (u16)~0 || devad > 32)
		return -EINVAL;

	/*if (phydev->drv->read_mmd) {
		val = phydev->drv->read_mmd(phydev, devad, regnum);
	} else if (phydev->is_c45) {
		u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);

		val = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr, addr);
	} 
	else*/
	{
		int phy_addr = phydev->addr;

		nxmutex_lock(&bus->mdio_lock);
		mmd_phy_indirect(bus, phy_addr, devad, regnum);

		/* Read the content of the MMD's selected register */
		val = __mdiobus_read(bus, phy_addr, MII_MMD_DATA);
		nxmutex_unlock(&bus->mdio_lock);
	}
	return val;
}

u32 mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap)
{
	u32 supported = 0;

	if (eee_cap & MDIO_EEE_100TX)
		supported |= SUPPORTED_100baseT_Full;
	if (eee_cap & MDIO_EEE_1000T)
		supported |= SUPPORTED_1000baseT_Full;
	if (eee_cap & MDIO_EEE_10GT)
		supported |= SUPPORTED_10000baseT_Full;
	if (eee_cap & MDIO_EEE_1000KX)
		supported |= SUPPORTED_1000baseKX_Full;
	if (eee_cap & MDIO_EEE_10GKX4)
		supported |= SUPPORTED_10000baseKX4_Full;
	if (eee_cap & MDIO_EEE_10GKR)
		supported |= SUPPORTED_10000baseKR_Full;

	return supported;
}

u32 mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv)
{
	u32 adv = 0;

	if (eee_adv & MDIO_EEE_100TX)
		adv |= ADVERTISED_100baseT_Full;
	if (eee_adv & MDIO_EEE_1000T)
		adv |= ADVERTISED_1000baseT_Full;
	if (eee_adv & MDIO_EEE_10GT)
		adv |= ADVERTISED_10000baseT_Full;
	if (eee_adv & MDIO_EEE_1000KX)
		adv |= ADVERTISED_1000baseKX_Full;
	if (eee_adv & MDIO_EEE_10GKX4)
		adv |= ADVERTISED_10000baseKX4_Full;
	if (eee_adv & MDIO_EEE_10GKR)
		adv |= ADVERTISED_10000baseKR_Full;

	return adv;
}

bool phy_check_valid(int speed, int duplex, u32 features)
{
	unsigned long mask = features;

	return !!phy_lookup_setting(speed, duplex, &mask, BITS_PER_LONG, true);
}

int phy_write_mmd(struct phy_device *phydev, int devad, u32 regnum, u16 val)
{
	int ret;
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct mii_bus * bus = priv->mii;

	if (regnum > (u16)~0 || devad > 32)
		return -EINVAL;

	/*if (phydev->drv->write_mmd) {
		ret = phydev->drv->write_mmd(phydev, devad, regnum, val);
	} else if (phydev->is_c45) {
		u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);

		ret = mdiobus_write(phydev->mdio.bus, phydev->mdio.addr,
				    addr, val);
	} else*/
	{
		int phy_addr = phydev->addr;

		nxmutex_lock(&bus->mdio_lock);
		mmd_phy_indirect(bus, phy_addr, devad, regnum);

		/* Write the data into MMD's selected register */
		__mdiobus_write(bus, phy_addr, MII_MMD_DATA, val);
		nxmutex_unlock(&bus->mdio_lock);

		ret = 0;
	}
	return ret;
}

int phy_init_eee(struct phy_device *phydev, bool clk_stop_enable)
{
	if (!phydev->drv)
		return -EIO;

	/* According to 802.3az,the EEE is supported only in full duplex-mode.
	 */
	if (phydev->duplex == DUPLEX_FULL) {
		int eee_lp, eee_cap, eee_adv;
		u32 lp, cap, adv;
		int status;

		/* Read phy status to properly get the right settings */
		status = phy_read_status(phydev);
		if (status)
			return status;

		/* First check if the EEE ability is supported */
		eee_cap = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_PCS_EEE_ABLE);
		if (eee_cap <= 0)
			goto eee_exit_err;

		cap = mmd_eee_cap_to_ethtool_sup_t(eee_cap);
		if (!cap)
			goto eee_exit_err;

		/* Check which link settings negotiated and verify it in
		 * the EEE advertising registers.
		 */
		eee_lp = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_LPABLE);
		if (eee_lp <= 0)
			goto eee_exit_err;

		eee_adv = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV);
		if (eee_adv <= 0)
			goto eee_exit_err;

		adv = mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		lp = mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		if (!phy_check_valid(phydev->speed, phydev->duplex, lp & adv))
			goto eee_exit_err;

		if (clk_stop_enable) {
			/* Configure the PHY to stop receiving xMII
			 * clock while it is signaling LPI.
			 */
			int val = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
			if (val < 0)
				return val;

			val |= MDIO_PCS_CTRL1_CLKSTOP_EN;
			phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, val);
		}

		return 0; /* EEE supported */
	}
eee_exit_err:
	return -EPROTONOSUPPORT;
}

int __phy_read_page(struct phy_device *phydev)
{
	return phydev->drv->read_page(phydev);
}

static int __phy_write_page(struct phy_device *phydev, int page)
{
	return phydev->drv->write_page(phydev, page);
}

int phy_save_page(struct phy_device *phydev)
{
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct mii_bus * bus = priv->mii;
	
	nxmutex_lock( &(bus->mdio_lock) );
	return __phy_read_page(phydev);
}

int phy_select_page(struct phy_device *phydev, int page)
{
	int ret, oldpage;

	oldpage = ret = phy_save_page(phydev);
	if (ret < 0)
		return ret;

	if (oldpage != page) {
		ret = __phy_write_page(phydev, page);
		if (ret < 0)
			return ret;
	}

	return oldpage;
}

int phy_restore_page(struct phy_device *phydev, int oldpage, int ret)
{
	int r;
	struct net_driver_s * ndev = (struct net_driver_s *)(phydev->priv);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct mii_bus * bus = priv->mii;

	if (oldpage >= 0) {
		r = __phy_write_page(phydev, oldpage);

		/* Propagate the operation return code if the page write
		 * was successful.
		 */
		if (ret >= 0 && r < 0)
			ret = r;
	} else {
		/* Propagate the phy page selection error code */
		ret = oldpage;
	}

	nxmutex_unlock(&(bus->mdio_lock) );

	return ret;
}

int phy_modify_paged(struct phy_device *phydev, int page, u32 regnum,
		     u16 mask, u16 set)
{
	int ret = 0, oldpage;

	oldpage = phy_select_page(phydev, page);
	if (oldpage >= 0)
		ret = __phy_modify(phydev, regnum, mask, set);

	return phy_restore_page(phydev, oldpage, ret);
}

int phy_read_paged(struct phy_device *phydev, int page, u32 regnum)
{
	int ret = 0, oldpage;

	oldpage = phy_select_page(phydev, page);
	if (oldpage >= 0)
		ret = __phy_read(phydev, regnum);

	return phy_restore_page(phydev, oldpage, ret);
}

int phy_write_paged(struct phy_device *phydev, int page, u32 regnum, u16 val)
{
	int ret = 0, oldpage;

	oldpage = phy_select_page(phydev, page);
	if (oldpage >= 0)
		ret = __phy_write(phydev, regnum, val);

	return phy_restore_page(phydev, oldpage, ret);
}

int phy_modify_paged_changed(struct phy_device *phydev, int page, u32 regnum,
			     u16 mask, u16 set)
{
	int ret = 0, oldpage;

	oldpage = phy_select_page(phydev, page);
	if (oldpage >= 0)
		ret = __phy_modify_changed(phydev, regnum, mask, set);

	return phy_restore_page(phydev, oldpage, ret);
}

