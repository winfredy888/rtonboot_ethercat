#include <nuttx/config.h>

#include <stdio.h>

#include "stmmac_config.h"

#include "linux_phy.h"

#define RTL8211F_ALDPS_PLL_OFF			BIT(1)
#define RTL8211F_ALDPS_ENABLE			BIT(2)
#define RTL8211F_ALDPS_XTAL_OFF			BIT(12)

#define RTL8211F_PHYCR1				0x18

#define RTL8211F_TX_DELAY			BIT(8)
#define RTL8211F_RX_DELAY			BIT(3)

#define RTL8211F_INER_LINK_STATUS		BIT(4)

#define RTL821x_INER				0x12
#define RTL8211F_INSR				0x1d

#define RTL821x_PAGE_SELECT			0x1f

extern int __phy_read(struct phy_device *phydev, u32 regnum);

extern int __phy_write(struct phy_device *phydev, u32 regnum, u16 val);

extern int phy_write_paged(struct phy_device *phydev, int page, u32 regnum, u16 val);

extern int phy_read_paged(struct phy_device *phydev, int page, u32 regnum);

extern int phy_write(struct phy_device *phydev, u32 regnum, u16 val);

extern int phy_modify_paged(struct phy_device *phydev, int page, u32 regnum,
		     u16 mask, u16 set);
		     
extern int phy_modify_paged_changed(struct phy_device *phydev, int page, u32 regnum,
			     u16 mask, u16 set);		     
		     
extern int genphy_config_init(struct phy_device *phydev);		     

static int rtl821x_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, RTL821x_PAGE_SELECT);
}

static int rtl821x_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, RTL821x_PAGE_SELECT, page);
}

int rtl8211f_config_intr(struct phy_device *phydev)
{
	u16 val;

    if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = RTL8211F_INER_LINK_STATUS;
	else
		val = 0;

	return phy_write_paged(phydev, 0xa42, RTL821x_INER, val);
}

int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read_paged(phydev, 0xa43, RTL8211F_INSR);

	return (err < 0) ? err : 0;
}

int rtl8211f_config_init(struct phy_device *phydev)
{
    #if 0
    
    int ret;
	u16 val = 0;
    
    ret = genphy_config_init(phydev);
    
	//OK3568 net LED set
	phy_write(phydev, 31, 0xd04);     //page to 0xd04 to set LED
	phy_write(phydev, 0x10, 0x6d68);  //set LED 0\1 alive and LED2 active
	phy_write(phydev, 31, 0xa42);     //page to 0xa42 for end LED set

	if (ret < 0)
		return ret;

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		val = RTL8211F_TX_DELAY;

	return phy_modify_paged(phydev, 0xd08, 0x11, RTL8211F_TX_DELAY, val);
	
	#else
	
	u16 val;
	u16 val_txdly, val_rxdly;
	int ret;
	
	ret = genphy_config_init(phydev);
	
	val = RTL8211F_ALDPS_ENABLE | RTL8211F_ALDPS_PLL_OFF | RTL8211F_ALDPS_XTAL_OFF;
	phy_modify_paged_changed(phydev, 0xa43, RTL8211F_PHYCR1, val, val);
	
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		val_txdly = 0;
		val_rxdly = 0;
		break;

	case PHY_INTERFACE_MODE_RGMII_RXID:
		val_txdly = 0;
		val_rxdly = RTL8211F_RX_DELAY;
		break;

	case PHY_INTERFACE_MODE_RGMII_TXID:
		val_txdly = RTL8211F_TX_DELAY;
		val_rxdly = 0;
		break;

	case PHY_INTERFACE_MODE_RGMII_ID:
		val_txdly = RTL8211F_TX_DELAY;
		val_rxdly = RTL8211F_RX_DELAY;
		break;

	default: /* the rest of the modes imply leaving delay as is. */
		return 0;
	}
	
	ret = phy_modify_paged_changed(phydev, 0xd08, 0x11, RTL8211F_TX_DELAY,
				       val_txdly);
	if (ret < 0) {
		printk(KERN_ERR, "Failed to update the TX delay register\n");
		return ret;
	} else if (ret) {
		printk(KERN_DEBUG, 
			"%s 2ns TX delay (and changing the value from pin-strapping RXD1 or the bootloader)\n",
			val_txdly ? "Enabling" : "Disabling");
	} else {
		printk(KERN_DEBUG, 
			"2ns TX delay was already %s (by pin-strapping RXD1 or bootloader configuration)\n",
			val_txdly ? "enabled" : "disabled");
	}

	ret = phy_modify_paged_changed(phydev, 0xd08, 0x15, RTL8211F_RX_DELAY,
				       val_rxdly);
	if (ret < 0) {
		printk(KERN_ERR, "Failed to update the RX delay register\n");
		return ret;
	} else if (ret) {
		printk(KERN_DEBUG, 
			"%s 2ns RX delay (and changing the value from pin-strapping RXD0 or the bootloader)\n",
			val_rxdly ? "Enabling" : "Disabling");
	} else {
		printk(KERN_DEBUG, 
			"2ns RX delay was already %s (by pin-strapping RXD0 or bootloader configuration)\n",
			val_rxdly ? "enabled" : "disabled");
	}

	return 0;
	
	#endif
}

struct phy_driver realtek_drv = {
	.phy_id		= 0x001cc916,
	.name		= "RTL8211F Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &rtl8211f_config_init,
	.ack_interrupt	= &rtl8211f_ack_interrupt,
	.config_intr	= &rtl8211f_config_intr,
	.read_page	= rtl821x_read_page,
	.write_page	= rtl821x_write_page,
};
