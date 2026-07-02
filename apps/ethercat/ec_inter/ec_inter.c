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

#include "../../../nuttx/arch/arm64/src/rk3568/ectimer.h"

#define CONFIG_EC_INTER_SEND_TASK_PRIORITY 130
#define CONFIG_EC_INTER_SEND_TASK_STACKSIZE 32768


typedef uint64_t     u64;
typedef uint16_t     u16;

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

#define     Bool                              int
#define     ETHERCAT_STATUS_OP                0x08
#define     STATUS_SERVO_ENABLE_BIT           (0x04)

typedef enum  _SysWorkingStatus
{
    SYS_WORKING_POWER_ON,
    SYS_WORKING_SAFE_MODE,
    SYS_WORKING_OP_MODE,
    SYS_WORKING_LINK_DOWN,
    SYS_WORKING_IDLE_STATUS
} SysWorkingStatus;

typedef  struct  _GSysRunningParm
{
	SysWorkingStatus   m_gWorkStatus;
} GSysRunningParm;

GSysRunningParm    gSysRunning;

extern int g_ec_inter_stop_run;

int ecstate = 0;

sem_t g_ec_inter_slave_op_sem;

int is_ec_inter_in_op = 0;

/****************************************************************************/

// Application parameters
#define FREQUENCY 1000
#define CLOCK_TO_USE CLOCK_REALTIME

/****************************************************************************/

#define MYNSEC_PER_SEC (1000000000L)
#define PERIOD_NS (MYNSEC_PER_SEC / FREQUENCY)

/****************************************************************************/

static ec_master_t * master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domainServoInput = NULL;
static ec_domain_state_t domainServoInput_state = {};
static ec_domain_t *domainServoOutput = NULL;
static ec_domain_state_t domainServoOutput_state = {};

static uint8_t *domainOutput_pd = NULL;
static uint8_t *domainInput_pd = NULL;

static ec_slave_config_t * sc_asda;
static ec_slave_config_state_t sc_asda_state;

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
    { 0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE },
    { 1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE },
    { 2, EC_DIR_OUTPUT, 1, asda_pdo_1600, EC_WD_DISABLE },
    { 3, EC_DIR_INPUT, 1, asda_pdo_1a00, EC_WD_DISABLE },
    { 0xff }
};

static unsigned int sync_ref_counter = 0;
/*static u64 cycletime_ns = PERIOD_NS;*/

/****************************************************************************/

int ConfigPDO(void)
{
    linux_printf("ec_inter Configuring PDOs...\n");
    domainServoOutput = lib_ecrt_master_create_domain(master);
    if (!domainServoOutput) {
        return -1;
    }
    
    domainServoInput = lib_ecrt_master_create_domain(master);
    if (!domainServoInput) {
        return -1;
    }
    
    linux_printf("ec_inter Creating slave configurations...\n");
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
    if (lib_ecrt_domain_reg_pdo_entry_list(domainServoOutput, domainServoOutput_regs)) {
        linux_printf("PDO entry registration failed!\n");
        return -1;
    }
    if (lib_ecrt_domain_reg_pdo_entry_list(domainServoInput, domainServoInput_regs)) {
        linux_printf("PDO entry registration failed!\n");
        return -1;
    }
	
    linux_printf("Creating SDO requests...\n");
    
    #if 1
    
    lib_ecrt_slave_config_sdo8(sc_asda, 0x6060, 0, 8);
    lib_ecrt_slave_config_sdo8(sc_asda, 0x60C2, 1, 1);
    
    #endif
    
    return 0;
}

/****************************************************************************/

static void rt_check_domain_state(void)
{
    ec_domain_state_t ds = {};
    ec_domain_state_t ds1 = {};
    
    //domainServoInput
    lib_ecrt_domain_state(domainServoInput, &ds);
    if (ds.working_counter != domainServoInput_state.working_counter) {
        linux_printf("domainServoInput: WC %u.\n", ds.working_counter);
    }
    if (ds.wc_state != domainServoInput_state.wc_state) {
        linux_printf("domainServoInput: State %u.\n", ds.wc_state);
    }
    
    domainServoInput_state = ds;
    //domainServoOutput
    lib_ecrt_domain_state(domainServoOutput, &ds1);
    if (ds1.working_counter != domainServoOutput_state.working_counter) {
        linux_printf("domainServoOutput: WC %u.\n", ds1.working_counter);
    }
    if (ds1.wc_state != domainServoOutput_state.wc_state) {
        linux_printf("domainServoOutput: State %u.\n", ds1.wc_state);
    }
    
    domainServoOutput_state = ds1;
}

