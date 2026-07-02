#ifndef __EC_STMMAC_CONFIG_H__
#define __EC_STMMAC_CONFIG_H__

#undef CONFIG_DEBUG_FS

#undef CONFIG_STMMAC_PTP

#define DISABLE_HWTSTAMPS 1

#define DISABLE_TSO 1

#define DISABLE_TX_FRAGS 1

#define MODULE 1

#define NETIF_MSG_DEBUG 1

#define NETIF_MSG_DEBUG_PKTDATA 1

#define MAX_NETPORT_NUM 1

#include "patch.h"
#include "skbuff.h"

#include "../../../arch/arm64/src/common//arm64_arch.h"

#include <nuttx/bits.h>

#define ETH_ALEN 6

struct netdev_priv {
	struct sk_buff * skb_to_lower;
	void * net_priv;
};

typedef s64 time64_t;
 
struct timespec64 {
	time64_t	tv_sec;
	long		tv_nsec;
};

typedef s64	ktime_t;

#define lower_32_bits(n) ((u32)(n))
#define upper_32_bits(n) ((u32)(((n) >> 32) & 0xFFFFFFFF))

enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};

/*#define BIT_MASK(nr)		(1UL << ((nr) % 32))
#define BIT_WORD(nr)		((nr) / 32)*/

static inline void __change_bit(int nr, volatile void *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p ^= mask;
}

static inline void change_bit(int nr, volatile void * addr)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__change_bit(nr, addr);
	spin_unlock_irqrestore(NULL, flags);
}

static inline int __test_and_set_bit(int nr, volatile void *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

static inline int test_and_set_bit(int nr, volatile void * addr)
{
	int out;

	irqstate_t flags = spin_lock_irqsave(NULL);
	out = __test_and_set_bit(nr, addr);
	spin_unlock_irqrestore(NULL, flags);

	return out;
}

static inline void __set_bit(int nr, volatile void *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
}

static inline void set_bit(int nr, volatile void *addr)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__set_bit(nr, addr);
	spin_unlock_irqrestore(NULL, flags);
}

static inline int __test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

static inline int test_and_clear_bit(int nr, volatile void * addr)
{
	int out;

	irqstate_t flags = spin_lock_irqsave(NULL);
	out = __test_and_clear_bit(nr, addr);
	spin_unlock_irqrestore(NULL, flags);

	return out;
}

static inline void __clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
}

static inline void clear_bit(int nr, volatile void *addr)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__clear_bit(nr, addr);
	spin_unlock_irqrestore(NULL, flags);
}

/*
 * This routine doesn't need to be atomic.
 */
static inline int test_bit(int nr, const void * addr)
{
    return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}

