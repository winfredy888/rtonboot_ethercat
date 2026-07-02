#include <nuttx/config.h>
#include <nuttx/signal.h>
#include <stdint.h>
#include <assert.h>
#include <debug.h>
#include <stdio.h>

#include "rtonboot_trace.h"

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

extern void flush_dcache_range(unsigned long start, unsigned long stop);

char * ptr_bulk_start = (char *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER + OFFSET_TO_RTONBOOT_BULK_BUFFER );
char * ptr_trace_start;

extern uint64_t RTONBOOT_TRACE_LOCK;
extern uint64_t RTONBOOT_TRACE_LEVEL;

void init_rtonboot_trace(void)
{
	unsigned long start;
    unsigned long stop;
    
	ptr_trace_start = ptr_bulk_start + 8;
	
	*((uint32_t *)ptr_bulk_start) = 0;
	
	*((uint32_t *)(ptr_bulk_start + 4)) = 0;
	
	start = ((unsigned long)CONFIG_RAM_START) + (OFFSET_TO_RTONBOOT_SHARE_BUFFER)  + (OFFSET_TO_RTONBOOT_BULK_BUFFER);
				
	stop = start + 64;
		
	flush_dcache_range(start, stop);
}	

void RtonbootTrace(int cur_level, char * fmt, va_list args)
{
	uint32_t cur_len;
	uint32_t cur_len_bak;
	uint32_t cnt;
	uint32_t len1;
	uint32_t len2;
	unsigned long start;
    unsigned long stop;
    char buf[80];
    char * ptr_append;
    uint64_t trace_lock;
	
	if(!RTONBOOT_TRACE_LEVEL)
		return;
		
	if(RTONBOOT_TRACE_LEVEL < cur_level)
			return;
	
	trace_lock = atomic_load_acquire_64(&RTONBOOT_TRACE_LOCK);		
	while(trace_lock)
	{
		nxsig_usleep(1);
		
		trace_lock = atomic_load_acquire_64(&RTONBOOT_TRACE_LOCK);
	}
	
	atomic_store_release_64(&RTONBOOT_TRACE_LOCK, 1);
	
	cur_len = *((uint32_t *)ptr_bulk_start);
	
	cnt = *((uint32_t *)(ptr_bulk_start + 4));
	
	sprintf(&buf[0], "CNT: %d ", cnt);
	
	len1 = strlen(&buf[0]);
	
	len2 = vsnprintf(NULL, 0, fmt, args);
	
	if( (cur_len + len1 + len2 + 1) >= (RTONBOOT_BULK_BUFFER_SIZE - 8) )
	{
		ptr_append = ptr_trace_start;
		
		cur_len_bak = 0;
		
		cur_len = len1 + len2 + 1;
	}	
	else
	{
		ptr_append = ptr_trace_start + cur_len;
		
		cur_len_bak = cur_len;
		
		cur_len += len1 + len2 + 1;
	}	
	
	strcpy(ptr_append, &buf[0]);
	
	ptr_append += len1;
	
	vsnprintf(ptr_append, len2 + 1, fmt, args);
	
	*((uint32_t *)ptr_bulk_start) = cur_len;
	
	*((uint32_t *)(ptr_bulk_start + 4)) = cnt + 1;
	
	start = ((unsigned long)(CONFIG_RAM_START)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER;
    
	stop = start + 64;
				
	flush_dcache_range(start, stop);  
	
	start = ((unsigned long)(CONFIG_RAM_START)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len_bak;
			
	stop = ((unsigned long)(CONFIG_RAM_START)) + OFFSET_TO_RTONBOOT_SHARE_BUFFER
			+ OFFSET_TO_RTONBOOT_BULK_BUFFER + cur_len;	
			
	flush_dcache_range(start, stop);			
	
	atomic_store_release_64(&RTONBOOT_TRACE_LOCK, 0);
}

void RtonbootTraceDetail(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RtonbootTrace(RTONBOOT_TRACE_LEVEL_DETAIL, fmt, args);
	va_end(args);
}

void RtonbootTraceInfo(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RtonbootTrace(RTONBOOT_TRACE_LEVEL_INFO, fmt, args);
	va_end(args);
}		

void RtonbootTraceWarn(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RtonbootTrace(RTONBOOT_TRACE_LEVEL_WARN, fmt, args);
	va_end(args);
}

void RtonbootTraceError(char * fmt, ...)	
{
	va_list args;
	
	va_start(args, fmt);
	RtonbootTrace(RTONBOOT_TRACE_LEVEL_ERROR, fmt, args);
	va_end(args);
}	



