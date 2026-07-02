#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "../../../nuttx/libs/libsoem/ethercat.h"

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

extern int g_motorpp_stop_run;

int is_real_quit_motorpp = 0;

char IOmap_motorpp[4096];

int expectedWKC_motorpp; // Expected Work Counter
boolean needlf_motorpp; // Flag to indicate if a line feed is needed
volatile int wkc_motorpp; // Work Counter (volatile to ensure it is updated correctly in multi-threaded context)
boolean inOP_motorpp;

uint8 currentgroup_motorpp = 0;

#define EC_TIMEOUTMON 5000

#define SYNC_CYCLE 125

uint32_t cycletime_ns_motorpp; 

int64 toff_motorpp, gl_delta_motorpp;

int dorun_motorpp = 0;

bool start_ecatthread_thread_motorpp;

int ctime_thread_motorpp;

pthread_mutex_t target_position_mutex;

bool target_position_updated = false;

volatile int32_t target_position = 0;

OSAL_THREAD_HANDLE thread1_motorpp; // Handle for the EtherCAT check thread
OSAL_THREAD_HANDLE thread2_motorpp;

int8_t SLAVE_ID_motorpp; // Slave ID for EtherCAT communication

struct MotorStatus {
    bool is_operational;
    uint16_t status_word;
    int32_t actual_position;
    int32_t actual_velocity;
    int16_t actual_torque;
} motor_status;

// Structure for RXPDO (Control data sent to slave)
typedef struct {
    uint16_t controlword;      // 0x6040:0, 16 bits
    int32_t target_position;   // 0x607A:0, 32 bits
    uint8_t mode_of_operation; // 0x6060:0, 8 bits
    uint8_t padding;          // 8 bits padding for alignment
} __attribute__((__packed__)) rxpdo_t;

// Structure for TXPDO (Status data received from slave)
typedef struct {
    uint16_t statusword;      // 0x6041:0, 16 bits
    int32_t actual_position;  // 0x6064:0, 32 bits
    int32_t actual_velocity;  // 0x606C:0, 32 bits
    int16_t actual_torque;    // 0x6077:0, 16 bits
} __attribute__((__packed__)) txpdo_t;

typedef enum {  NOT_READY = 0, 
                SWITCH_DISABLED,
                READY_SWITCH, 
                SWITCHED_ON,
                OPERATION_ENABLED,
                QUICK_STOP,
                FAULT_REACTION,
                FAULT,
                UNKNOWN} PDS_STATUS; 
                
PDS_STATUS getPDSStatus_pp(txpdo_t * p_txpdo)
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