static void rt_check_master_state(void)
{
    ec_master_state_t ms;

	lib_ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding) {
        linux_printf("%u slave(s).\n", ms.slaves_responding);
    }

    if (ms.al_states != master_state.al_states) {
        linux_printf("AL states: 0x%02X.\n", ms.al_states);
    }

    if (ms.link_up != master_state.link_up) {
        linux_printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }

    master_state = ms;
}

static void ec_inter_check_slave_config_states(void)
{
    ec_slave_config_state_t s;
    
    lib_ecrt_slave_config_state(sc_asda,&s);
    
    if (s.al_state != sc_asda_state.al_state)
    {
        linux_printf("sc_asda_state: State 0x%02X.\n", s.al_state);
    }    
        
    if (s.online != sc_asda_state.online)
        linux_printf("sc_asda_state: %s.\n", s.online ? "online" : "offline");
        
    if (s.operational != sc_asda_state.operational)
        linux_printf("sc_asda_state: %soperational.\n",s.operational ? "" : "Not ");
        
    sc_asda_state = s;
}

void ReleaseMaster(void)
{
    if(master)
    {
        linux_printf("ec_inter End of Program, release master\n");
        
        lib_ecrt_release_master(master);
        
        master = NULL;
    }
}

int ActivateMaster(void)
{
    /*int ret;
    u64 time_ns;*/
    
    if(master)
        return 0;
        
    linux_printf("ec_inter Requesting master...\n");    
        
    master = lib_ecrt_request_master(0);
    if (!master) 
    {
        return -1;
    }

    ConfigPDO();

    // configure SYNC signals for this slave
    lib_ecrt_slave_config_dc(sc_asda, 0x0300, PERIOD_NS, 0, 0, 0);
    
    #if 0
    
    time_ns = ec_perf_cur_ns();
    
    lib_ecrt_master_application_time(master, time_ns);
    
    ret = lib_ecrt_master_select_reference_clock(master, NULL);
    if (ret < 0) 
    {
        linux_printf("ec_inter Failed to select reference clock: %s\n",
                strerror(-ret));
                
        return ret;
    }
    
    #endif

    linux_printf("ec_inter Activating master...\n");
    if (lib_ecrt_master_activate(master)) 
    {
        linux_printf("ec_inter Activating master...failed\n");
        
        return -1;
    }
    
    if (!(domainInput_pd = lib_ecrt_domain_data(domainServoInput))) 
    {
        linux_printf("ec_inter Failed to get domain data pointer.\n");
        
        return -1;
    }
    
    if (!(domainOutput_pd = lib_ecrt_domain_data(domainServoOutput))) 
    {
        linux_printf("ec_inter Failed to get domain data pointer.\n");
        
        return -1;
    }
    
    linux_printf("ec_inter Activating master...success\n");
    
    return 0;
}

