#include "mempool.h"

struct MemoryPoolDesc MemoryPoolDescArray[MAX_POOL_COUNT] =
{
	{
		NULL,
		TX_DESC_BLOCK_SIZE,
		TX_DESC_BLOCK_COUNT,
		0
	},	
	
	{
		NULL,
		RX_DESC_BLOCK_SIZE,
		RX_DESC_BLOCK_COUNT,
		0
	},
	
	{
		NULL,
		TX_SKB_BLOCK_SIZE,
		TX_SKB_BLOCK_COUNT,
		1
	},
	
	{
		NULL,
		RX_SKB_BLOCK_SIZE,
		RX_SKB_BLOCK_COUNT,
		1
	},
	
	{
		NULL,
		SK_BUFF_BLOCK_SIZE,
		SK_BUFF_BLOCK_COUNT,
		1
	},
	
	{
		NULL,
		EC_USLEEP_BLOCK_SIZE,
		EC_USLEEP_BLOCK_COUNT,
		1
	},
};

MemoryPool * InitMemoryPool(int blockSize, int blockCount, int is_alloc_block_list)
{
    MemoryPool * pool = NULL;
    char * data_ptr;
    char * data_ptr_begin;
    char * real_data_ptr_begin;
    u64 temp;
    struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	MemoryBlock * p_block_item;
 
    pool = (MemoryPool *)kmm_malloc(sizeof(MemoryPool));
    if(!pool)
    {
		pr_err("fail to malloc MemoryPool\n");
		
		return NULL;
	}
	
	pool->is_alloc_block_list = is_alloc_block_list;
	
	INIT_LIST_HEAD( &(pool->free_list_head) );
	
	INIT_LIST_HEAD( &(pool->used_list_head) );
    
    data_ptr = kmm_malloc((blockSize * blockCount) + MEM_BLOCK_ALIGN_SIZE);
    if(!data_ptr)
    {
		pr_err("fail to malloc mempool dataptr\n");
		
		goto fail_malloc_1;
	}
	
	temp = (u64)(data_ptr);
	temp = MYALIGN(temp, MEM_BLOCK_ALIGN_SIZE);
	data_ptr_begin = (char *)(temp);
	real_data_ptr_begin = data_ptr_begin;
    
    if(is_alloc_block_list)
    {
		for(int i = 0; i < blockCount; i++)
		{
			MemoryBlock * block = (MemoryBlock *)kmm_malloc(sizeof(MemoryBlock));
			if(!block)
			{
				pr_err("fail to malloc MemoryBlock\n");
		
				goto fail_malloc;
			}	
		
			block->data = data_ptr_begin;
			
			list_add_tail(&(block->memory_block_list), &(pool->free_list_head));
        
			data_ptr_begin += blockSize;
		}
	}	
    
    pool->data_begin = data_ptr;
    pool->real_data_begin = real_data_ptr_begin;
    
    if(is_alloc_block_list)
    {
		pool->freeCount = blockCount;
		pool->blockCount = blockCount;
	}
	else
	{
		pool->freeCount = 0;
		pool->blockCount = 0;
	}		
     
    return pool;
    
fail_malloc:
 
    if(is_alloc_block_list)
    {
		p_head = &(pool->free_list_head);
		
		if( list_empty(p_head) )
			goto fail_malloc_1;
			
		for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
		{
			pTempBak = pTemp->next;
		
			p_block_item = (MemoryBlock *)pTemp;
			
			kmm_free(p_block_item);
		}
	}	
    
fail_malloc_1: 
    
    kmm_free(data_ptr);
    
    kmm_free(pool);
    
    return NULL;	
}

void * AllocateBlock(MemoryPool *pool)
{
	struct list_head * p_head;
	struct list_head * pTemp;
	MemoryBlock * node;
	
	p_head = &(pool->free_list_head);
		
	if( list_empty(p_head) )
			return NULL;
			
	if(pool->freeCount == 0)
			return NULL;
			
	pTemp = p_head->next;		
        
    node = (MemoryBlock *)pTemp;
    
    list_del(&(node->memory_block_list));
    
    list_add_tail(&(node->memory_block_list), &(pool->used_list_head) );
        
    pool->usedCount++;
    pool->freeCount--;
 
    return node->data;
}