int servoOff_pp(txpdo_t * p_txpdo, rxpdo_t * p_rxpdo)
{
	PDS_STATUS pd_status;
	
	pd_status = getPDSStatus_pp(p_txpdo);
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

rxpdo_t g_rxpdo;
txpdo_t g_txpdo;

OSAL_THREAD_FUNC ecatcheck_motorpp(void *ptr) {
    int slave; // Variable to hold the current slave index
    (void)ptr; // Not used

    while ((!is_real_quit_motorpp)) { // Infinite loop for monitoring
        if (inOP_motorpp && ((wkc_motorpp < expectedWKC_motorpp) || ec_group[currentgroup_motorpp].docheckstate)) {
            if (needlf_motorpp) {
                needlf_motorpp = FALSE; // Reset line feed flag
                linux_printf("\n"); // Print a new line
            }
            ec_group[currentgroup_motorpp].docheckstate = FALSE; // Reset check state
            ec_readstate(); // Read the state of all slaves
            for (slave = 1; slave <= ec_slavecount; slave++) { // Loop through each slave
                if ((ec_slave[slave].group == currentgroup_motorpp) && (ec_slave[slave].state != EC_STATE_OPERATIONAL)) {
                    ec_group[currentgroup_motorpp].docheckstate = TRUE; // Set check state if slave is not operational
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        linux_printf("ERROR: Slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK); // Acknowledge error state
                        ec_writestate(slave); // Write the state change to the slave
                    } else if (ec_slave[slave].state == EC_STATE_SAFE_OP) {
                        linux_printf("WARNING: Slave %d is in SAFE_OP, changing to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL; // Change state to operational
                        ec_writestate(slave); // Write the state change to the slave
                    } else if (ec_slave[slave].state > EC_STATE_NONE) {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON)) { // Reconfigure the slave if needed
                            ec_slave[slave].islost = FALSE; // Mark slave as found
                            linux_printf("MESSAGE: Slave %d reconfigured\n", slave);
                        }
                    } else if (!ec_slave[slave].islost) {
                        ec_statecheck(slave, EC_STATE_OPERATIONAL,  EC_TIMEOUTRET); // Check the state of the slave
                        if (ec_slave[slave].state == EC_STATE_NONE) {
                            ec_slave[slave].islost = TRUE; // Mark slave as lost
                            linux_printf("ERROR: Slave %d lost\n", slave);
                        }
                    }
                }
                if (ec_slave[slave].islost) { // If the slave is marked as lost
                    if (ec_slave[slave].state == EC_STATE_NONE) {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON)) { // Attempt to recover the lost slave
                            ec_slave[slave].islost = FALSE; // Mark slave as found
                            linux_printf("MESSAGE: Slave %d recovered\n", slave);
                        }
                    } else {
                        ec_slave[slave].islost = FALSE; // Mark slave as found
                        linux_printf("MESSAGE: Slave %d found\n", slave);
                    }
                }
            }
            if (!ec_group[currentgroup_motorpp].docheckstate) {
                linux_printf("OK: All slaves resumed OPERATIONAL.\n"); // Confirm all slaves are operational
            }
        }
        
        osal_usleep(10000); 
    }
}

void ec_sync_motorpp(int64 reftime, int64 cycletime, int64 *offsettime) {
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
    gl_delta_motorpp = delta; // Update global delta variable
}

OSAL_THREAD_FUNC_RT ecatthread_motorpp(void *ptr) {
    int64 cycletime; // Variable to hold the cycle time

    cycletime = *(int *)ptr * 1000; /* Convert cycle time from ms to ns */

    toff_motorpp = 0; // Initialize time offset
    dorun_motorpp = 0; // Initialize run flag
    ec_send_processdata(); // Send initial process data

    while ((!is_real_quit_motorpp)) { // Infinite loop for real-time processing
        dorun_motorpp++; // Increment run counter
    
        
        osal_usleep( (cycletime + toff_motorpp) / 1000);

        if (start_ecatthread_thread_motorpp) { // Check if the EtherCAT thread should run
            wkc_motorpp = ec_receive_processdata(EC_TIMEOUTRET); // Receive process data and store the Work Counter

            if (ec_slave[0].hasdc) { // If the first slave supports Distributed Clock
                /* Calculate toff to synchronize Linux time with DC time */
                ec_sync_motorpp(ec_DCtime, cycletime, &toff_motorpp); // Synchronize time
            }

            ec_send_processdata(); // Send process data to the slaves
        }
    }
}

void update_motor_status(int slave_id) {
    motor_status.status_word = g_txpdo.statusword;
    motor_status.actual_position = g_txpdo.actual_position;
    motor_status.actual_velocity = g_txpdo.actual_velocity;
    motor_status.actual_torque = g_txpdo.actual_torque;
    
    // 检查状态字以确定是否可操作
    motor_status.is_operational = (g_txpdo.statusword & 0x0F) == 0x07;  // 检查是否使能并准备好
}

