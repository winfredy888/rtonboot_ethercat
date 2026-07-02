#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "../../../nuttx/libs/libsoem/ethercat.h"

/*#define MEASURE_TIMING 1*/
#undef MEASURE_TIMING

#define TOTAL_LAT 100000

extern uint64_t maincore_arm64_arch_timer_count(void);

extern uint64_t maincore_get_delay(uint64_t old_cnt, uint64_t cur_cnt);

extern void append_soem_lat(uint32_t period_ns, uint32_t exec_ns, uint32_t latency_ns);

extern void print_soem_lat(void);

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

extern int g_motorctl_stop_run;

int g_motorctl_target_step;

int is_real_quit = 0;

char IOmap_motorctl[4096];

int expectedWKC_motorctl; // Expected Work Counter
boolean needlf_motorctl; // Flag to indicate if a line feed is needed
volatile int wkc_motorctl; // Work Counter (volatile to ensure it is updated correctly in multi-threaded context)
boolean inOP_motorctl;

uint8 currentgroup_motorctl = 0;

#define EC_TIMEOUTMON 5000

#define SYNC_CYCLE 125

uint32_t cycletime_ns_motorctl;

typedef enum {  NOT_READY = 0, 
                SWITCH_DISABLED,
                READY_SWITCH, 
                SWITCHED_ON,
                OPERATION_ENABLED,
                QUICK_STOP,
                FAULT_REACTION,
                FAULT,
                UNKNOWN} PDS_STATUS; 

typedef struct {
    uint16_t controlword;      // 0x6040:0, 16 bits
    int32_t target_position;   // 0x607A:0, 32 bits
    uint8_t mode_of_operation; // 0x6060:0, 8 bits
    uint8_t padding;           // 8 bits padding for alignment
} __attribute__((__packed__)) rxpdo_t;

// Structure for TXPDO (Status data received from slave)
typedef struct {
    uint16_t statusword;      // 0x6041:0, 16 bits
    int32_t actual_position;  // 0x6064:0, 32 bits
    int32_t actual_velocity;  // 0x606C:0, 32 bits
    int16_t actual_torque;    // 0x6077:0, 16 bits
} __attribute__((__packed__)) txpdo_t;

int64 toff, gl_delta;

int dorun = 0;

bool start_ecatthread_thread;

int ctime_thread;

int8_t SLAVE_ID; 

OSAL_THREAD_HANDLE thread1_id;
OSAL_THREAD_HANDLE thread2;

PDS_STATUS getPDSStatus(txpdo_t * p_txpdo)
{
	uint16 statusword = p_txpdo->statusword;
	// printf("statusword: %4x \n",statusword );
	if (((statusword) & 0x004f) == 0x0000) { // x0xx 0000
		return NOT_READY;
	}else if (((statusword) & 0x004f) == 0x0040) { // x1xx 0000
		return SWITCH_DISABLED;
	}else if (((statusword) & 0x006f) == 0x0021) { // x01x 0001
		return READY_SWITCH;
	}else if (((statusword) & 0x006f) == 0x0023) { // x01x 0011
		return SWITCHED_ON;
	}else if (((statusword) & 0x006f) == 0x0027) { // x01x 0111
		return OPERATION_ENABLED;
	}else if (((statusword) & 0x006f) == 0x0007) { // x00x 0111
		return QUICK_STOP;
	}else if (((statusword) & 0x004f) == 0x000f) { // x0xx 1111
		return FAULT_REACTION;
	}else if (((statusword) & 0x004f) == 0x0008) { // x0xx 1000
		return FAULT;
	}else{
		return UNKNOWN;
  }
}

int servoOff(txpdo_t * p_txpdo, rxpdo_t * p_rxpdo)
{
	PDS_STATUS pd_status;
	
	pd_status = getPDSStatus(p_txpdo);
	if(pd_status == SWITCH_DISABLED)
			return 1;
	
	#if 0
	
	switch ( pd_status ) 
	{
			case READY_SWITCH:
				p_rxpdo->controlword = 0x0000; // disable voltage
				break;
			case SWITCHED_ON:
				p_rxpdo->controlword = 0x0006; // shutdown
				break;
			case OPERATION_ENABLED:
				p_rxpdo->controlword = 0x0007; // disable operation
				break;
			default:
				// printf("unknown status");
				p_rxpdo->controlword = 0x0000; // disable operation
				break;
	};
	
	#else
	
	p_rxpdo->controlword = 0x0080;
	
	#endif
	
	return 0;
}


