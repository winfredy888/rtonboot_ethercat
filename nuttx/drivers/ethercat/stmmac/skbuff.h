#ifndef __EC_SK_BUFF_H__
#define __EC_SK_BUFF_H__

#include "mempool.h"

#define PAGE_SIZE 8192

#define __LITTLE_ENDIAN_BITFIELD 1

enum pkt_hash_types {
	PKT_HASH_TYPE_NONE,	/* Undefined type */
	PKT_HASH_TYPE_L2,	/* Input: src_MAC, dest_MAC */
	PKT_HASH_TYPE_L3,	/* Input: src_IP, dst_IP */
	PKT_HASH_TYPE_L4,	/* Input: src_IP, dst_IP, src_port, dst_port */
};

struct page {
	unsigned char page_content[PAGE_SIZE];
};

struct sk_buff;

struct sk_buff_head {
	struct sk_buff	* next;
	struct sk_buff	* prev;
	u32 qlen;
};

struct skb_frag_struct
{
	struct page *page;
	u16 page_offset;
	u16 size;
};

typedef struct skb_frag_struct skb_frag_t;

#define MAX_SKB_FRAGS (65536/PAGE_SIZE + 1)

enum {
	SKB_GSO_TCPV4 = 1 << 0,

	/* This indicates the skb is from an untrusted source. */
	SKB_GSO_DODGY = 1 << 1,

	/* This indicates the tcp segment has CWR set. */
	SKB_GSO_TCP_ECN = 1 << 2,

	SKB_GSO_TCP_FIXEDID = 1 << 3,

	SKB_GSO_TCPV6 = 1 << 4,

	SKB_GSO_FCOE = 1 << 5,

	SKB_GSO_GRE = 1 << 6,

	SKB_GSO_GRE_CSUM = 1 << 7,

	SKB_GSO_IPXIP4 = 1 << 8,

	SKB_GSO_IPXIP6 = 1 << 9,

	SKB_GSO_UDP_TUNNEL = 1 << 10,

	SKB_GSO_UDP_TUNNEL_CSUM = 1 << 11,

	SKB_GSO_PARTIAL = 1 << 12,

	SKB_GSO_TUNNEL_REMCSUM = 1 << 13,

	SKB_GSO_SCTP = 1 << 14,

	SKB_GSO_ESP = 1 << 15,

	SKB_GSO_UDP = 1 << 16,

	SKB_GSO_UDP_L4 = 1 << 17,
};

enum {
	/* generate hardware time stamp */
	SKBTX_HW_TSTAMP = 1 << 0,

	/* generate software time stamp when queueing packet to NIC */
	SKBTX_SW_TSTAMP = 1 << 1,

	/* device driver is going to provide hardware time stamp */
	SKBTX_IN_PROGRESS = 1 << 2,

	/* device driver supports TX zero-copy buffers */
	SKBTX_DEV_ZEROCOPY = 1 << 3,

	/* generate wifi status information (where possible) */
	SKBTX_WIFI_STATUS = 1 << 4,

	/* This indicates at least one fragment might be overwritten
	 * (as in vmsplice(), sendfile() ...)
	 * If we need to compute a TX checksum, we'll need to copy
	 * all frags to avoid possible bad checksum
	 */
	SKBTX_SHARED_FRAG = 1 << 5,

	/* generate software time stamp when entering packet scheduling */
	SKBTX_SCHED_TSTAMP = 1 << 6,
};

struct skb_shared_info {
	unsigned int	nr_frags;
	
	u8	tx_flags;
	
	unsigned short	gso_size;
	
	struct sk_buff	*frag_list;
	
	unsigned int	gso_type;
	
	skb_frag_t	frags[MAX_SKB_FRAGS];
};

#define CHECKSUM_NONE		0
#define CHECKSUM_UNNECESSARY	1
#define CHECKSUM_COMPLETE	2
#define CHECKSUM_PARTIAL	3


struct sk_buff {
	struct sk_buff	* next;			/* Next buffer in list 				*/
	struct sk_buff	* prev;			/* Previous buffer in list 			*/
	struct sk_buff_head * list;		/* List we are on				*/
	
	struct net_driver_s * dev;
	
	u16	queue_mapping;
	
	#define PKT_TYPE_OFFSET()	offsetof(struct sk_buff, __pkt_type_offset)

	u8			__pkt_type_offset[0];
	u8			ip_summed:2;
	
	unsigned int 	len;
	unsigned int 	data_len;
	/*unsigned int	truesize;*/		/* Buffer size 					*/
	
	u16			protocol;
	
	u16			transport_header;

	unsigned char	*head;			/* Head of buffer 				*/
	unsigned char	*data;			/* Data head pointer				*/
	unsigned char	*tail;			/* Tail pointer					*/
	unsigned char 	*end;			/* End pointer					*/
};

struct tcphdr {
	u16	source;
	u16	dest;
	u32	seq;
	u32	ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u16	res1:4,
		doff:4,
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u16	doff:4,
		res1:4,
		cwr:1,
		ece:1,
		urg:1,
		ack:1,
		psh:1,
		rst:1,
		syn:1,
		fin:1;
#else
#error	"Adjust your <asm/byteorder.h> defines"
#endif	
	u16	window;
	u16	check;
	u16	urg_ptr;
};

#define skb_shinfo(SKB)		((struct skb_shared_info *)((SKB)->end))

#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)

#define NET_IP_ALIGN 0

#define NET_SKB_PAD	   32

#define SKB_DATA_ALIGN(X)	ALIGN(X, 16)

#define SKB_WITH_OVERHEAD(X)	\
	((X) - SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))
	
#define SKB_MAX_ORDER(X, ORDER) \
	SKB_WITH_OVERHEAD((PAGE_SIZE << (ORDER)) - (X))	
	
#define SKB_MAX_HEAD(X)		(SKB_MAX_ORDER((X), 0))


void kfree_skb(struct sk_buff *skb, int idx);

#define dev_kfree_skb(a)	kfree_skb(a, TX_SKB_POOL_IDX)

#define dev_kfree_skb_any(s) kfree_skb(s, RX_SKB_POOL_IDX)

struct sk_buff * dev_alloc_skb(unsigned int length);

void skb_reserve(struct sk_buff *skb, unsigned int len);

unsigned char *skb_push(struct sk_buff *skb, unsigned int len);

u16 skb_get_queue_mapping(const struct sk_buff *skb);

unsigned int skb_headlen(const struct sk_buff *skb);

int skb_transport_offset(const struct sk_buff *skb);

unsigned int tcp_hdrlen(const struct sk_buff *skb);

unsigned int skb_frag_size(const skb_frag_t *frag);

bool skb_is_gso(const struct sk_buff *skb);

unsigned char *skb_put(struct sk_buff *skb, unsigned int len);

void skb_checksum_none_assert(const struct sk_buff *skb);

struct sk_buff * netdev_alloc_skb_ip_align(struct net_driver_s * dev,
		unsigned int length);

void skb_copy_to_linear_data(struct sk_buff *skb,
					   const void *from,
					   const unsigned int len);


#endif
