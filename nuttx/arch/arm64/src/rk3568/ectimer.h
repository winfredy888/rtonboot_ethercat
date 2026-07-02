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

extern uint64_t my_arm64_arch_timer_count(void);

extern void ec_timer_usleep(int idx, uint64_t us);

extern uint64_t ec_perf_get_delay(uint64_t old_cnt, uint64_t cur_cnt);

extern uint64_t ec_perf_get_calc_diff(uint64_t old_cnt, uint64_t cur_cnt, uint64_t ns);

extern uint64_t ec_perf_cur_ns(void);

#endif