OSAL_THREAD_FUNC ecatcheck_motorctl(void *ptr) {
    int slave; // Variable to hold the current slave index
    (void)ptr; // Not used
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;

    while((!is_real_quit)) {
        if (inOP_motorctl && ((wkc_motorctl < expectedWKC_motorctl) || ec_group[currentgroup_motorctl].docheckstate)) {
            if (needlf_motorctl) {
                needlf_motorctl = FALSE;
                linux_printf("\n");
            }
            
            // Increase the consecutive error count
            if (wkc_motorctl < expectedWKC_motorctl) {
                consecutive_errors++;
                linux_printf("WARNING: Working counter error (%d/%d), consecutive errors: %d\n", 
                       wkc_motorctl, expectedWKC_motorctl, consecutive_errors);
            } else {
                consecutive_errors = 0;
            }

            // If the consecutive errors exceed the threshold, attempt reinitialization
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                linux_printf("ERROR: Too many consecutive errors, attempting recovery...\n");
                ec_group[currentgroup_motorctl].docheckstate = TRUE;
                // Reset the error count
                consecutive_errors = 0;
            }

            ec_group[currentgroup_motorctl].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++) {
                if ((ec_slave[slave].group == currentgroup_motorctl) && (ec_slave[slave].state != EC_STATE_OPERATIONAL)) {
                    ec_group[currentgroup_motorctl].docheckstate = TRUE;
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        linux_printf("ERROR: Slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state == EC_STATE_SAFE_OP) {
                        linux_printf("WARNING: Slave %d is in SAFE_OP, changing to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state > EC_STATE_NONE) {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            linux_printf("MESSAGE: Slave %d reconfigured\n", slave);
                        }
                    } else if (!ec_slave[slave].islost) {
                        ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (!ec_slave[slave].state) {
                            ec_slave[slave].islost = TRUE;
                            linux_printf("ERROR: Slave %d lost\n", slave);
                        }
                    }
                }
                if (ec_slave[slave].islost) {
                    if (!ec_slave[slave].state) {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            linux_printf("MESSAGE: Slave %d recovered\n", slave);
                        }
                    } else {
                        ec_slave[slave].islost = FALSE;
                        linux_printf("MESSAGE: Slave %d found\n", slave);
                    }
                }
            }
            if (!ec_group[currentgroup_motorctl].docheckstate) {
                linux_printf("OK: All slaves resumed OPERATIONAL.\n");
            }
        }
        osal_usleep(10000); 
    }
}

/* 
 * PI calculation to synchronize Linux time with the Distributed Clock (DC) time.
 * This function calculates the offset time needed to align the Linux time with the DC time.
 */
void ec_sync(int64 reftime, int64 cycletime, int64 *offsettime) {
    static int64 integral = 0; // Integral term for PI controller
    int64 delta; // Variable to hold the difference between reference time and cycle time
    delta = (reftime) % cycletime; // Calculate the delta time
    if (delta > (cycletime / 2)) {
        delta = delta - cycletime; // Adjust delta if it's greater than half the cycle time
    }
    if (delta > 0) {
        integral++; // Increment integral if delta is positive
    }
    if (delta < 0) {
        integral--; // Decrement integral if delta is negative
    }
    *offsettime = -(delta / 100) - (integral / 20); // Calculate the offset time
    gl_delta = delta; // Update global delta variable
}

#ifdef MEASURE_TIMING

uint32_t calc_lat_ns(uint64_t wakeupCNT, uint64_t StartCNT, int64 cycletime)
{
	uint64_t delay;
	
	delay = maincore_get_delay(wakeupCNT, StartCNT);
	
	delay -= cycletime;
	
	return (uint32_t)(delay);
}	

#endif

