#include <nuttx/config.h>
#include <assert.h>
#include <nuttx/semaphore.h>
#include <nuttx/kmalloc.h>
#include <nuttx/spinlock.h>

#include "skbuff.h"

extern int linux_printf(const char *fmt, ...);

struct page * get_page(void)
{
	return (struct page *)(kmm_malloc(sizeof(struct page)));
}

void put_page(struct page * pg)
{
	kmm_free(pg);
}

inline int skb_queue_empty(struct sk_buff_head *list)
{
	return (list->next == (struct sk_buff *) list);
}

static inline void skb_headerinit(void *p)
{
	struct sk_buff *skb = p;
	
	memset(skb, 0, sizeof(struct sk_buff));

	skb->prev = skb->next = NULL;
	skb->list = NULL;
	
	skb->dev = NULL;
	
	skb->queue_mapping = 0;
}

static void skb_drop_fraglist(struct sk_buff *skb, int idx)
{
	struct sk_buff *list = skb_shinfo(skb)->frag_list;

	skb_shinfo(skb)->frag_list = NULL;

	do {
		struct sk_buff *this = list;
		list = list->next;
		kfree_skb(this, idx);
	} while (list);
}

static void skb_release_data(struct sk_buff *skb, int idx)
{
	if (skb_shinfo(skb)->nr_frags) 
	{
			int i;
			for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
				put_page(skb_shinfo(skb)->frags[i].page);
	}
	
	if (skb_shinfo(skb)->frag_list)
			skb_drop_fraglist(skb, idx);
	
	if(idx == TX_SKB_POOL_IDX)
	{
		FreeTXBlock(skb->head);
	}
	else if(idx == RX_SKB_POOL_IDX)
	{		
		FreeRXBlock(skb->head);
	}
}

void kfree_skbmem(struct sk_buff *skb, int idx)
{
	skb_release_data(skb, idx);

	#if 0
	
	kmm_free(skb);
	
	#else
	
	FreeSKBufBlock(skb);
	
	#endif
}

void __kfree_skb(struct sk_buff *skb, int idx)
{
	#if 0
	
	if (skb->list)
	 	printk(KERN_WARNING, "Warning: kfree_skb passed an skb still "
		       "on a list (from %p).\n", __builtin_return_address(0));
		       
	skb_headerinit(skb);  /* clean state */
	
	#endif
	
	kfree_skbmem(skb, idx);
}

void kfree_skb(struct sk_buff *skb, int idx)
{
		__kfree_skb(skb, idx);
}

inline struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

inline struct sk_buff *skb_peek_tail(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->prev;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

inline u32 skb_queue_len(struct sk_buff_head *list_)
{
	return(list_->qlen);
}

inline void skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = (struct sk_buff *)list;
	list->next = (struct sk_buff *)list;
	list->qlen = 0;
}

inline void __skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

inline void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__skb_queue_head(list, newsk);
	spin_unlock_irqrestore(NULL, flags);
}

void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

inline void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__skb_queue_tail(list, newsk);
	spin_unlock_irqrestore(NULL, flags);
}

inline struct sk_buff *__skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *result;

	prev = (struct sk_buff *) list;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result = next;
		next = next->next;
		list->qlen--;
		next->prev = prev;
		prev->next = next;
		result->next = NULL;
		result->prev = NULL;
		result->list = NULL;
	}
	return result;
}

inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *result;

	irqstate_t flags = spin_lock_irqsave(NULL);
	result = __skb_dequeue(list);
	spin_unlock_irqrestore(NULL, flags);
	
	return result;
}

inline void __skb_insert(struct sk_buff *newsk,
	struct sk_buff * prev, struct sk_buff *next,
	struct sk_buff_head * list)
{
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
	newsk->list = list;
	list->qlen++;
}

inline void skb_insert(struct sk_buff *old, struct sk_buff *newsk)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__skb_insert(newsk, old->prev, old, old->list);
	spin_unlock_irqrestore(NULL, flags);
}

inline void __skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	__skb_insert(newsk, old, old->next, old->list);
}

inline void skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	__skb_append(old, newsk);
	spin_unlock_irqrestore(NULL, flags);
}

inline void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	struct sk_buff * next, * prev;

	list->qlen--;
	next = skb->next;
	prev = skb->prev;
	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	next->prev = prev;
	prev->next = next;
}

/*
 *	Remove an sk_buff from its list. Works even without knowing the list it
 *	is sitting on, which can be handy at times. It also means that THE LIST
 *	MUST EXIST when you unlink. Thus a list must have its contents unlinked
 *	_FIRST_.
 */

inline void skb_unlink(struct sk_buff *skb)
{
	irqstate_t flags = spin_lock_irqsave(NULL);
	if(skb->list)
		__skb_unlink(skb, skb->list);
	spin_unlock_irqrestore(NULL, flags);
}

unsigned char *__skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	return tmp;
}

unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	
	ASSERT(skb->tail <= skb->end);
	
	return tmp;
}

inline unsigned char *__skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	return skb->data;
}

