#ifndef __EC_LINUX_PHY_H__
#define __EC_LINUX_PHY_H__

#include "linux_mii.h"

#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_2500		2500
#define SPEED_5000		5000
#define SPEED_10000		10000
#define SPEED_14000		14000
#define SPEED_20000		20000
#define SPEED_25000		25000
#define SPEED_40000		40000
#define SPEED_50000		50000
#define SPEED_56000		56000
#define SPEED_100000		100000

typedef enum {
	PHY_INTERFACE_MODE_NA,
	PHY_INTERFACE_MODE_INTERNAL,
	PHY_INTERFACE_MODE_MII,
	PHY_INTERFACE_MODE_GMII,
	PHY_INTERFACE_MODE_SGMII,
	PHY_INTERFACE_MODE_TBI,
	PHY_INTERFACE_MODE_REVMII,
	PHY_INTERFACE_MODE_RMII,
	PHY_INTERFACE_MODE_RGMII,
	PHY_INTERFACE_MODE_RGMII_ID,
	PHY_INTERFACE_MODE_RGMII_RXID,
	PHY_INTERFACE_MODE_RGMII_TXID,
	PHY_INTERFACE_MODE_RTBI,
	PHY_INTERFACE_MODE_SMII,
	PHY_INTERFACE_MODE_XGMII,
	PHY_INTERFACE_MODE_MOCA,
	PHY_INTERFACE_MODE_QSGMII,
	PHY_INTERFACE_MODE_TRGMII,
	PHY_INTERFACE_MODE_1000BASEX,
	PHY_INTERFACE_MODE_2500BASEX,
	PHY_INTERFACE_MODE_RXAUI,
	PHY_INTERFACE_MODE_XAUI,
	/* 10GBASE-KR, XFI, SFI - single lane 10G Serdes */
	PHY_INTERFACE_MODE_10GKR,
	PHY_INTERFACE_MODE_MAX,
} phy_interface_t;

static inline bool phy_interface_mode_is_rgmii(phy_interface_t mode)
{
	return mode >= PHY_INTERFACE_MODE_RGMII &&
		mode <= PHY_INTERFACE_MODE_RGMII_TXID;
};

#define MII_BUS_ID_SIZE	61
#define PHY_MAX_ADDR	32

#define MII_ADDR_C45 (1<<30)

#define	NETDEV_ALIGN		32

#define PHY_POLL		-1
#define PHY_IGNORE_INTERRUPT	-2

#define PHY_HAS_INTERRUPT	0x00000001

#define PHY_INTERRUPT_ENABLED	0x80000000

struct mii_bus {
	char name[40];
	
	char id[MII_BUS_ID_SIZE];
	void *priv;
	int (*read)(struct mii_bus *bus, int addr, int regnum);
	int (*write)(struct mii_bus *bus, int addr, int regnum, u16 val);
	int (*reset)(struct mii_bus *bus);

	/* list of all PHYs on bus */
	/*struct mdio_device *mdio_map[PHY_MAX_ADDR];*/

	/* PHY addresses to be ignored when probing */
	u32 phy_mask;

	/* PHY addresses to ignore the TA/read failure */
	u32 phy_ignore_ta_mask;
	
	mutex_t mdio_lock;

	/*
	 * An array of interrupts, each PHY's interrupt at the index
	 * matching its address
	 */
	int irq[PHY_MAX_ADDR];

	/* GPIO reset pulse width in microseconds */
	int reset_delay_us;
};

enum phy_state {
	PHY_DOWN = 0,
	PHY_STARTING,
	PHY_READY,
	PHY_PENDING,
	PHY_UP,
	PHY_AN,
	PHY_RUNNING,
	PHY_NOLINK,
	PHY_FORCING,
	PHY_CHANGELINK,
	PHY_HALTED,
	PHY_RESUMING
};

#define SPEED_UNKNOWN		-1

#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01
#define DUPLEX_UNKNOWN		0xff

#define AUTONEG_DISABLE		0x00
#define AUTONEG_ENABLE		0x01

struct phy_device;

struct phy_driver {
	u32 phy_id;
	char *name;
	u32 phy_id_mask;
	u32 features;
	u32 flags;
	const void * driver_data;

	/*
	 * Called to issue a PHY software reset
	 */
	int (*soft_reset)(struct phy_device *phydev);

