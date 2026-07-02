/*****************************************************************************
 *
 *  Copyright (C) 2007-2022  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#include <nuttx/signal.h>

#include "../../../nuttx/libs/libethercat/ecrt.h"

#define CONFIG_EC_CYCLIC_SEND_TASK_PRIORITY 130
#define CONFIG_EC_CYCLIC_SEND_TASK_STACKSIZE 32768


typedef uint64_t     u64;
typedef uint16_t     u16;

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);
extern int this_cpu(void);

struct rtonboot_ec_cyclic_perf {
	uint64_t period_min_ns;
	uint64_t period_max_ns;
	uint64_t exec_min_ns;
	uint64_t exec_max_ns;
	uint64_t latency_min_ns;
	uint64_t latency_max_ns;
};

sem_t g_slave_op_sem;

int is_in_op = 0;

/****************************************************************************/

// Application parameters
#define FREQUENCY 1000
#define CLOCK_TO_USE CLOCK_REALTIME
#define MEASURE_TIMING 1

#define USE_RTONBOOT_PERF 1

/****************************************************************************/

#define MYNSEC_PER_SEC (1000000000L)
#define PERIOD_NS (MYNSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
        (B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/****************************************************************************/

extern int is_cyclic_quit;

static ec_master_t * master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domainServoInput = NULL;
static ec_domain_state_t domainServoInput_state = {};
static ec_domain_t *domainServoOutput = NULL;
static ec_domain_state_t domainServoOutput_state = {};

static uint8_t *domainOutput_pd = NULL;
static uint8_t *domainInput_pd = NULL;

static ec_slave_config_t * sc_asda;
static ec_slave_config_state_t __attribute__((unused)) sc_asda_state;

/****************************************************************************/

#define asda_Pos0 0, 0

#define asda 0xdd010000, 0x70503010

// offsets for PDO entries
static unsigned int  cntlwd[1];
static unsigned int  ipData[1];
static unsigned int  status[1];
static unsigned int  actpos[1];
static unsigned int  actvel[1];

// process data
static ec_pdo_entry_reg_t domainServoOutput_regs[] = {
    {asda_Pos0, asda, 0x6040, 0x00, &cntlwd[0], NULL},
    {asda_Pos0, asda, 0x607a, 0x00, &ipData[0], NULL},
    {}
};
static ec_pdo_entry_reg_t domainServoInput_regs[] = {
    {asda_Pos0, asda, 0x6064, 0x00, &actpos[0], NULL},
    {asda_Pos0, asda, 0x6041, 0x00, &status[0], NULL},
    {asda_Pos0, asda, 0x606c, 0x00, &actvel[0], NULL},
    {}
};

static ec_pdo_entry_info_t asda_pdo_entries_output[] = {
    { 0x6040, 0x00, 16 }, //control word
    { 0x607a, 0x00, 32 }  //TargetPosition

};

static ec_pdo_entry_info_t asda_pdo_entries_input[] = {
    { 0x6064, 0x00, 32 }, //actualPosition
    { 0x6041, 0x00, 16 }, //status word
    { 0x606c, 0x00, 32 }, //Velocity actual value
};

static ec_pdo_info_t asda_pdo_1600[] = {
    { 0x1600, 2, asda_pdo_entries_output },
};

static ec_pdo_info_t asda_pdo_1a00[] = {
    { 0x1A00, 3, asda_pdo_entries_input },
};

static ec_sync_info_t asda_syncs[] = {
    /*{ 0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE },
    { 1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE },*/
    { 2, EC_DIR_OUTPUT, 1, asda_pdo_1600, EC_WD_DISABLE },
    { 3, EC_DIR_INPUT, 1, asda_pdo_1a00, EC_WD_DISABLE },
    { 0xff }
};

static unsigned int counter = 0;
static unsigned int sync_ref_counter = 0;

static u64 cycletime_ns = PERIOD_NS;

static void rt_check_domain_state(void)
{
    ec_domain_state_t ds = {};
    ec_domain_state_t ds1 = {};
    
    //domainServoInput
    lib_ecrt_domain_state(domainServoInput, &ds);
    if (ds.working_counter != domainServoInput_state.working_counter) {
        linux_printf("domainServoInput: WC %u. %d\n", ds.working_counter, this_cpu());
    }
    if (ds.wc_state != domainServoInput_state.wc_state) {
        linux_printf("domainServoInput: State %u.\n", ds.wc_state);
    }
    
    domainServoInput_state = ds;
    //domainServoOutput
    lib_ecrt_domain_state(domainServoOutput, &ds1);
    /*if (ds1.working_counter != domainServoOutput_state.working_counter) {
        linux_printf("domainServoOutput: WC %u.\n", ds1.working_counter);
    }
    if (ds1.wc_state != domainServoOutput_state.wc_state) {
        linux_printf("domainServoOutput: State %u.\n", ds1.wc_state);
    }*/
    
    domainServoOutput_state = ds1;
}

static void rt_check_master_state(void)
{
    ec_master_state_t ms;

	lib_ecrt_master_state(master, &ms);

    /*if (ms.slaves_responding != master_state.slaves_responding) {
        linux_printf("%u slave(s).\n", ms.slaves_responding);
    }

    if (ms.al_states != master_state.al_states) {
        linux_printf("AL states: 0x%02X.\n", ms.al_states);
    }

    if (ms.link_up != master_state.link_up) {
        linux_printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }*/

    master_state = ms;
}

/****************************************************************************/

void my_nsleep(uint64_t ns)
{
	uint64_t sec;
	struct timespec rqtp;
	
	sec = ns / MYNSEC_PER_SEC;
	
	ns -= (sec * MYNSEC_PER_SEC);
	
	rqtp.tv_sec = (time_t)sec;
	rqtp.tv_nsec = ns;
	
	nxsig_nanosleep(&rqtp, NULL);
}

extern struct rtonboot_ec_cyclic_perf g_ec_cyclic_perf;

u64 op_end;

extern u64 op_start;

extern uint64_t maincore_arm64_arch_timer_count(void);

/*extern void ecslave_timer_usleep(uint64_t us);*/

extern void ec_timer_usleep(int idx, uint64_t us);

extern uint64_t maincore_get_delay(uint64_t old_cnt, uint64_t cur_cnt);

/*extern void ecslave_ec_timer_init(void);*/

extern uint64_t maincore_perf_cur_ns(void);

extern uint64_t ec_perf_get_calc_diff(uint64_t old_cnt, uint64_t cur_cnt, uint64_t ns);

extern void prepare_ec_perf_and_wake_up(void);

/*void cyclic_task(void)*/
int cyclic_task(int argc, char *argv[])
{
    u64 wakeupTime;
    u64 wakeupNS;
    u64 time_ns;
    
    int is_skip_first;
    
    #if 1
    
    if(!is_in_op)
	{
		nxsem_wait(&g_slave_op_sem);
			
		is_in_op = 1;
		
		/*usleep(10);*/
	}
	
	#endif
	
	is_skip_first = 0;
    
#ifdef MEASURE_TIMING

    u64 startTime, endTime = 0ULL, lastStartTime = 0ULL;
    u64 period_ns = 0, exec_ns = 0, latency_ns = 0,
             latency_min_ns = 0, latency_max_ns = 0,
             period_min_ns = 0, period_max_ns = 0,
             exec_min_ns = 0, exec_max_ns = 0;
                     
#endif

/**/
    while(!(is_cyclic_quit)) 
    {	
		if(!is_skip_first)
		{
			wakeupTime = maincore_arm64_arch_timer_count();
        
			ec_timer_usleep(1, cycletime_ns / 1000);
		}	
		
		#ifdef MEASURE_TIMING
        
			startTime = maincore_arm64_arch_timer_count();
        
		#endif
        
        // Write application time to master
        //
        // It is a good idea to use the target time (not the measured time) as
        // application time, because it is more stable.
        //
                
        wakeupNS = maincore_perf_cur_ns();
        
        lib_ecrt_master_application_time(master, wakeupNS);
        
        if(is_skip_first)
        {
			rt_check_master_state();
		}	
		
#ifdef MEASURE_TIMING
        
        if(is_skip_first)
        {        
			lastStartTime = startTime;
			
			goto skip_first;
		}	
		else
		{
			latency_ns = ec_perf_get_calc_diff(wakeupTime, startTime, cycletime_ns);
			period_ns = maincore_get_delay(lastStartTime, startTime);
			exec_ns = maincore_get_delay(lastStartTime, endTime);
		}	
			
        lastStartTime = startTime;
		
        if (latency_ns > latency_max_ns) {
            latency_max_ns = latency_ns;
        }
        if (latency_ns < latency_min_ns) {
            latency_min_ns = latency_ns;
        }
        if (period_ns > period_max_ns) {
            period_max_ns = period_ns;
        }
        if (period_ns < period_min_ns) {
            period_min_ns = period_ns;
        }
        if (exec_ns > exec_max_ns) {
            exec_max_ns = exec_ns;
        }
        if (exec_ns < exec_min_ns) {
            exec_min_ns = exec_ns;
        }
        
        skip_first:
        
#endif

        lib_ecrt_master_receive(master);
		lib_ecrt_domain_process(domainServoOutput);
		lib_ecrt_domain_process(domainServoInput);

        // check process data state (optional)
        rt_check_domain_state();

        if (counter)
        { 
            counter--;
        } else { // do this at 1 Hz
            counter = FREQUENCY;

            // check for master state (optional)
            rt_check_master_state();

#ifdef MEASURE_TIMING
                    
            /*if(!is_skip_first)*/
            if(0)
            {
				/*#ifdef USE_RTONBOOT_PERF*/
				#if 1
				
				g_ec_cyclic_perf.period_min_ns = period_min_ns;
				g_ec_cyclic_perf.period_max_ns = period_max_ns;
				g_ec_cyclic_perf.exec_min_ns = exec_min_ns;
				g_ec_cyclic_perf.exec_max_ns = exec_max_ns;
				g_ec_cyclic_perf.latency_min_ns = latency_min_ns;
				g_ec_cyclic_perf.latency_max_ns = latency_max_ns;
				
				prepare_ec_perf_and_wake_up();
				
				#else
				
				linux_printf("period     %10u ... %10u\n",
                    period_min_ns, period_max_ns);
				linux_printf("exec       %10u ... %10u\n",
                    exec_min_ns, exec_max_ns);
                    
				linux_printf("latency    %10u ... %10u\n",
                    latency_min_ns, latency_max_ns);
				
				#endif
			}
			
			period_max_ns = 0;
            period_min_ns = 0xffffffff;
            exec_max_ns = 0;
            exec_min_ns = 0xffffffff;
            latency_max_ns = 0;
            latency_min_ns = 0xffffffff;
		
#endif
        }
        
        u16 temp = EC_READ_U16(domainInput_pd + status[0]);
        
        ++temp;
        --temp;

        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; // sync every cycle
 
            time_ns = maincore_perf_cur_ns();
                        
            lib_ecrt_master_sync_reference_clock_to(master, time_ns);
        }
        
        lib_ecrt_master_sync_slave_clocks(master);

		lib_ecrt_domain_queue(domainServoOutput);
		lib_ecrt_domain_queue(domainServoInput);
		lib_ecrt_master_send(master);
		
		/*linux_printf("8\n");*/
        
#ifdef MEASURE_TIMING

		endTime = maincore_arm64_arch_timer_count();
        
#endif

		is_skip_first = 0;
    };
    
    task_delete(0);
    
    return 0;
}