void FreeBlock(MemoryPool *pool, void *data)
{
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	MemoryBlock * node;
	
	p_head = &(pool->used_list_head);
		
	if( list_empty(p_head) )
		return;
			
	for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
	{
		pTempBak = pTemp->next;
		
		node = (MemoryBlock *)pTemp;
			
		if( (node->data) == data)
			goto found;
	}
	
	return;
	
	found:
	
	list_del(&(node->memory_block_list));
    
    list_add_tail(&(node->memory_block_list), &(pool->free_list_head) );
    
    pool->freeCount++;
    pool->usedCount--;
	
    return;
}

void DestroyMemoryPool(MemoryPool *pool)
{
	struct list_head * p_head;
	struct list_head * pTemp;
	struct list_head * pTempBak;
	MemoryBlock * node;
    int is_alloc_block_list = pool->is_alloc_block_list;
    
    if(is_alloc_block_list)
    {
		p_head = &(pool->free_list_head);
		
		if( list_empty(p_head) )
			goto free_used;
			
		for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
		{
			pTempBak = pTemp->next;
		
			node = (MemoryBlock *)pTemp;
			
			kmm_free(node);
		}	
			
		free_used:	
		
			p_head = &(pool->used_list_head);
		
			if( list_empty(p_head) )
				goto free_other;
				
			for(pTemp = p_head->next; pTemp != p_head; pTemp = pTempBak)
			{
				pTempBak = pTemp->next;
		
				node = (MemoryBlock *)pTemp;
			
				kmm_free(node);
			}
	}
	
	free_other:	
    
		kmm_free(pool->data_begin);
   
		kmm_free(pool);
 
		return;
}

int InitDMAPool(void)
{
	int i;
	
	for(i = 0; i < MAX_POOL_COUNT; ++i)
	{
		MemoryPoolDescArray[i].InternalPool = InitMemoryPool(MemoryPoolDescArray[i].block_size, 
			MemoryPoolDescArray[i].block_count, MemoryPoolDescArray[i].is_alloc_block_list);
		if(!(MemoryPoolDescArray[i].InternalPool))
		{
			goto fail_destory;
		}	
	}
	
	return 0;
	
	fail_destory:
	
		for(int j = 0; j < i; j++)
		{
			DestroyMemoryPool(MemoryPoolDescArray[j].InternalPool);
		}	
		
		return -1;
}

void DestoryDMAPool(void)
{
	for(int j = 0; j < MAX_POOL_COUNT; j++)
	{
			DestroyMemoryPool(MemoryPoolDescArray[j].InternalPool);
	}
}

void * AllocateTXBlock(void)
{
	MemoryPool * pool = MemoryPoolDescArray[TX_SKB_POOL_IDX].InternalPool;
	
	return AllocateBlock(pool);
}

void * AllocateRXBlock(void)
{
	MemoryPool * pool = MemoryPoolDescArray[RX_SKB_POOL_IDX].InternalPool;
	
	return AllocateBlock(pool);
}

void * AllocateSKBufBlock(void)
{
	MemoryPool * pool = MemoryPoolDescArray[SK_BUFF_POOL_IDX].InternalPool;
	
	return AllocateBlock(pool);
}

void * AllocateECUsleepBlock(void)
{
	MemoryPool * pool = MemoryPoolDescArray[EC_USLEEP_POOL_IDX].InternalPool;
	
	return AllocateBlock(pool);
}

void FreeTXBlock(void *data)
{
	MemoryPool * pool = MemoryPoolDescArray[TX_SKB_POOL_IDX].InternalPool;
	
	FreeBlock(pool, data);
}

void FreeRXBlock(void *data)
{	
	MemoryPool * pool = MemoryPoolDescArray[RX_SKB_POOL_IDX].InternalPool;
	
	FreeBlock(pool, data);
}

void FreeSKBufBlock(void *data)
{	
	MemoryPool * pool = MemoryPoolDescArray[SK_BUFF_POOL_IDX].InternalPool;
	
	FreeBlock(pool, data);
}

void FreeECUsleepBlock(void *data)
{	
	MemoryPool * pool = MemoryPoolDescArray[EC_USLEEP_POOL_IDX].InternalPool;
	
	FreeBlock(pool, data);
}

void * GetTXDescPoolDataBegin(void)
{
	MemoryPool * pool = MemoryPoolDescArray[TX_DESC_POOL_IDX].InternalPool;
	
	return (pool->real_data_begin);
}

void * GetRXDescPoolDataBegin(void)
{
	MemoryPool * pool = MemoryPoolDescArray[RX_DESC_POOL_IDX].InternalPool;
	
	return (pool->real_data_begin);
}