inline unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	
	ASSERT(skb->data >= skb->head);
		
	return skb->data;
}

inline unsigned char *__skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len-=len;
	return 	skb->data+=len;
}

inline unsigned char * skb_pull(struct sk_buff *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
	return __skb_pull(skb,len);
}

inline int skb_headroom(struct sk_buff *skb)
{
	return skb->data-skb->head;
}

inline int skb_tailroom(struct sk_buff *skb)
{
	return skb->end-skb->tail;
}

inline void skb_reserve(struct sk_buff *skb, unsigned int len)
{
	int align_len = MYALIGN(len, MEM_BLOCK_ALIGN_SIZE);
	skb->data += align_len;
	skb->tail += align_len;
}

struct sk_buff * alloc_skb(unsigned int size, int idx)
{
	struct sk_buff *skb;
	u8 *data;

	/* Get the HEAD */
	
	#if 0
	
	skb = kmm_malloc(sizeof(struct sk_buff));
	if (skb == NULL) 
		goto nohead;
		
	#else
	
	skb = AllocateSKBufBlock();
	if (skb == NULL) 
		goto nohead;
	
	#endif	

	/* Get the DATA. Size must match skb_add_mtu(). */
	if (size > 131072 - 32)
		goto nodata;
	size = ((size + 15) & ~15);
	
	#if 0
	 
	data = kmm_malloc(size + sizeof(struct skb_shared_info));
	if (data == NULL)
		goto nodata;
		
	#else
	
	if(idx == RX_SKB_POOL_IDX)
	{		
		data = AllocateRXBlock();
	}
	else if(idx == TX_SKB_POOL_IDX)
	{
		data = AllocateTXBlock();
	}	
	else
	{
		data = NULL;
	}
	
	if (data == NULL)
		goto nodata;
	
	#endif	

	/* Note that this counter is useless now - you can just look in the
	 * skbuff_head entry in /proc/slabinfo. We keep it only for emergency
	 * cases.
	 */

	/*skb->truesize = size + sizeof(struct sk_buff);*/

	/* Load the data pointers. */
	skb->head = data;
	skb->data = data;
	skb->tail = data;
	skb->end = data + size;

	/* Set up other state */
	skb->len = 0;
	skb->data_len = 0;
	
	skb_shinfo(skb)->nr_frags = 0;
	skb_shinfo(skb)->frag_list = NULL;

	return skb;

nodata:

    #if 0
    
	kmm_free(skb);
	
	#else
	
	FreeSKBufBlock(skb);
	
	#endif
	
nohead:
	
	return NULL;
}

inline struct sk_buff * dev_alloc_skb(unsigned int length)
{
	struct sk_buff *skb;

	skb = alloc_skb(length + 16, TX_SKB_POOL_IDX);
	if (skb)
		skb_reserve(skb,16);
	return skb;
}

inline u16 skb_get_queue_mapping(const struct sk_buff *skb)
{
	/*return skb->queue_mapping;*/
	
	return 0;
}	

static inline unsigned char *skb_transport_header(const struct sk_buff *skb)
{
	return skb->head + skb->transport_header;
}

inline int skb_transport_offset(const struct sk_buff *skb)
{
	return skb_transport_header(skb) - skb->data;
}

static inline struct tcphdr *tcp_hdr(const struct sk_buff *skb)
{
	return (struct tcphdr *)skb_transport_header(skb);
}

static inline unsigned int __tcp_hdrlen(const struct tcphdr *th)
{
	return th->doff * 4;
}

unsigned int tcp_hdrlen(const struct sk_buff *skb)
{
	return __tcp_hdrlen(tcp_hdr(skb));
}

unsigned int skb_headlen(const struct sk_buff *skb)
{
	return skb->len - skb->data_len;
}

unsigned int skb_frag_size(const skb_frag_t *frag)
{
	return frag->size;
}

bool skb_is_gso(const struct sk_buff *skb)
{
	return skb_shinfo(skb)->gso_size;
}

void skb_checksum_none_assert(const struct sk_buff *skb)
{
	ASSERT(skb->ip_summed == CHECKSUM_NONE);
}

struct sk_buff *__netdev_alloc_skb(struct net_driver_s *dev, unsigned int len)
{
	struct sk_buff *skb;

	len += NET_SKB_PAD;
	
	skb = alloc_skb(len, RX_SKB_POOL_IDX);
	if (!skb)
		goto skb_fail;
	goto skb_success;

skb_success:
	skb_reserve(skb, NET_SKB_PAD);
	skb->dev = dev;

skb_fail:
	return skb;
}

struct sk_buff * netdev_alloc_skb_ip_align(struct net_driver_s * dev,
		unsigned int length)
{
	struct sk_buff *skb = __netdev_alloc_skb(dev, length + NET_IP_ALIGN);

	if (NET_IP_ALIGN && skb)
		skb_reserve(skb, NET_IP_ALIGN);
		
	return skb;
}

void skb_copy_to_linear_data(struct sk_buff *skb,
					   const void *from,
					   const unsigned int len)
{
	memcpy(skb->data, from, len);
}
