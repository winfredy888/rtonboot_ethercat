#ifndef __NUTTX_NET_H
#define __NUTTX_NET_H

#include <nuttx/fs/ioctl.h>

#define SIOCGMIIPHY      _SIOC(0x0024)  /* Get address of MII PHY in use */
#define SIOCGMIIREG      _SIOC(0x0025)  /* Get a MII register via MDIO */
#define SIOCSMIIREG      _SIOC(0x0026)  /* Set a MII register via MDIO */

#define IFNAMSIZ 16

struct ether_addr
{
  uint8_t ether_addr_octet[6];            /* 48-bit Ethernet address */
};

struct eth_hdr_s
{
  uint8_t  dest[6]; /* Ethernet destination address (6 bytes) */
  uint8_t  src[6];  /* Ethernet source address (6 bytes) */
  uint16_t type;    /* Type code (2 bytes) */
};

struct netdev_statistics_s
{
  /* Rx Status */

  uint32_t rx_packets;     /* Number of packets received */
  uint32_t rx_fragments;   /* Number of fragments received */
  uint32_t rx_errors;      /* Number of receive errors */
  uint32_t rx_dropped;     /* Unsupported Rx packets received */

  /* Tx Status */

  uint32_t tx_packets;     /* Number of Tx packets queued */
  uint32_t tx_done;        /* Number of packets completed */
  uint32_t tx_errors;      /* Number of receive errors (incl timeouts) */
  uint32_t tx_timeouts;    /* Number of Tx timeout errors */

  /* Other status */

  uint32_t errors;         /* Total number of errors */
};

struct net_driver_s
{
  /* This link is used to maintain a single-linked list of ethernet drivers.
   * Must be the first field in the structure due to blink type casting.
   */

  FAR struct net_driver_s *flink;

  /* This is the name of network device assigned when netdev_register was
   * called.
   * This name is only used to support socket ioctl lookups by device name
   * Examples: "eth0"
   */

  char d_ifname[IFNAMSIZ];

  /* Drivers interface flags.  See IFF_* definitions in include/net/if.h */

  uint32_t d_flags;

  /* Multi network devices using multiple link layer protocols are
   * supported
   */

  /* Link layer address */

  union
  {
    struct ether_addr ether;    /* Device Ethernet MAC address */

  } d_mac;

  struct netdev_statistics_s d_statistics;

  /* Driver callbacks */

  int (*d_ifup)(FAR struct net_driver_s *dev);
  int (*d_ifdown)(FAR struct net_driver_s *dev);
  int (*d_txavail)(FAR struct net_driver_s *dev);

  int (*d_ioctl)(FAR struct net_driver_s *dev, int cmd,
                 unsigned long arg);

  /* Drivers may attached device-specific, private information */

  FAR void *d_private;
  
  #if CONFIG_ETHERCAT_stmmac
  
  FAR void * d_stmmac_priv;
  
  #endif
};

#endif