	/*
	 * Called to initialize the PHY,
	 * including after a reset
	 */
	int (*config_init)(struct phy_device *phydev);

	/*
	 * Called during discovery.  Used to set
	 * up device-specific structures, if any
	 */
	int (*probe)(struct phy_device *phydev);

	/*
	 * Configures the advertisement and resets
	 * autonegotiation if phydev->autoneg is on,
	 * forces the speed to the current settings in phydev
	 * if phydev->autoneg is off
	 */
	int (*config_aneg)(struct phy_device *phydev);

	/* Determines the auto negotiation result */
	int (*aneg_done)(struct phy_device *phydev);

	/* Determines the negotiated speed and duplex */
	int (*read_status)(struct phy_device *phydev);
	
	int (*read_page)(struct phy_device *dev);
	
    int (*write_page)(struct phy_device *dev, int page);
    
    int (*ack_interrupt)(struct phy_device *phydev);
    
    int (*config_intr)(struct phy_device *phydev);

	/* Clears up any memory if needed */
	void (*remove)(struct phy_device *phydev);

	int (*set_loopback)(struct phy_device *dev, bool enable);
};

struct phy_device {
	/* Information about the PHY type */
	/* And management functions */
	struct phy_driver *drv;

	u32 phy_id;
	
	int addr;

	unsigned is_pseudo_fixed_link:1;

	unsigned autoneg:1;
	/* The most recently read link state */
	unsigned link:1;

	enum phy_state state;

	u32 dev_flags;

	phy_interface_t interface;
	
	/*
	 * forced speed & duplex (no autoneg)
	 * partner speed & duplex & pause (autoneg)
	 */
	int speed;
	int duplex;
	int pause;
	int asym_pause;

	/* Union of PHY and Attached devices' supported modes */
	/* See mii.h for more info */
	
	u32 interrupts;  
	
	u32 supported;
	u32 advertising;
	u32 lp_advertising;

	int link_timeout;

	/*
	 * Interrupt number for this PHY
	 * -1 means no interrupt
	 */
	int irq;
	
	struct work_struct state_queue;
	
	mutex_t lock;

	/* private data pointer */
	/* For use by PHYs to maintain extra state */
	void *priv;

	void (*adjust_link)(struct net_driver_s * dev);
};

enum ethtool_link_mode_bit_indices {
	ETHTOOL_LINK_MODE_10baseT_Half_BIT	= 0,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT	= 1,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT	= 2,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT	= 3,
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT	= 4,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT	= 5,
	ETHTOOL_LINK_MODE_Autoneg_BIT		= 6,
	ETHTOOL_LINK_MODE_TP_BIT		= 7,
	ETHTOOL_LINK_MODE_AUI_BIT		= 8,
	ETHTOOL_LINK_MODE_MII_BIT		= 9,
	ETHTOOL_LINK_MODE_FIBRE_BIT		= 10,
	ETHTOOL_LINK_MODE_BNC_BIT		= 11,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT	= 12,
	ETHTOOL_LINK_MODE_Pause_BIT		= 13,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT	= 14,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT	= 15,
	ETHTOOL_LINK_MODE_Backplane_BIT		= 16,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT	= 17,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT	= 18,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT	= 19,
	ETHTOOL_LINK_MODE_10000baseR_FEC_BIT	= 20,
	ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT = 21,
	ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT	= 22,
	ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT	= 23,
	ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT	= 24,
	ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT	= 25,
	ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT	= 26,
	ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT	= 27,
	ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT	= 28,
	ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT	= 29,
	ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT	= 30,
	ETHTOOL_LINK_MODE_25000baseCR_Full_BIT	= 31,
	ETHTOOL_LINK_MODE_25000baseKR_Full_BIT	= 32,
	ETHTOOL_LINK_MODE_25000baseSR_Full_BIT	= 33,
	ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT	= 34,
	ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT	= 35,
	ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT	= 36,
	ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT	= 37,
	ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT	= 38,
	ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT	= 39,
	ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT		= 40,
	ETHTOOL_LINK_MODE_1000baseX_Full_BIT	= 41,
	ETHTOOL_LINK_MODE_10000baseCR_Full_BIT	= 42,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT	= 43,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT	= 44,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT	= 45,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT	= 46,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT	= 47,
	ETHTOOL_LINK_MODE_5000baseT_Full_BIT	= 48,