int taskid_cyclic_task = -1;

/****************************************************************************/



int main(int argc, char **argv)
{
	#if defined (CONFIG_SMP)
	
	cpu_set_t cpuset;
	int ret;
	
	#endif
	
	/*usleep(10000);
	
	ecslave_ec_timer_init();*/
			
    master = lib_ecrt_request_master(0);
    if (!master) 
    {
		linux_printf("Failed to request_master.\n");
		
        return -1;
    }
    
    domainServoOutput = lib_ecrt_master_create_domain(master);
    if (!domainServoOutput) 
    {
		linux_printf("Failed to create domainServoOutput.\n");
		
        return -1;
    }
    
    domainServoInput = lib_ecrt_master_create_domain(master);
    if (!domainServoInput) 
    {
		linux_printf("Failed to create domainServoInput.\n");
		
        return -1;
    }
    
    sc_asda =
        lib_ecrt_master_slave_config(master, asda_Pos0, asda);
    if (!sc_asda) 
    {
        linux_printf("Failed to get slave configuration.\n");
        
        return -1;
    }
    
    if (lib_ecrt_slave_config_pdos(sc_asda, EC_END, asda_syncs)) 
    {
        linux_printf("Failed to configure PDOs.\n");
        
        return -1;
    }
    
    /********************/
    if (lib_ecrt_domain_reg_pdo_entry_list(domainServoOutput, domainServoOutput_regs)) 
    {
        linux_printf("PDO entry registration failed!\n");
        
        return -1;
    }
    
    if (lib_ecrt_domain_reg_pdo_entry_list(domainServoInput, domainServoInput_regs)) 
    {
        linux_printf("PDO entry registration failed!\n");
        
        return -1;
    }
	
    linux_printf("Creating SDO requests...\n");
    
    lib_ecrt_slave_config_sdo8(sc_asda, 0x6060, 0, 8);
    lib_ecrt_slave_config_sdo8(sc_asda, 0x60C2, 1, 1);
    
    lib_ecrt_slave_config_dc(sc_asda, 0x0300, PERIOD_NS, 0, 0, 0);
    
    if (lib_ecrt_master_activate(master)) 
    {
        linux_printf("ec_cyclic Activating master...failed\n");
        
        return -1;
    }
    
    if (!(domainInput_pd = lib_ecrt_domain_data(domainServoInput))) 
    {
        linux_printf("ec_cyclic Failed to get domain data pointer.\n");
        
        return -1;
    }
    
    if (!(domainOutput_pd = lib_ecrt_domain_data(domainServoOutput))) 
    {
        linux_printf("ec_cyclic  Failed to get domain data pointer.\n");
        
        return -1;
    }
    
    nxsem_init(&g_slave_op_sem, 0, 0);
    
    linux_printf("Starting cyclic task.\n");
    
    taskid_cyclic_task = task_create("ec_cyclic_send_task", CONFIG_EC_CYCLIC_SEND_TASK_PRIORITY,
                       CONFIG_EC_CYCLIC_SEND_TASK_STACKSIZE, cyclic_task,
                       NULL);
    if(taskid_cyclic_task < 0)
    {
		linux_printf("Winfred Young create ec_cyclic_send_task fail\n");
		
		goto release_master;
    }

	#if defined (CONFIG_SMP)
	
    CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	
	ret = nxsched_set_affinity(taskid_cyclic_task, sizeof(cpuset), &cpuset);
	if (ret < 0)
    {
		linux_printf("Winfred Young ERROR: Failed to set SEND Task affinity ret=%d\n", ret);
	}
	
	#endif	    
    
    while(!(is_cyclic_quit)) {
		usleep(1LL * 1000LL * 1000LL);
	}
	
    /*cyclic_task();*/
	
	usleep(1LL * 1000LL * 1000LL);
    
    release_master:
    
		if(master)
		{
			linux_printf("ec_cyclic End of Program, release master\n");
        
			lib_ecrt_release_master(master);
        
			master = NULL;
		}

		return 0;
}

/****************************************************************************/