int adsa2_pp_run(char * ifname) {
    SLAVE_ID_motorpp = 1; // Set the slave ID to 1

    int quit_count = 0;
    int can_quit;
    
    // 1. Call ec_config_init() to move from INIT to PRE-OP state.
    linux_printf("__________STEP 1___________________\n");
    // Initialize EtherCAT master on the specified network interface
    if (ec_init(ifname) <= 0) {
        linux_printf("Error: Could not initialize EtherCAT master!\n");
        linux_printf("No socket connection on Ethernet port. Execute as root.\n");
        linux_printf("___________________________________________\n");
        return -1; // Return error if initialization fails
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

    for(int i = 1; i <= ec_slavecount; i++) { // Loop through each slave
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

    for(int i = 1; i <= ec_slavecount; i++) { // Loop through each slave
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
    for(int i = 1; i <= ec_slavecount; i++) {
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

    linux_printf("Slave %d TXPDO mapping configuration result: %d\n", SLAVE_ID_motorpp, retval);

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
    for (int i = 1; i <= ec_slavecount; i++) {
       // (void)ecx_FPWR(ecx_context.port, i, ECT_REG_DCSYNCACT, sizeof(WA), &WA, 5 * EC_TIMEOUTRET);
        linux_printf("Name: %s\n", ec_slave[i].name); //Print the name of the slave
        linux_printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", 
               i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_INIT);
        linux_printf("___________________________________________\n");
        ecx_dcsync0(&ecx_context, i, TRUE, cycletime_ns_motorpp, 0);  //Synchronize the distributed clock for the slave
    }

    // Map the configured PDOs to the IOmap
    ec_config_map(&IOmap_motorpp);

    linux_printf("__________STEP 5___________________\n");

    // Ensure all slaves are in PRE-OP state
    for(int i = 1; i <= ec_slavecount; i++) {
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
    expectedWKC_motorpp = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC; // Calculate expected WKC based on outputs and inputs
    linux_printf("Calculated workcounter %d\n", expectedWKC_motorpp);

    // Read and display basic status information of the slaves
    ec_readstate(); // Read the state of all slaves
    for(int i = 1; i <= ec_slavecount; i++) {
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
    for(int i = 1; i <= ec_slavecount; i++) {
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
    start_ecatthread_thread_motorpp = TRUE; // Flag to indicate that the EtherCAT thread should start
    osal_thread_create_rt(&thread1_motorpp, 128000, (void *)&ecatthread_motorpp, (void *)&ctime_thread_motorpp); // Create the real-time EtherCAT thread
    // set_thread_affinity(*thread1, 4); // Optional: Set CPU affinity for the thread
    osal_thread_create(&thread2_motorpp, 128000, (void *)&ecatcheck_motorpp, NULL); // Create the EtherCAT check thread
    // set_thread_affinity(*thread2, 5); // Optional: Set CPU affinity for the thread
    linux_printf("___________________________________________\n");


    // 8. Transition to OP state
    linux_printf("__________STEP 8___________________\n");

    // Send process data to the slaves
    ec_send_processdata();
    wkc_motorpp = ec_receive_processdata(EC_TIMEOUTRET); // Receive process data and store the Work Counter

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
    for (int i = 1; i <= ec_slavecount; i++) {
        linux_printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", 
               i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_OPERATIONAL); // Print slave information
        linux_printf("Name: %s\n", ec_slave[i].name); // Print the name of the slave
        linux_printf("___________________________________________\n");
    }

    // 9. Configure servomotor and mode operation
    linux_printf("__________STEP 9___________________\n");

    if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
        linux_printf("Operational state reached for all slaves.\n");
        
        uint8 operation_mode = 1;  // PP Mode = 1
        
        // 初始化PDO数据
        rxpdo_t rxpdo;
        txpdo_t txpdo;
        
        uint16_t  Control_Word;
        Control_Word = 128;
        

        uint32_t  Profile_velocity;
        Profile_velocity = 50000;

        uint32_t  Profile_acceleration;
        Profile_acceleration = 150000;

        uint32_t  Profile_deceleration;
        Profile_deceleration = 150000;
        
        txpdo.statusword = 0;


        for (int i = 1; i <= ec_slavecount; i++) {
            ec_SDOwrite(i, 0x6040, 0x00, FALSE, sizeof(Control_Word), &Control_Word, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6060, 0x00, FALSE, sizeof(operation_mode), &operation_mode, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6081, 0x00, FALSE, sizeof(Profile_velocity), &Profile_velocity, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6083, 0x00, FALSE, sizeof(Profile_acceleration), &Profile_acceleration, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6084, 0x00, FALSE, sizeof(Profile_deceleration), &Profile_deceleration, EC_TIMEOUTSAFE);

        }
        
        int step = 0;

        // 主循环
        for(int i = 1; i <= 3 * 60 * 60 * 1000; i++) {
				
            ec_send_processdata();
            wkc_motorpp = ec_receive_processdata(EC_TIMEOUTRET);

            

            // Send output data to each slave
            for (int slave = 1; slave <= ec_slavecount; slave++) {
                memcpy(ec_slave[slave].outputs, &rxpdo, sizeof(rxpdo_t));
            }

            if(wkc_motorpp >= expectedWKC_motorpp) {
                if (i % 1000 == 0) {  // Print every 100 cycles
                    for(int slave = 1; slave <= ec_slavecount; slave++) {
                        // Get input data using the structure
                        memcpy(&txpdo, ec_slave[slave].inputs, sizeof(txpdo_t));
                
						#if 0
						
                        // Print received data
                        linux_printf("Slave %d:\n", slave);
                        linux_printf("  Status Word: 0x%04x\n", txpdo.statusword);
                        linux_printf("  Position: %d\n", txpdo.actual_position);
                        linux_printf("  Velocity: %d\n", txpdo.actual_velocity);
                        linux_printf("  Torque: %d\n", txpdo.actual_torque);
                        
                        // Parse status word
                        linux_printf("  State: ");
                        if (txpdo.statusword & 0x0001) linux_printf("Ready to switch on ");
                        if (txpdo.statusword & 0x0002) linux_printf("Switched on ");
                        if (txpdo.statusword & 0x0004) linux_printf("Operation enabled ");
                        if (txpdo.statusword & 0x0008) linux_printf("Fault ");
                        if (txpdo.statusword & 0x0010) linux_printf("Voltage enabled ");
                        if (txpdo.statusword & 0x0020) linux_printf("Quick stop ");
                        if (txpdo.statusword & 0x0040) linux_printf("Switch on disabled ");
                        if (txpdo.statusword & 0x0080) linux_printf("Warning ");
                        linux_printf("\n");
                    }
                    
                    #else
                    
                    /*linux_printf("%d  ActPos: %d\n", i, txpdo.actual_position);*/
                    
                    #endif
                    
                    /*linux_printf("----------------------------------------\n");*/
					}
               }
                
                needlf_motorpp = TRUE;

           if (step <= 200) {
                // Initial state
                rxpdo.controlword = 0x0080;
                rxpdo.target_position = 0;
                rxpdo.mode_of_operation = 1;
                rxpdo.padding = 0;
            }
            else if (step <= 300) {
                // Shutdown command
                rxpdo.controlword = 0x0006;
                rxpdo.target_position = 0;
                rxpdo.mode_of_operation = 1;
                rxpdo.padding = 0;
            }
            else if (step <= 400) {
                // Switch On command
                rxpdo.controlword = 0x0007;
                rxpdo.target_position = 0;
                rxpdo.mode_of_operation = 1;
                rxpdo.padding = 0;
            }
            else if (step <= 500) {
                // Enable Operation command
                rxpdo.controlword = 0x000F;
                rxpdo.target_position = 0;
                rxpdo.mode_of_operation = 1;
                rxpdo.padding = 0;
            }
            else if(g_motorpp_stop_run)
			{
				can_quit = servoOff_pp(&txpdo, &rxpdo);
						
				can_quit = can_quit;
						
				++quit_count;
				if( (quit_count >= 100) )
				{
					linux_printf("s:%x\n", txpdo.statusword);
					
					is_real_quit_motorpp = 1;
				}	
			}		
            else {
                // Normal operation with position control
                update_motor_status(SLAVE_ID_motorpp);  // 添加电机状态更新
                
                pthread_mutex_lock(&target_position_mutex);
                
                if (step > 800) {
                    static bool new_setpoint = false;
                    static uint16_t control_toggle = 0x0000;
                    
                    rxpdo.mode_of_operation = 1;  // PP模式
                    rxpdo.padding = 0;
                    
                    if (target_position_updated) {
                        // 收到新的目标位置
                        rxpdo.target_position = target_position;
                        new_setpoint = true;
                        control_toggle ^= 0x0040;  // 切换控制位
                        target_position_updated = false;
                        /*linux_printf(">>> Received new target position: %d\n", target_position);*/
                    }

                    if (new_setpoint) {
                        // 设置新位置触发位和绝对位置位
                        rxpdo.controlword = 0x000F | 0x0010 | control_toggle;  // Enable + new setpoint + toggle bit
                        /*linux_printf(">>> Triggering motion with control word: 0x%04x\n", rxpdo.controlword);*/
                        new_setpoint = false;
                    } else {
                        // 保持使能状态
                        rxpdo.controlword = 0x000F | control_toggle;
                    }

                    // 添加调试信息
                    
                    #if 0
                    
                    if (i % 1000 == 0) {
                        linux_printf("\n--- Motion Status Update ---\n");
                        linux_printf("Target Position: %d\n", rxpdo.target_position);
                        linux_printf("Actual Position: %d\n", txpdo.actual_position);
                        linux_printf("Control Word: 0x%04x\n", rxpdo.controlword);
                        linux_printf("Status Word: 0x%04x\n", txpdo.statusword);
                        linux_printf("Position Error: %d\n", rxpdo.target_position - txpdo.actual_position);
                        linux_printf("-------------------------\n\n");
                    }
                    
                    #endif
                }
                
                pthread_mutex_unlock(&target_position_mutex);
            }
          }  

            if(step < 900) {
                step += 1;
            }
            
            if(is_real_quit_motorpp)
					break;

            osal_usleep(SYNC_CYCLE);
        }
    }
    
    /*osal_timert t;
        
    	ec_dcsync0(1, FALSE, cycletime_ns_motorpp, 0); // SYNC0 on slave 1
		osal_timer_start(&t, 1000);
		
		while(osal_timer_is_expired(&t)==FALSE);
		
		ec_send_processdata();
		ec_receive_processdata(EC_TIMEOUTRET);*/
		
		linux_printf("emergency stop!\r\n");
		
		linux_printf("\nRequest init state for all slaves\n");
		ec_slave[0].state = EC_STATE_INIT;
		//request INIT state for all slaves 
		ec_writestate(0); 

    osal_usleep(1e6);

   
     ec_close();

    linux_printf("EtherCAT master closed.\n");

    return 0;
}


int main(int argc, char **argv) 
{
	needlf_motorpp = FALSE;
    inOP_motorpp = FALSE;
    start_ecatthread_thread_motorpp = FALSE;
    dorun_motorpp = 0;
    ctime_thread_motorpp = SYNC_CYCLE; 
    cycletime_ns_motorpp = (uint32_t)1000 * SYNC_CYCLE;
    is_real_quit_motorpp = 0;
    g_motorpp_stop_run = 0;
    currentgroup_motorpp = 0;
    
    pthread_mutex_init(&target_position_mutex, NULL);
    
    linux_printf("SOEM (Simple Open EtherCAT Master)\nMotorpp\n");
   
      
   adsa2_pp_run("eth0");

   linux_printf("End program\n");
    
	return 0;
}