	ETHTOOL_LINK_MODE_FEC_NONE_BIT	= 49,
	ETHTOOL_LINK_MODE_FEC_RS_BIT	= 50,
	ETHTOOL_LINK_MODE_FEC_BASER_BIT	= 51,

	/* Last allowed bit for __ETHTOOL_LINK_MODE_LEGACY_MASK is bit
	 * 31. Please do NOT define any SUPPORTED_* or ADVERTISED_*
	 * macro for bits > 31. The only way to use indices > 31 is to
	 * use the new ETHTOOL_GLINKSETTINGS/ETHTOOL_SLINKSETTINGS API.
	 */

	__ETHTOOL_LINK_MODE_LAST
	  = ETHTOOL_LINK_MODE_FEC_BASER_BIT,
};

#define __ETHTOOL_LINK_MODE_LEGACY_MASK(base_name)	\
	(1UL << (ETHTOOL_LINK_MODE_ ## base_name ## _BIT))

/* DEPRECATED macros. Please migrate to
 * ETHTOOL_GLINKSETTINGS/ETHTOOL_SLINKSETTINGS API. Please do NOT
 * define any new SUPPORTED_* macro for bits > 31.
 */
#define SUPPORTED_10baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define SUPPORTED_10baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define SUPPORTED_100baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define SUPPORTED_100baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define SUPPORTED_1000baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define SUPPORTED_1000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define SUPPORTED_Autoneg		__ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define SUPPORTED_TP			__ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define SUPPORTED_AUI			__ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define SUPPORTED_MII			__ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define SUPPORTED_FIBRE			__ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define SUPPORTED_BNC			__ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define SUPPORTED_10000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define SUPPORTED_Pause			__ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define SUPPORTED_Asym_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define SUPPORTED_2500baseX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define SUPPORTED_Backplane		__ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define SUPPORTED_1000baseKX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define SUPPORTED_10000baseKX4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define SUPPORTED_10000baseKR_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define SUPPORTED_10000baseR_FEC	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define SUPPORTED_20000baseMLD2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define SUPPORTED_20000baseKR2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define SUPPORTED_40000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define SUPPORTED_40000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define SUPPORTED_40000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define SUPPORTED_40000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define SUPPORTED_56000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define SUPPORTED_56000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define SUPPORTED_56000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define SUPPORTED_56000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)

#define ADVERTISED_10baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define ADVERTISED_10baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define ADVERTISED_100baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define ADVERTISED_100baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define ADVERTISED_1000baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define ADVERTISED_1000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define ADVERTISED_Autoneg		__ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define ADVERTISED_TP			__ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define ADVERTISED_AUI			__ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define ADVERTISED_MII			__ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define ADVERTISED_FIBRE		__ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define ADVERTISED_BNC			__ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define ADVERTISED_10000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define ADVERTISED_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define ADVERTISED_Asym_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define ADVERTISED_2500baseX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define ADVERTISED_Backplane		__ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define ADVERTISED_1000baseKX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define ADVERTISED_10000baseKX4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define ADVERTISED_10000baseKR_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define ADVERTISED_10000baseR_FEC	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define ADVERTISED_20000baseMLD2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define ADVERTISED_20000baseKR2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define ADVERTISED_40000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define ADVERTISED_40000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define ADVERTISED_40000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define ADVERTISED_40000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define ADVERTISED_56000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define ADVERTISED_56000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define ADVERTISED_56000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define ADVERTISED_56000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)

#define PHY_DEFAULT_FEATURES	(SUPPORTED_Autoneg | \
				 SUPPORTED_TP | \
				 SUPPORTED_MII)
				 
#define PHY_100BT_FEATURES	(SUPPORTED_100baseT_Half | \
				 SUPPORTED_100baseT_Full)
				 
#define PHY_10BT_FEATURES	(SUPPORTED_10baseT_Half | \
				 SUPPORTED_10baseT_Full)
				 
#define PHY_1000BT_FEATURES	(SUPPORTED_1000baseT_Half | \
				 SUPPORTED_1000baseT_Full)

#define PHY_BASIC_FEATURES	(PHY_10BT_FEATURES | \
				 PHY_100BT_FEATURES | \
				 PHY_DEFAULT_FEATURES)
				 