#define netdev_dbg(dev, format, ...)				\
    pr_debug("NETDEV: %s " format, (dev)->d_ifname, ##__VA_ARGS__)
    
#define netdev_info(dev, format, ...)				\
    pr_info("NETDEV: %s " format, (dev)->d_ifname, ##__VA_ARGS__)
    
#define netdev_err(dev, format, ...)				\
    pr_err("NETDEV: %s " format, (dev)->d_ifname, ##__VA_ARGS__)    
    
#define netdev_warn(dev, format, ...)				\
    pr_warn("NETDEV: %s " format, (dev)->d_ifname, ##__VA_ARGS__)    
    
    
#define netif_dbg netdev_dbg 

void *kmalloc_array(size_t n, size_t size);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static inline bool is_multicast_ether_addr(const u8 *addr)
{
	u32 a = *(const u32 *)addr;

#if BYTE_ORDER == LITTLE_ENDIAN

	return 0x01 & a;

#else

	return 0x01 & (a >> ((sizeof(a) * 8) - 8));

#endif
}  

static inline bool is_zero_ether_addr(const u8 *addr)
{
	return ((*(const u32 *)addr) | (*(const u16 *)(addr + 4))) == 0;
}

static inline bool is_valid_ether_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

#define SMP_CACHE_BYTES 16

#define SZ_16K				0x00004000

#define INT_MIN		(-INT_MAX - 1)

enum netdev_tx {
	__NETDEV_TX_MIN	 = INT_MIN,	/* make sure enum is signed */
	NETDEV_TX_OK	 = 0x00,	/* driver took care of packet */
	NETDEV_TX_BUSY	 = 0x10,	/* driver tx path was busy*/
};

typedef enum netdev_tx netdev_tx_t;

struct udphdr {
	u16	source;
	u16	dest;
	u16	len;
	u16	check;
};

#define wmb() __asm__ __volatile__("dmb ishst" ::: "memory")

#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")

#define dma_mb()	dmb(osh)
#define dma_rmb()	dmb(oshld)
#define dma_wmb()	dmb(oshst)

enum {
	NETIF_F_SG_BIT,			/* Scatter/gather IO. */
	NETIF_F_IP_CSUM_BIT,		/* Can checksum TCP/UDP over IPv4. */
	__UNUSED_NETIF_F_1,
	NETIF_F_HW_CSUM_BIT,		/* Can checksum all the packets. */
	NETIF_F_IPV6_CSUM_BIT,		/* Can checksum TCP/UDP over IPV6 */
	NETIF_F_HIGHDMA_BIT,		/* Can DMA to high memory. */
	NETIF_F_FRAGLIST_BIT,		/* Scatter/gather IO. */
	NETIF_F_HW_VLAN_CTAG_TX_BIT,	/* Transmit VLAN CTAG HW acceleration */
	NETIF_F_HW_VLAN_CTAG_RX_BIT,	/* Receive VLAN CTAG HW acceleration */
	NETIF_F_HW_VLAN_CTAG_FILTER_BIT,/* Receive filtering on VLAN CTAGs */
	NETIF_F_VLAN_CHALLENGED_BIT,	/* Device cannot handle VLAN packets */
	NETIF_F_GSO_BIT,		/* Enable software GSO. */
	NETIF_F_LLTX_BIT,		/* LockLess TX - deprecated. Please */
					/* do not use LLTX in new drivers */
	NETIF_F_NETNS_LOCAL_BIT,	/* Does not change network namespaces */
	NETIF_F_GRO_BIT,		/* Generic receive offload */
	NETIF_F_LRO_BIT,		/* large receive offload */

	/**/NETIF_F_GSO_SHIFT,		/* keep the order of SKB_GSO_* bits */
	NETIF_F_TSO_BIT			/* ... TCPv4 segmentation */
		= NETIF_F_GSO_SHIFT,
	NETIF_F_GSO_ROBUST_BIT,		/* ... ->SKB_GSO_DODGY */
	NETIF_F_TSO_ECN_BIT,		/* ... TCP ECN support */
	NETIF_F_TSO_MANGLEID_BIT,	/* ... IPV4 ID mangling allowed */
	NETIF_F_TSO6_BIT,		/* ... TCPv6 segmentation */
	NETIF_F_FSO_BIT,		/* ... FCoE segmentation */
	NETIF_F_GSO_GRE_BIT,		/* ... GRE with TSO */
	NETIF_F_GSO_GRE_CSUM_BIT,	/* ... GRE with csum with TSO */
	NETIF_F_GSO_IPXIP4_BIT,		/* ... IP4 or IP6 over IP4 with TSO */
	NETIF_F_GSO_IPXIP6_BIT,		/* ... IP4 or IP6 over IP6 with TSO */
	NETIF_F_GSO_UDP_TUNNEL_BIT,	/* ... UDP TUNNEL with TSO */
	NETIF_F_GSO_UDP_TUNNEL_CSUM_BIT,/* ... UDP TUNNEL with TSO & CSUM */
	NETIF_F_GSO_PARTIAL_BIT,	/* ... Only segment inner-most L4
					 *     in hardware and all other
					 *     headers in software.
					 */
	NETIF_F_GSO_TUNNEL_REMCSUM_BIT, /* ... TUNNEL with TSO & REMCSUM */
	NETIF_F_GSO_SCTP_BIT,		/* ... SCTP fragmentation */
	NETIF_F_GSO_ESP_BIT,		/* ... ESP with TSO */
	NETIF_F_GSO_UDP_BIT,		/* ... UFO, deprecated except tuntap */
	NETIF_F_GSO_UDP_L4_BIT,		/* ... UDP payload GSO (not UFO) */
	/**/NETIF_F_GSO_LAST =		/* last bit, see GSO_MASK */
		NETIF_F_GSO_UDP_L4_BIT,

	NETIF_F_FCOE_CRC_BIT,		/* FCoE CRC32 */
	NETIF_F_SCTP_CRC_BIT,		/* SCTP checksum offload */
	NETIF_F_FCOE_MTU_BIT,		/* Supports max FCoE MTU, 2158 bytes*/
	NETIF_F_NTUPLE_BIT,		/* N-tuple filters supported */
	NETIF_F_RXHASH_BIT,		/* Receive hashing offload */
	NETIF_F_RXCSUM_BIT,		/* Receive checksumming offload */
	NETIF_F_NOCACHE_COPY_BIT,	/* Use no-cache copyfromuser */
	NETIF_F_LOOPBACK_BIT,		/* Enable loopback */
	NETIF_F_RXFCS_BIT,		/* Append FCS to skb pkt data */
	NETIF_F_RXALL_BIT,		/* Receive errored frames too */
	NETIF_F_HW_VLAN_STAG_TX_BIT,	/* Transmit VLAN STAG HW acceleration */
	NETIF_F_HW_VLAN_STAG_RX_BIT,	/* Receive VLAN STAG HW acceleration */
	NETIF_F_HW_VLAN_STAG_FILTER_BIT,/* Receive filtering on VLAN STAGs */
	NETIF_F_HW_L2FW_DOFFLOAD_BIT,	/* Allow L2 Forwarding in Hardware */

	NETIF_F_HW_TC_BIT,		/* Offload TC infrastructure */
	NETIF_F_HW_ESP_BIT,		/* Hardware ESP transformation offload */
	NETIF_F_HW_ESP_TX_CSUM_BIT,	/* ESP with TX checksum offload */
	NETIF_F_RX_UDP_TUNNEL_PORT_BIT, /* Offload of RX port for UDP tunnels */
	NETIF_F_HW_TLS_TX_BIT,		/* Hardware TLS TX offload */
	NETIF_F_HW_TLS_RX_BIT,		/* Hardware TLS RX offload */

	NETIF_F_GRO_HW_BIT,		/* Hardware Generic receive offload */
	NETIF_F_HW_TLS_RECORD_BIT,	/* Offload TLS record */

	/*
	 * Add your fresh new feature above and remember to update
	 * netdev_features_strings[] in net/ethtool/common.c and maybe
	 * some feature mask #defines below. Please also describe it
	 * in Documentation/networking/netdev-features.txt.
	 */

	/**/NETDEV_FEATURE_COUNT
};

/* copy'n'paste compression ;) */
#define __NETIF_F_BIT(bit)	((netdev_features_t)1 << (bit))
#define __NETIF_F(name)		__NETIF_F_BIT(NETIF_F_##name##_BIT)

#define NETIF_F_FCOE_CRC	__NETIF_F(FCOE_CRC)
#define NETIF_F_FCOE_MTU	__NETIF_F(FCOE_MTU)
#define NETIF_F_FRAGLIST	__NETIF_F(FRAGLIST)
#define NETIF_F_FSO		__NETIF_F(FSO)
#define NETIF_F_GRO		__NETIF_F(GRO)
#define NETIF_F_GRO_HW		__NETIF_F(GRO_HW)
#define NETIF_F_GSO		__NETIF_F(GSO)
#define NETIF_F_GSO_ROBUST	__NETIF_F(GSO_ROBUST)
#define NETIF_F_HIGHDMA		__NETIF_F(HIGHDMA)
#define NETIF_F_HW_CSUM		__NETIF_F(HW_CSUM)
#define NETIF_F_HW_VLAN_CTAG_FILTER __NETIF_F(HW_VLAN_CTAG_FILTER)
#define NETIF_F_HW_VLAN_CTAG_RX	__NETIF_F(HW_VLAN_CTAG_RX)
#define NETIF_F_HW_VLAN_CTAG_TX	__NETIF_F(HW_VLAN_CTAG_TX)
#define NETIF_F_IP_CSUM		__NETIF_F(IP_CSUM)
#define NETIF_F_IPV6_CSUM	__NETIF_F(IPV6_CSUM)
#define NETIF_F_LLTX		__NETIF_F(LLTX)
#define NETIF_F_LOOPBACK	__NETIF_F(LOOPBACK)
#define NETIF_F_LRO		__NETIF_F(LRO)
#define NETIF_F_NETNS_LOCAL	__NETIF_F(NETNS_LOCAL)
#define NETIF_F_NOCACHE_COPY	__NETIF_F(NOCACHE_COPY)
#define NETIF_F_NTUPLE		__NETIF_F(NTUPLE)
#define NETIF_F_RXCSUM		__NETIF_F(RXCSUM)
#define NETIF_F_RXHASH		__NETIF_F(RXHASH)
#define NETIF_F_SCTP_CRC	__NETIF_F(SCTP_CRC)
#define NETIF_F_SG		__NETIF_F(SG)
#define NETIF_F_TSO6		__NETIF_F(TSO6)
#define NETIF_F_TSO_ECN		__NETIF_F(TSO_ECN)
#define NETIF_F_TSO		__NETIF_F(TSO)
#define NETIF_F_VLAN_CHALLENGED	__NETIF_F(VLAN_CHALLENGED)
#define NETIF_F_RXFCS		__NETIF_F(RXFCS)
#define NETIF_F_RXALL		__NETIF_F(RXALL)
#define NETIF_F_GSO_GRE		__NETIF_F(GSO_GRE)
#define NETIF_F_GSO_GRE_CSUM	__NETIF_F(GSO_GRE_CSUM)
#define NETIF_F_GSO_IPXIP4	__NETIF_F(GSO_IPXIP4)
#define NETIF_F_GSO_IPXIP6	__NETIF_F(GSO_IPXIP6)
#define NETIF_F_GSO_UDP_TUNNEL	__NETIF_F(GSO_UDP_TUNNEL)
#define NETIF_F_GSO_UDP_TUNNEL_CSUM __NETIF_F(GSO_UDP_TUNNEL_CSUM)
#define NETIF_F_TSO_MANGLEID	__NETIF_F(TSO_MANGLEID)
#define NETIF_F_GSO_PARTIAL	 __NETIF_F(GSO_PARTIAL)
#define NETIF_F_GSO_TUNNEL_REMCSUM __NETIF_F(GSO_TUNNEL_REMCSUM)
#define NETIF_F_GSO_SCTP	__NETIF_F(GSO_SCTP)
#define NETIF_F_GSO_ESP		__NETIF_F(GSO_ESP)
#define NETIF_F_GSO_UDP		__NETIF_F(GSO_UDP)
#define NETIF_F_HW_VLAN_STAG_FILTER __NETIF_F(HW_VLAN_STAG_FILTER)
#define NETIF_F_HW_VLAN_STAG_RX	__NETIF_F(HW_VLAN_STAG_RX)
#define NETIF_F_HW_VLAN_STAG_TX	__NETIF_F(HW_VLAN_STAG_TX)
#define NETIF_F_HW_L2FW_DOFFLOAD	__NETIF_F(HW_L2FW_DOFFLOAD)
#define NETIF_F_HW_TC		__NETIF_F(HW_TC)
#define NETIF_F_HW_ESP		__NETIF_F(HW_ESP)
#define NETIF_F_HW_ESP_TX_CSUM	__NETIF_F(HW_ESP_TX_CSUM)
#define	NETIF_F_RX_UDP_TUNNEL_PORT  __NETIF_F(RX_UDP_TUNNEL_PORT)
#define NETIF_F_HW_TLS_RECORD	__NETIF_F(HW_TLS_RECORD)
#define NETIF_F_GSO_UDP_L4	__NETIF_F(GSO_UDP_L4)
#define NETIF_F_HW_TLS_TX	__NETIF_F(HW_TLS_TX)
#define NETIF_F_HW_TLS_RX	__NETIF_F(HW_TLS_RX)

#define readl_poll_timeout(addr, val, cond, delay_us, timeout_us) \
({ \
	u64 __timeout_us = (timeout_us); \
	unsigned long __delay_us = (delay_us); \
	u64 timeout_jiffies = jiffies + usecs_to_jiffies(__timeout_us); \
	for (;;) { \
		(val) = getreg32((u64)(addr)); \
		if (cond) \
			break; \
		if (__timeout_us && \
		    (timeout_jiffies <= jiffies)) { \
			(val) = getreg32((u64)(addr)); \
			break; \
		} \
		if (__delay_us) \
			usleep_range((__delay_us >> 2) + 1, __delay_us);	\
	} \
	(cond) ? 0 : -ETIMEDOUT; \
})

#define DIV_ROUND_UP(n,d)           (((n) + (d) - 1) / (d))

#define ENET_MAC_FRMF_PM                (1 << 0)

#define IFF_PROMISC                     ENET_MAC_FRMF_PM

#define ENET_MAC_FRMF_MFD               (1 << 4)                           /* Bit 4: multicast filter disable */
#define IFF_ALLMULTI                    ENET_MAC_FRMF_MFD

enum netdev_priv_flags {
	IFF_802_1Q_VLAN			= 1<<0,
	IFF_EBRIDGE			= 1<<1,
	IFF_BONDING			= 1<<2,
	IFF_ISATAP			= 1<<3,
	IFF_WAN_HDLC			= 1<<4,
	IFF_XMIT_DST_RELEASE		= 1<<5,
	IFF_DONT_BRIDGE			= 1<<6,
	IFF_DISABLE_NETPOLL		= 1<<7,
	IFF_MACVLAN_PORT		= 1<<8,
	IFF_BRIDGE_PORT			= 1<<9,
	IFF_OVS_DATAPATH		= 1<<10,
	IFF_TX_SKB_SHARING		= 1<<11,
	IFF_UNICAST_FLT			= 1<<12,
	IFF_TEAM_PORT			= 1<<13,
	IFF_SUPP_NOFCS			= 1<<14,
	IFF_LIVE_ADDR_CHANGE		= 1<<15,
	IFF_MACVLAN			= 1<<16,
	IFF_XMIT_DST_RELEASE_PERM	= 1<<17,
	IFF_L3MDEV_MASTER		= 1<<18,
	IFF_NO_QUEUE			= 1<<19,
	IFF_OPENVSWITCH			= 1<<20,
	IFF_L3MDEV_SLAVE		= 1<<21,
	IFF_TEAM			= 1<<22,
	IFF_RXFH_CONFIGURED		= 1<<23,
	IFF_PHONY_HEADROOM		= 1<<24,
	IFF_MACSEC			= 1<<25,
	IFF_NO_RX_HANDLER		= 1<<26,
	IFF_FAILOVER			= 1<<27,
	IFF_FAILOVER_SLAVE		= 1<<28,
	IFF_L3MDEV_RX_HANDLER		= 1<<29,
	IFF_LIVE_RENAME_OK		= 1<<30,
};

#define netdev_mc_empty(dev)            1
#define netdev_uses_dsa(dev)            0

static inline unsigned long __fls(unsigned long word)
{
	int num = 63;

	if (!(word & (~0ul << 32))) {
		num -= 32;
		word <<= 32;
	}
	
	return num;
}	

static inline int fls64(u64 x)
{
	if (x == 0)
		return 0;
	return __fls(x) + 1;
}

static inline int ilog2(u64 n)
{
	return fls64(n) - 1;
}

#define MYGENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))	
	
#define     usleep_range(a, b)	usleep((b))	
	
    
#endif
