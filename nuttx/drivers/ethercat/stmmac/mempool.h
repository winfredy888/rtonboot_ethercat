#ifndef __MEM_POOL_H__
#define __MEM_POOL_H__

#include <nuttx/config.h>
#include <nuttx/semaphore.h>
#include <nuttx/kmalloc.h>
#include <debug.h>
#include <stdio.h>

#include "patch.h"

#include "list.h"

#define MEM_BLOCK_ALIGN_SIZE 32

#define TX_DESC_POOL_IDX 0
#define RX_DESC_POOL_IDX 1
#define TX_SKB_POOL_IDX 2
#define RX_SKB_POOL_IDX 3
#define SK_BUFF_POOL_IDX 4
#define EC_USLEEP_POOL_IDX 5

#define MAX_POOL_COUNT 6

#define TX_DESC_BLOCK_SIZE 32
#define TX_DESC_BLOCK_COUNT 512

#define RX_DESC_BLOCK_SIZE 32
#define RX_DESC_BLOCK_COUNT 512

#define TX_SKB_BLOCK_SIZE 2048
#define TX_SKB_BLOCK_COUNT 8 

#define RX_SKB_BLOCK_SIZE 2048
#define RX_SKB_BLOCK_COUNT 8

#define SK_BUFF_BLOCK_SIZE 128
#define SK_BUFF_BLOCK_COUNT (RX_SKB_BLOCK_COUNT + TX_SKB_BLOCK_COUNT)

#define SK_BUFF_BLOCK_SIZE 128
#define SK_BUFF_BLOCK_COUNT (RX_SKB_BLOCK_COUNT + TX_SKB_BLOCK_COUNT)

#define EC_USLEEP_BLOCK_SIZE 64
#define EC_USLEEP_BLOCK_COUNT 4

#define __MYALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define MYALIGN(x,a) __MYALIGN_MASK((x),(typeof(x))(a)-1)



typedef struct MemoryBlock{
	struct list_head memory_block_list;
    void *data;
} MemoryBlock;

typedef struct MemoryPool{
    struct list_head free_list_head;
    struct list_head used_list_head;
    int freeCount;
    int usedCount;
    int blockCount;
    void * data_begin;
    void * real_data_begin;
    int is_alloc_block_list;
} MemoryPool;

struct MemoryPoolDesc {
	MemoryPool * InternalPool;
	int block_size;
	int block_count;
	int is_alloc_block_list;
};

int InitDMAPool(void);

void DestoryDMAPool(void);

void * AllocateTXBlock(void);

void * AllocateRXBlock(void);

void * AllocateSKBufBlock(void);

void * AllocateECUsleepBlock(void);

void FreeTXBlock(void *data);

void FreeRXBlock(void *data);

void FreeSKBufBlock(void *data);

void FreeECUsleepBlock(void *data);

void * GetTXDescPoolDataBegin(void);

void * GetRXDescPoolDataBegin(void);

#endif