OSAL_THREAD_FUNC_RT ecatthread(void *ptr) {
    int64 cycletime;

    cycletime = *(int *)ptr * 1000;

    toff = 0;
    dorun = 0;
    
    // Initialize PDO data
    rxpdo_t rxpdo;
    txpdo_t txpdo;
    
    rxpdo.controlword = 0x0080;
    rxpdo.target_position = 0;
    rxpdo.mode_of_operation = 8;
    rxpdo.padding = 0;
    
    // Send initial process data
    for (int slave = 1; slave <= ec_slavecount; slave++) {
        memcpy(ec_slave[slave].outputs, &rxpdo, sizeof(rxpdo_t));
    }
    
    ec_send_processdata();

    int step = 0;
    
    int quit_count = 0;
    
    int can_quit = 0;
    
    #ifdef MEASURE_TIMING
    
    uint64_t wakeupCNT;
    uint64_t StartCNT;
    uint64_t EndCNT;
    uint64_t LastStartCNT;
    
    uint32_t period_ns = 0, exec_ns = 0, latency_ns = 0;
    uint32_t total_lat;
    
    wakeupCNT = 0;
    StartCNT = 0;
    EndCNT = 0;
    LastStartCNT = 0;
    total_lat = 0;
    
    #endif
    
    txpdo.actual_position = 0;
    txpdo.statusword = 0;

    while (1) {
		
		#ifdef MEASURE_TIMING
		
			if ( (start_ecatthread_thread) && (!g_motorctl_stop_run) )
			{
				wakeupCNT = maincore_arm64_arch_timer_count();
			}
			
		#endif
		
        osal_usleep( (cycletime + toff) / 1000);
        
        #ifdef MEASURE_TIMING
		
			if ( (start_ecatthread_thread) && (!g_motorctl_stop_run) )
			{
				StartCNT = maincore_arm64_arch_timer_count();
				
				if( (LastStartCNT) && (EndCNT) )
				{
					latency_ns = calc_lat_ns(wakeupCNT, StartCNT, cycletime + toff);
					
					period_ns = maincore_get_delay(LastStartCNT, StartCNT);
					
					exec_ns = maincore_get_delay(LastStartCNT, EndCNT);
					
					append_soem_lat(period_ns, exec_ns, latency_ns);
					
					++total_lat;
					
					if(total_lat >= TOTAL_LAT)
					{
						g_motorctl_stop_run = 1;
					}	
				}
				
				LastStartCNT = StartCNT;
			}
			
		#endif
                
        dorun++;

        if (start_ecatthread_thread) {
            // Receive process data
            wkc_motorctl = ec_receive_processdata(EC_TIMEOUTRET);

            if (wkc_motorctl >= expectedWKC_motorctl) {
                // Retrieve the current motor status
                for (int slave = 1; slave <= ec_slavecount; slave++) {
                    memcpy(&txpdo, ec_slave[slave].inputs, sizeof(txpdo_t));
                }

                // State machine control
                if (step <= 400) {
                    rxpdo.controlword = 0x0080;
                    rxpdo.target_position = 0;
                } else if (step <= 600) {
                    rxpdo.controlword = 0x0006;
                    rxpdo.target_position = txpdo.actual_position;
                } else if (step <= 800) {
                    rxpdo.controlword = 0x0007;
                    rxpdo.target_position = txpdo.actual_position;
                } else if (step <= 1000) {
                    rxpdo.controlword = 0x000F;
                    rxpdo.target_position = txpdo.actual_position;
                } else {
					
					if(g_motorctl_stop_run)
					{
						can_quit = servoOff(&txpdo, &rxpdo);
						
						can_quit = can_quit;
						
						++quit_count;
						if( (quit_count >= 100) )
						{
							linux_printf("s:%x\n", txpdo.statusword);
							
							is_real_quit = 1;
						}	
					}
					else
					{
						// Update output PDO
						rxpdo.controlword = 0x000F;
						rxpdo.target_position = txpdo.actual_position + g_motorctl_target_step;
						rxpdo.mode_of_operation = 8;
					}	
                }

                // Send PDO data to the slaves
                for (int slave = 1; slave <= ec_slavecount; slave++) {
                    memcpy(ec_slave[slave].outputs, &rxpdo, sizeof(rxpdo_t));
                }

                // Print status information every 100 cycles
                if (dorun % 100 == 0) 
                {
                    /*linux_printf("%d pos=%d\n", dorun, txpdo.actual_position);*/
                }

                if (step < 1200) {
                    step++;
                }
            } else {
                linux_printf("WARNING: Working counter error (wkc: %d, expected: %d)\n", 
                       wkc_motorctl, expectedWKC_motorctl);
                       
                 #ifdef MEASURE_TIMING
		
					if ( (!g_motorctl_stop_run) )
					{
						LastStartCNT = 0;
					}
            
				#endif       
            }

            // Clock synchronization
            if (ec_slave[0].hasdc) {
                ec_sync(ec_DCtime, cycletime, &toff);
            }

            // Send process data
            ec_send_processdata();
            
            #ifdef MEASURE_TIMING
		
			if ( (!g_motorctl_stop_run) )
			{
				EndCNT = maincore_arm64_arch_timer_count();
            }
            
            #endif
            
            if(is_real_quit)
				break;
        }
    }
    
    inOP_motorctl = false; 
    
    #if 0
    
    osal_timert t;
        
        	ec_dcsync0(1, FALSE, cycletime, 0); // SYNC0 on slave 1
		osal_timer_start(&t, SYNC_CYCLE);
		
		while(osal_timer_is_expired(&t)==FALSE);
		
		ec_send_processdata();
		ec_receive_processdata(EC_TIMEOUTRET);
		
	#endif
		
	linux_printf("\nRequest init state for all slaves\n");
     ec_slave[0].state = EC_STATE_INIT;
     /* request INIT state for all slaves */
     ec_writestate(0);	
		
		linux_printf("emergency stop!\r\n");
}