int DriverEtherCAT(int argc, char *argv[])
{
    u64 wakeupNS;
    u64 time_ns;
    static int curpos = 0;
    static int cycle_counter = 0;
    int is_skip_first;
    
    if(!is_ec_inter_in_op)
	{
		nxsem_wait(&g_ec_inter_slave_op_sem);
		
		is_ec_inter_in_op = 1;
	}
	
	if(gSysRunning.m_gWorkStatus == SYS_WORKING_POWER_ON)
        return 0;	    
	
	is_skip_first = 0;

    while(!(g_ec_inter_stop_run)) 
    {	
		cycle_counter++;
		if(cycle_counter >= 90 * 1000)
		{
			cycle_counter = 0;
        
			g_ec_inter_stop_run = 1;
			
			break;
		}
		
		if(!is_skip_first)
		{
			/*ec_timer_usleep(1, cycletime_ns / 1000);*/
		}	
                
        /*wakeupNS = ec_perf_cur_ns();*/
        
        wakeupNS = 0;
        
        lib_ecrt_master_application_time(master, wakeupNS);
        			
        lib_ecrt_master_receive(master);
		lib_ecrt_domain_process(domainServoOutput);
		lib_ecrt_domain_process(domainServoInput);
		
		rt_check_domain_state();

		if (!(cycle_counter % 500)) 
		{
			rt_check_master_state();
			
			ec_inter_check_slave_config_states();
		}
		
		switch (gSysRunning.m_gWorkStatus)
		{
			case SYS_WORKING_SAFE_MODE:
        
				rt_check_master_state();
				ec_inter_check_slave_config_states();
			
				if((master_state.al_states & ETHERCAT_STATUS_OP))
				{
					int tmp = true;
					
					if(sc_asda_state.al_state != ETHERCAT_STATUS_OP)
					{
						tmp = false;
						
						break;
					}
				
					if(tmp)
					{
						ecstate = 0;
					
						gSysRunning.m_gWorkStatus = SYS_WORKING_OP_MODE;
					
						linux_printf("OP_MODE\n");
					}
				}
			
				break;

			case SYS_WORKING_OP_MODE:
			
				ecstate++;
        
				if(ecstate <= 20)
				{
					switch (ecstate)
					{
						case 3:
							
							EC_WRITE_U16(domainOutput_pd + cntlwd[0], 0x80);       
							
							break;
							
						case 9:
							
							curpos = EC_READ_S32(domainInput_pd + actpos[0]);       
							
							EC_WRITE_S32(domainOutput_pd + ipData[0], EC_READ_S32(domainInput_pd + actpos[0])); 
							
							linux_printf("cur pos = %d\n", curpos);
							
							break;
							
						case 11:
							
							EC_WRITE_U16(domainOutput_pd + cntlwd[0], 0x06);
							
							break;
							
						case 13:
							
							EC_WRITE_U16(domainOutput_pd + cntlwd[0], 0x07);
                    
							break;
								
						case 15:
							
							EC_WRITE_U16(domainOutput_pd + cntlwd[0], 0x0F);
								
							break;
							
						/*case 17:
							
							EC_WRITE_U16(domainOutput_pd + cntlwd[0], 0x1F);
								
							break;	*/
					};
				}
				else 
				{
					int tmp  = true;
					
					uint16_t sts;
					
					sts = EC_READ_U16(domainInput_pd + status[0]);
					
					if((sts & (STATUS_SERVO_ENABLE_BIT)) == 0)
					{
						tmp = false;
						
						break;
					}
			
					if(tmp)
					{
						ecstate = 0;
					
						gSysRunning.m_gWorkStatus = SYS_WORKING_IDLE_STATUS;
                
						linux_printf("IDLE\n");
					}
				}
			
				break;

			default:
		
				if (!(cycle_counter % 1000)) 
				{
					linux_printf("curpos = %d\t", curpos);
					linux_printf("actpos... %d\t", EC_READ_S32(domainInput_pd + actpos[0]));
				}
			
				curpos += 100;
				
				EC_WRITE_S32(domainOutput_pd + ipData[0], curpos);
			
				break;
		};
        
        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; // sync every cycle
 
            /*time_ns = ec_perf_cur_ns();*/
            
            time_ns = 0;
                        
            lib_ecrt_master_sync_reference_clock_to(master, time_ns);
        }
        
        lib_ecrt_master_sync_slave_clocks(master);

		lib_ecrt_domain_queue(domainServoOutput);
		lib_ecrt_domain_queue(domainServoInput);
		lib_ecrt_master_send(master);
		
		is_skip_first = 0;
		
		/*linux_printf("8\n");*/
    };
    
    task_delete(0);
    
    return 0;
}

int taskid_inter_task = -1;

/****************************************************************************/

int main(int argc, char **argv)
{
	gSysRunning.m_gWorkStatus = SYS_WORKING_POWER_ON;
    if(gSysRunning.m_gWorkStatus == SYS_WORKING_POWER_ON)
    {
        ActivateMaster();
        
        ecstate = 0;
        
        gSysRunning.m_gWorkStatus = SYS_WORKING_SAFE_MODE;
        
        linux_printf("ec_inter SYS_WORKING_SAFE_MODE\n"); 
    }
    
    nxsem_init(&g_ec_inter_slave_op_sem, 0, 0);

    linux_printf("Starting inter task.\n");
    
    taskid_inter_task = task_create("ec_inter_send_task", CONFIG_EC_INTER_SEND_TASK_PRIORITY,
                       CONFIG_EC_INTER_SEND_TASK_STACKSIZE, DriverEtherCAT,
                       NULL);
    if(taskid_inter_task < 0)
    {
		linux_printf("Winfred Young create ec_cyclic_send_task fail\n");
		
		ReleaseMaster();
		
		return -1;
    }
   
    while (!(g_ec_inter_stop_run)) 
	{
		usleep(1LL * 1000LL * 1000LL);
	} 
    
	usleep(1LL * 1000LL * 1000LL);
	
	ReleaseMaster();
    
    return 0;
}

/****************************************************************************/