#define PHY_GBIT_FEATURES	(PHY_BASIC_FEATURES | \
				 PHY_1000BT_FEATURES)
				 
#define PHY_AN_TIMEOUT		10				 
#define PHY_FORCE_TIMEOUT	10

struct phy_setting {
	u32 speed;
	u8 duplex;
	u8 bit;
};

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define PHY_STATE_TIME		1

#define PHY_ID_FMT "%s:%02x"

struct phy_device *phy_connect(struct net_driver_s * dev, const char *bus_id,
			       void (*handler)(struct net_driver_s *),
			       phy_interface_t interface);
			       
void phy_disconnect(struct phy_device *phydev);			       

void phy_attached_info(struct phy_device *phydev);

void phy_stop(struct phy_device *phydev);

void phy_print_status(struct phy_device *phydev);

void phy_start(struct phy_device *phydev);

#define MDIO_MMD_PMAPMD		1	/* Physical Medium Attachment/
					 * Physical Medium Dependent */
#define MDIO_MMD_WIS		2	/* WAN Interface Sublayer */
#define MDIO_MMD_PCS		3	/* Physical Coding Sublayer */
#define MDIO_MMD_PHYXS		4	/* PHY Extender Sublayer */
#define MDIO_MMD_DTEXS		5	/* DTE Extender Sublayer */
#define MDIO_MMD_TC		6	/* Transmission Convergence */
#define MDIO_MMD_AN		7	/* Auto-Negotiation */
#define MDIO_MMD_C22EXT		29	/* Clause 22 extension */
#define MDIO_MMD_VEND1		30	/* Vendor specific 1 */
#define MDIO_MMD_VEND2		31	/* Vendor specific 2 */

#define MDIO_CTRL1		MII_BMCR
#define MDIO_STAT1		MII_BMSR
#define MDIO_DEVID1		MII_PHYSID1
#define MDIO_DEVID2		MII_PHYSID2
#define MDIO_SPEED		4	/* Speed ability */
#define MDIO_DEVS1		5	/* Devices in package */
#define MDIO_DEVS2		6
#define MDIO_CTRL2		7	/* 10G control 2 */
#define MDIO_STAT2		8	/* 10G status 2 */
#define MDIO_PMA_TXDIS		9	/* 10G PMA/PMD transmit disable */
#define MDIO_PMA_RXDET		10	/* 10G PMA/PMD receive signal detect */
#define MDIO_PMA_EXTABLE	11	/* 10G PMA/PMD extended ability */
#define MDIO_PKGID1		14	/* Package identifier */
#define MDIO_PKGID2		15
#define MDIO_AN_ADVERTISE	16	/* AN advertising (base page) */
#define MDIO_AN_LPA		19	/* AN LP abilities (base page) */
#define MDIO_PCS_EEE_ABLE	20	/* EEE Capability register */
#define MDIO_PCS_EEE_WK_ERR	22	/* EEE wake error counter */
#define MDIO_PHYXS_LNSTAT	24	/* PHY XGXS lane state */
#define MDIO_AN_EEE_ADV		60	/* EEE advertisement */
#define MDIO_AN_EEE_LPABLE	61	/* EEE link partner ability */

#define MDIO_EEE_100TX		MDIO_AN_EEE_ADV_100TX	/* 100TX EEE cap */
#define MDIO_EEE_1000T		MDIO_AN_EEE_ADV_1000T	/* 1000T EEE cap */
#define MDIO_EEE_10GT		0x0008	/* 10GT EEE cap */
#define MDIO_EEE_1000KX		0x0010	/* 1000KX EEE cap */
#define MDIO_EEE_10GKX4		0x0020	/* 10G KX4 EEE cap */
#define MDIO_EEE_10GKR		0x0040	/* 10G KR EEE cap */

#define MDIO_AN_EEE_ADV_100TX	0x0002	/* Advertise 100TX EEE cap */
#define MDIO_AN_EEE_ADV_1000T	0x0004	/* Advertise 1000T EEE cap */

#define MDIO_PCS_CTRL1_CLKSTOP_EN	0x400	/* Stop the clock during LPI */

int phy_init_eee(struct phy_device *phydev, bool clk_stop_enable);


#endif
