#ifndef _RTONBOOT_TRACE_H_
#define _RTONBOOT_TRACE_H_

#include "offset_common.h"

#define RTONBOOT_TRACE_LEVEL_ERROR 1
#define RTONBOOT_TRACE_LEVEL_WARN 2
#define RTONBOOT_TRACE_LEVEL_INFO 3
#define RTONBOOT_TRACE_LEVEL_DETAIL 4

void RtonbootTraceDetail(char * fmt, ...);

void RtonbootTraceInfo(char * fmt, ...);

void RtonbootTraceWarn(char * fmt, ...);

void RtonbootTraceError(char * fmt, ...);

static inline void atomic_store_release_64(uint64_t *ptr, uint64_t val)
{
    asm volatile(
        "stlr %1, [%0]"
        :
        : "r" (ptr), "r" (val)
        : "memory"
    );
}

static inline uint64_t atomic_load_acquire_64(const uint64_t *ptr)
{
    uint64_t val;
    
    asm volatile(
        "ldar %0, [%1]"
        : "=r" (val)
        : "r" (ptr)
        : "memory"
    );
    
    return val;
}

static inline void atomic_store_release_32(uint32_t *ptr, uint32_t val)
{
    asm volatile(
        "stlr %w[val], [%[ptr]]"
        : 
        : [ptr] "r" (ptr), [val] "r" (val)
        : "memory"
    );
}

static inline uint32_t atomic_load_acquire_32(const uint32_t *ptr)
{
    uint32_t val;
    asm volatile(
        "ldar %w[val], [%[ptr]]"
        : [val] "=r" (val)
        : [ptr] "r" (ptr)
        : "memory"
    );
    return val;
}


#endif
