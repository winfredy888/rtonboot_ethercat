#ifndef __ECTIMER_H__
#define __ECTIMER_H__

#include <nuttx/semaphore.h>

typedef struct TimerEvent_s
{
    uint64_t Timestamp;                  //! Current timer value
    uint64_t ReloadValue;                //! Timer delay value
    bool IsStarted;                      //! Is the timer currently running
    bool IsNext2Expire;                  //! Is the next timer to expire
    sem_t ec_usleep_sem;
    struct TimerEvent_s *Next;           //! Pointer to the next Timer object.
}timer_event_t;

extern void ecslave_timer_usleep(uint64_t us);

extern uint64_t slavecore_perf_cur_ns(void);

#endif