int adsa2_run(char *ifname) {
    SLAVE_ID = 1; // Set the slave ID to 1
    int i; // Loop control variables
    
    
    linux_printf("__________STEP 1___________________\n");
    
    if (ec_init(ifname) <= 0)
    {
		linux_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
    linux_printf("EtherCAT master initialized successfully.\n");
    linux_printf("___________________________________________\n");

    // Search for EtherCAT slaves on the network
    if (ec_config_init(FALSE) <= 0) {
        linux_printf("Error: Cannot find EtherCAT slaves!\n");
        linux_printf("___________________________________________\n");
        ec_close(); // Close the EtherCAT connection
        return -1; // Return error if no slaves are found
    }
    linux_printf("%d slaves found and configured.\n", ec_slavecount); // Print the number of slaves found
    linux_printf("___________________________________________\n");

    // 2. Change to pre-operational state to configure the PDO registers
    linux_printf("__________STEP 2___________________\n");

    // Check if the slave is ready to map
    ec_readstate(); // Read the state of the slaves

    for(i = 1; i <= ec_slavecount; i++) { // Loop through each slave
		#if 0
		if(ec_slave[i].state != EC_STATE_PRE_OP) { // If the slave is not in PRE-OP state
            // Print the current state and status code of the slave
            linux_printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                   i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
            linux_printf("\nRequest init state for slave %d\n", i); // Request to change the state to INIT
            ec_slave[i].state = EC_STATE_INIT; // Set the slave state to INIT
            linux_printf("___________________________________________\n");
        } else
        #endif        
         { // If the slave is in PRE-OP state
            ec_slave[0].state = EC_STATE_PRE_OP; // Set the first slave to PRE-OP state
            /* Request EC_STATE_PRE_OP state for all slaves */
            ec_writestate(0); // Write the state change to the slave
            /* Wait for all slaves to reach the PRE-OP state */
            if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) {
                linux_printf("State changed to EC_STATE_PRE_OP: %d \n", EC_STATE_PRE_OP);
                linux_printf("___________________________________________\n");
            } else {
                linux_printf("State EC_STATE_PRE_OP cannot be changed in step 2\n");
                return -1; // Return error if state change fails
            }
        }
    }

//##################################################################################################
    //3.- Map RXPOD
    linux_printf("__________STEP 3___________________\n");

    // Clear RXPDO mapping
    int retval = 0; // Variable to hold the return value of SDO write operations
    uint16 map_1c12; // Variable to hold the mapping for PDO
    uint8 zero_map = 0; // Variable to clear the PDO mapping
    uint32 map_object; // Variable to hold the mapping object
    uint16 clear_val = 0x0000; // Value to clear the mapping

    for(i = 1; i <= ec_slavecount; i++) { // Loop through each slave
        // 1. First, disable PDO
        retval += ec_SDOwrite(i, 0x1600, 0x00, FALSE, sizeof(zero_map), &zero_map, EC_TIMEOUTSAFE);
        
        // 2. Configure new PDO mapping
        // Control Word
        map_object = 0x60400010;  // 0x6040:0 Control Word, 16 bits
        retval += ec_SDOwrite(i, 0x1600, 0x01, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);
        
        // Target Position
        map_object = 0x607A0020;  // 0x607A:0 Target Position, 32 bits
        retval += ec_SDOwrite(i, 0x1600, 0x02, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);
        
        // Mode of Operation
        map_object = 0x60600008;  // 0x6060:0 Mode of Operation, 8 bits
        retval += ec_SDOwrite(i, 0x1600, 0x03, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);
        
        // Padding (8 bits)
        map_object = 0x00000008;  // 8 bits padding
        retval += ec_SDOwrite(i, 0x1600, 0x04, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);
        
        // Set number of mapped objects
        uint8 map_count = 4;
        retval += ec_SDOwrite(i, 0x1600, 0x00, FALSE, sizeof(map_count), &map_count, EC_TIMEOUTSAFE);
        
        // 4. Configure RXPDO allocation
        clear_val = 0x0000; // Clear the mapping
        retval += ec_SDOwrite(i, 0x1c12, 0x00, FALSE, sizeof(clear_val), &clear_val, EC_TIMEOUTSAFE);
        map_1c12 = 0x1600; // Set the mapping to the new PDO
        retval += ec_SDOwrite(i, 0x1c12, 0x01, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);
        map_1c12 = 0x0001; // Set the mapping index
        retval += ec_SDOwrite(i, 0x1c12, 0x00, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);
    }

    linux_printf("PDO mapping configuration result: %d\n", retval);
    if (retval < 0) {
        linux_printf("PDO mapping failed\n");
        return -1;
    }

    linux_printf("RXPOD Mapping set correctly.\n");
    linux_printf("___________________________________________\n");

    //........................................................................................
    // Map TXPOD
    retval = 0;
    uint16 map_1c13;
    for(i = 1; i <= ec_slavecount; i++) {
        // First, clear the TXPDO mapping
        clear_val = 0x0000;
        retval += ec_SDOwrite(i, 0x1A00, 0x00, FALSE, sizeof(clear_val), &clear_val, EC_TIMEOUTSAFE);

        // Configure TXPDO mapping entries
        // Status Word (0x6041:0, 16 bits)
        map_object = 0x60410010;
        retval += ec_SDOwrite(i, 0x1A00, 0x01, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);

        // Actual Position (0x6064:0, 32 bits)
        map_object = 0x60640020;
        retval += ec_SDOwrite(i, 0x1A00, 0x02, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);

        // Actual Velocity (0x606C:0, 32 bits)
        map_object = 0x606C0020;
        retval += ec_SDOwrite(i, 0x1A00, 0x03, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);

        // Actual Torque (0x6077:0, 16 bits)
        map_object = 0x60770010;
        retval += ec_SDOwrite(i, 0x1A00, 0x04, FALSE, sizeof(map_object), &map_object, EC_TIMEOUTSAFE);

        // Set the number of mapped objects (4 objects)
        uint8 map_count = 4;
        retval += ec_SDOwrite(i, 0x1A00, 0x00, FALSE, sizeof(map_count), &map_count, EC_TIMEOUTSAFE);

        // Configure TXPDO assignment
        // First, clear the assignment
        clear_val = 0x0000;
        retval += ec_SDOwrite(i, 0x1C13, 0x00, FALSE, sizeof(clear_val), &clear_val, EC_TIMEOUTSAFE);

        // Assign TXPDO to 0x1A00
        map_1c13 = 0x1A00;
        retval += ec_SDOwrite(i, 0x1C13, 0x01, FALSE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);

        // Set the number of assigned PDOs (1 PDO)
        map_1c13 = 0x0001;
        retval += ec_SDOwrite(i, 0x1C13, 0x00, FALSE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);
    }

    linux_printf("Slave %d TXPDO mapping configuration result: %d\n", SLAVE_ID, retval);

    if (retval < 0) {
        linux_printf("TXPDO Mapping failed\n");
        linux_printf("___________________________________________\n");
        return -1;
    }

    linux_printf("TXPDO Mapping set successfully\n");
    linux_printf("___________________________________________\n");

   //##################################################################################################

    //4.- Set ecx_context.manualstatechange = 1. Map PDOs for all slaves by calling ec_config_map().
   linux_printf("__________STEP 4___________________\n");

   ecx_context.manualstatechange = 1; //Disable automatic state change
   osal_usleep(1e6); //Sleep for 1 second

    // Print the information of the slaves found
    for (i = 1; i <= ec_slavecount; i++) {
       // (void)ecx_FPWR(ecx_context.port, i, ECT_REG_DCSYNCACT, sizeof(WA), &WA, 5 * EC_TIMEOUTRET);
        linux_printf("Name: %s\n", ec_slave[i].name); //Print the name of the slave
        linux_printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", 
               i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_INIT);
        linux_printf("___________________________________________\n");
        ecx_dcsync0(&ecx_context, i, TRUE, cycletime_ns_motorctl, 0);  //Synchronize the distributed clock for the slave
    }

    // Map the configured PDOs to the IOmap
    ec_config_map(&IOmap_motorctl);

    linux_printf("__________STEP 5___________________\n");

    // Ensure all slaves are in PRE-OP state
    for(i = 1; i <= ec_slavecount; i++) {
        if(ec_slave[i].state != EC_STATE_PRE_OP) { // Check if the slave is not in PRE-OP state
            linux_printf("Slave %d not in PRE-OP state. Current state: %d\n", i, ec_slave[i].state);
            return -1; // Return error if any slave is not in PRE-OP state
        }
    }

    // Configure Distributed Clock (DC)
    ec_configdc(); // Set up the distributed clock for synchronization

    // Request to switch to SAFE-OP state
    ec_slave[0].state = EC_STATE_SAFE_OP; // Set the first slave to SAFE-OP state
    ec_writestate(0); // Write the state change to the slave

    // Wait for the state transition
    if (ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4) == EC_STATE_SAFE_OP) {
        linux_printf("Successfully changed to SAFE_OP state\n"); // Confirm successful state change
    } else {
        linux_printf("Failed to change to SAFE_OP state\n");
        return -1; // Return error if state change fails
    }

    // Calculate the expected Work Counter (WKC)
    expectedWKC_motorctl = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC; // Calculate expected WKC based on outputs and inputs
    linux_printf("Calculated workcounter %d\n", expectedWKC_motorctl);

    // Read and display basic status information of the slaves
    ec_readstate(); // Read the state of all slaves
    for(i = 1; i <= ec_slavecount; i++) {
        linux_printf("Slave %d\n", i);
        linux_printf("  State: %02x\n", ec_slave[i].state); // Print the state of the slave
        linux_printf("  ALStatusCode: %04x\n", ec_slave[i].ALstatuscode); // Print the AL status code
        linux_printf("  Delay: %d\n", ec_slave[i].pdelay); // Print the delay of the slave
        linux_printf("  Has DC: %d\n", ec_slave[i].hasdc); // Check if the slave supports Distributed Clock
        linux_printf("  DC Active: %d\n", ec_slave[i].DCactive); // Check if DC is active for the slave
        linux_printf("  DC supported: %d\n", ec_slave[i].hasdc); // Print if DC is supported
    }

    /* ADSB3 and ADSA2 not support 0x1C32*/
    
    #if 0
    
    // Read DC synchronization configuration using the correct parameters
    for(i = 1; i <= ec_slavecount; i++) {
        uint16_t dcControl = 0; // Variable to hold DC control configuration
        int32_t cycleTime = 0; // Variable to hold cycle time
        int size; // Variable to hold size for reading

        // Read DC synchronization configuration, adding the correct size parameter
        size = sizeof(dcControl);
        if (ec_SDOread(i, 0x1C32, 0x01, FALSE, &size, &dcControl, EC_TIMEOUTSAFE) > 0) {
            linux_printf("Slave %d DC Configuration:\n", i);
            linux_printf("  DC Control: 0x%04x\n", dcControl); // Print the DC control configuration
            
            size = sizeof(cycleTime);
            if (ec_SDOread(i, 0x1C32, 0x02, FALSE, &size, &cycleTime, EC_TIMEOUTSAFE) > 0) {
                linux_printf("  Cycle Time: %d ns\n", cycleTime); // Print the cycle time
            }
        }
    }
    
    #endif

    linux_printf("__________STEP 6___________________\n");

    // Start the EtherCAT thread for real-time processing
     // Flag to indicate that the EtherCAT thread should start
    osal_thread_create_rt(&thread1_id, 128000, (void *)&ecatthread, (void *)&ctime_thread); // Create the real-time EtherCAT thread
    // set_thread_affinity(*thread1, 4); // Optional: Set CPU affinity for the thread
    osal_thread_create(&thread2, 128000, (void *)&ecatcheck_motorctl, NULL); // Create the EtherCAT check thread
    // set_thread_affinity(*thread2, 5); // Optional: Set CPU affinity for the thread
    linux_printf("___________________________________________\n");


    // 8. Transition to OP state
    linux_printf("__________STEP 8___________________\n");

    // Send process data to the slaves
    ec_send_processdata();
    wkc_motorctl = ec_receive_processdata(EC_TIMEOUTRET); // Receive process data and store the Work Counter

    // Set the first slave to operational state
    ec_slave[0].state = EC_STATE_OPERATIONAL; // Change the state of the first slave to OP
    ec_writestate(0); // Write the state change to the slave

    // Wait for the state transition to complete
    if ((ec_statecheck(0, EC_STATE_OPERATIONAL, 5 * EC_TIMEOUTSTATE)) == EC_STATE_OPERATIONAL) {
        linux_printf("State changed to EC_STATE_OPERATIONAL: %d\n", EC_STATE_OPERATIONAL); // Confirm successful state change
        linux_printf("___________________________________________\n");
    } else {
        linux_printf("State could not be changed to EC_STATE_OPERATIONAL\n"); // Error message if state change fails
        for (int cnt = 1; cnt <= ec_slavecount; cnt++) {
            linux_printf("ALstatuscode: %d\n", ecx_context.slavelist[cnt].ALstatuscode); // Print AL status codes for each slave
        }
    }

    // Read and display the state of all slaves
    ec_readstate(); // Read the state of all slaves
    for (i = 1; i <= ec_slavecount; i++) {
        linux_printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", 
               i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_OPERATIONAL); // Print slave information
        linux_printf("Name: %s\n", ec_slave[i].name); // Print the name of the slave
        linux_printf("___________________________________________\n");
    }

    // 9. Configure servomotor and mode operation
    linux_printf("__________STEP 9___________________\n");

    if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
        linux_printf("Operational state reached for all slaves.\n");
        

        uint8 operation_mode = 8;
        uint16_t Control_Word = 128;

        for (i = 1; i <= ec_slavecount; i++) {
            ec_SDOwrite(i, 0x6040, 0x00, FALSE, sizeof(Control_Word), &Control_Word, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6060, 0x00, FALSE, sizeof(operation_mode), &operation_mode, EC_TIMEOUTSAFE);

        }
        
        start_ecatthread_thread = TRUE;
        
  // The main loop only needs to keep the program running
        while(!(is_real_quit)) {
            osal_usleep(100000); // Sleep for 100ms to reduce CPU usage
        }
    } 
    
    /*ec_reconfig_slave(1, EC_TIMEOUTMON);*/
    
    osal_usleep(1e6);
        
     #if 0
        
     linux_printf("\nRequest init state for all slaves\n");
     ec_slave[0].state = EC_STATE_INIT;
     /* request INIT state for all slaves */
     ec_writestate(0);
     
     #endif
     
      ec_close();

    linux_printf("EtherCAT master closed.\n");

    return 0;
}

int main(int argc, char *argv[])
{
    needlf_motorctl = FALSE;
    inOP_motorctl = FALSE;
    start_ecatthread_thread = FALSE;
    dorun = 0;
    ctime_thread = SYNC_CYCLE;
    cycletime_ns_motorctl = (uint32_t)1000 * SYNC_CYCLE;
    g_motorctl_stop_run = 0;
    is_real_quit = 0;
    currentgroup_motorctl = 0;
    
   linux_printf("SOEM (Simple Open EtherCAT Master)\nMotorCtl\n");
   
      
   adsa2_run("eth0");

   linux_printf("End program\n");
   
   #ifdef MEASURE_TIMING
   
	print_soem_lat();
   
   #endif
   
   return (0);
}
