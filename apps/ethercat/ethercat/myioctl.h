/*****************************************************************************
 *
 *  Copyright (C) 2006-2024  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master.
 *
 *  The file is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the
 *  Free Software Foundation; version 2.1 of the License.
 *
 *  This file is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this file. If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************/

/**
   \file
   EtherCAT master character device IOCTL commands.
*/

/****************************************************************************/

#ifndef __EC_IOCTL_H__
#define __EC_IOCTL_H__

//#include <linux/ioctl.h>
//#include <sys/ioctl.h>

#include <nuttx/fs/ioctl.h>

#include "globals.h"
#include "myglobals.h"

#define _WETHERCATBASE      (0x8c00) /* Ethercat modules ioctl network commands */

#define _ECIOC(nr)       _IOC(_WETHERCATBASE,nr)

/** \cond */

/** EtherCAT master ioctl() version magic.
 *
 * Increment this when changing the ioctl interface!
 */
#define EC_IOCTL_VERSION_MAGIC 37

// Command-line tool
#define EC_IOCTL_MODULE                _ECIOC(0x0001)
#define EC_IOCTL_MASTER                _ECIOC(0x0002)
#define EC_IOCTL_SLAVE                 _ECIOC(0x0003)
#define EC_IOCTL_SLAVE_SYNC           _ECIOC(0x0004)
#define EC_IOCTL_SLAVE_SYNC_PDO       _ECIOC(0x0005)
#define EC_IOCTL_SLAVE_SYNC_PDO_ENTRY _ECIOC(0x0006)
#define EC_IOCTL_DOMAIN               _ECIOC(0x0007)
#define EC_IOCTL_DOMAIN_FMMU          _ECIOC(0x0008)
#define EC_IOCTL_DOMAIN_DATA          _ECIOC(0x0009)
#define EC_IOCTL_MASTER_DEBUG         _ECIOC(0x000a)
#define EC_IOCTL_MASTER_RESCAN        _ECIOC(0x000b)
#define EC_IOCTL_SLAVE_STATE          _ECIOC(0x000c)
#define EC_IOCTL_SLAVE_SDO            _ECIOC(0x000d)
#define EC_IOCTL_SLAVE_SDO_ENTRY      _ECIOC(0x000e)
#define EC_IOCTL_SLAVE_SDO_UPLOAD     _ECIOC(0x000f)
#define EC_IOCTL_SLAVE_SDO_DOWNLOAD   _ECIOC(0x0010)
#define EC_IOCTL_SLAVE_SII_READ       _ECIOC(0x0011)
#define EC_IOCTL_SLAVE_SII_WRITE      _ECIOC(0x0012)
#define EC_IOCTL_SLAVE_REG_READ       _ECIOC(0x0013)
#define EC_IOCTL_SLAVE_REG_WRITE      _ECIOC(0x0014)
#define EC_IOCTL_SLAVE_FOE_READ       _ECIOC(0x0015)
#define EC_IOCTL_SLAVE_FOE_WRITE      _ECIOC(0x0016)
#define EC_IOCTL_SLAVE_SOE_READ       _ECIOC(0x0017)
#define EC_IOCTL_SLAVE_SOE_WRITE      _ECIOC(0x0018)
/*#ifdef EC_EOE*/
#if 1
#define EC_IOCTL_SLAVE_EOE_IP_PARAM    _ECIOC(0x0019)
#endif
#define EC_IOCTL_CONFIG               _ECIOC(0x001a)
#define EC_IOCTL_CONFIG_PDO           _ECIOC(0x001b)
#define EC_IOCTL_CONFIG_PDO_ENTRY     _ECIOC(0x001c)
#define EC_IOCTL_CONFIG_SDO           _ECIOC(0x001d)
#define EC_IOCTL_CONFIG_IDN           _ECIOC(0x001e)
#define EC_IOCTL_CONFIG_FLAG          _ECIOC(0x001f)
#ifdef EC_EOE
#define EC_IOCTL_CONFIG_EOE_IP_PARAM  _ECIOC(0x0020)
#define EC_IOCTL_EOE_HANDLER          _ECIOC(0x0021)
#endif

// Application interface
#define EC_IOCTL_REQUEST               _ECIOC(0x0022)
#define EC_IOCTL_CREATE_DOMAIN         _ECIOC(0x0023)
#define EC_IOCTL_CREATE_SLAVE_CONFIG   _ECIOC(0x0024)
#define EC_IOCTL_SELECT_REF_CLOCK      _ECIOC(0x0025)
#define EC_IOCTL_ACTIVATE              _ECIOC(0x0026)
#define EC_IOCTL_DEACTIVATE            _ECIOC(0x0027)
#define EC_IOCTL_SEND                  _ECIOC(0x0028)
#define EC_IOCTL_RECEIVE               _ECIOC(0x0029)
#define EC_IOCTL_MASTER_STATE          _ECIOC(0x002a)
#define EC_IOCTL_MASTER_LINK_STATE     _ECIOC(0x002b)
#define EC_IOCTL_APP_TIME              _ECIOC(0x002c)
#define EC_IOCTL_SYNC_REF              _ECIOC(0x002d)
#define EC_IOCTL_SYNC_REF_TO           _ECIOC(0x002e)
#define EC_IOCTL_SYNC_SLAVES           _ECIOC(0x002f)
#define EC_IOCTL_REF_CLOCK_TIME        _ECIOC(0x0030)
#define EC_IOCTL_SYNC_MON_QUEUE        _ECIOC(0x0031)
#define EC_IOCTL_SYNC_MON_PROCESS      _ECIOC(0x0032)
#define EC_IOCTL_RESET                 _ECIOC(0x0033)
#define EC_IOCTL_SC_SYNC               _ECIOC(0x0034)
#define EC_IOCTL_SC_WATCHDOG           _ECIOC(0x0035)
#define EC_IOCTL_SC_ADD_PDO            _ECIOC(0x0036)
#define EC_IOCTL_SC_CLEAR_PDOS         _ECIOC(0x0037)
#define EC_IOCTL_SC_ADD_ENTRY          _ECIOC(0x0038)
#define EC_IOCTL_SC_CLEAR_ENTRIES      _ECIOC(0x0039)
#define EC_IOCTL_SC_REG_PDO_ENTRY      _ECIOC(0x003a)
#define EC_IOCTL_SC_REG_PDO_POS        _ECIOC(0x003b)
#define EC_IOCTL_SC_DC                 _ECIOC(0x003c)
#define EC_IOCTL_SC_SDO                _ECIOC(0x003d)
#define EC_IOCTL_SC_EMERG_SIZE         _ECIOC(0x003e)
#define EC_IOCTL_SC_EMERG_POP          _ECIOC(0x003f)
#define EC_IOCTL_SC_EMERG_CLEAR        _ECIOC(0x0040)
#define EC_IOCTL_SC_EMERG_OVERRUNS    _ECIOC(0x0041)
#define EC_IOCTL_SC_SDO_REQUEST       _ECIOC(0x0042)
#define EC_IOCTL_SC_SOE_REQUEST       _ECIOC(0x0043)
#define EC_IOCTL_SC_REG_REQUEST       _ECIOC(0x0044)
#define EC_IOCTL_SC_VOE               _ECIOC(0x0045)
#define EC_IOCTL_SC_STATE             _ECIOC(0x0046)
#define EC_IOCTL_SC_IDN               _ECIOC(0x0047)
#define EC_IOCTL_SC_FLAG              _ECIOC(0x0048)
#ifdef EC_EOE
#define EC_IOCTL_SC_EOE_IP_PARAM      _ECIOC(0x0049)
#endif
#define EC_IOCTL_SC_STATE_TIMEOUT     _ECIOC(0x004a)
#define EC_IOCTL_DOMAIN_SIZE          _ECIOC(0x004b)
#define EC_IOCTL_DOMAIN_OFFSET        _ECIOC(0x004c)
#define EC_IOCTL_DOMAIN_PROCESS       _ECIOC(0x004d)
#define EC_IOCTL_DOMAIN_QUEUE         _ECIOC(0x004e)
#define EC_IOCTL_DOMAIN_STATE         _ECIOC(0x004f)
#define EC_IOCTL_SDO_REQUEST_INDEX    _ECIOC(0x0050)
#define EC_IOCTL_SDO_REQUEST_TIMEOUT  _ECIOC(0x0051)
#define EC_IOCTL_SDO_REQUEST_STATE    _ECIOC(0x0052)
#define EC_IOCTL_SDO_REQUEST_READ     _ECIOC(0x0053)
#define EC_IOCTL_SDO_REQUEST_WRITE    _ECIOC(0x0054)
#define EC_IOCTL_SDO_REQUEST_DATA     _ECIOC(0x0055)
#define EC_IOCTL_SOE_REQUEST_IDN      _ECIOC(0x0056)
#define EC_IOCTL_SOE_REQUEST_TIMEOUT  _ECIOC(0x0057)
#define EC_IOCTL_SOE_REQUEST_STATE    _ECIOC(0x0058)
#define EC_IOCTL_SOE_REQUEST_READ     _ECIOC(0x0059)
#define EC_IOCTL_SOE_REQUEST_WRITE    _ECIOC(0x005a)
#define EC_IOCTL_SOE_REQUEST_DATA     _ECIOC(0x005b)
#define EC_IOCTL_REG_REQUEST_DATA     _ECIOC(0x005c)
#define EC_IOCTL_REG_REQUEST_STATE    _ECIOC(0x005d)
#define EC_IOCTL_REG_REQUEST_WRITE    _ECIOC(0x005e)
#define EC_IOCTL_REG_REQUEST_READ     _ECIOC(0x005f)
#define EC_IOCTL_VOE_SEND_HEADER      _ECIOC(0x0060)
#define EC_IOCTL_VOE_REC_HEADER       _ECIOC(0x0061)
#define EC_IOCTL_VOE_READ             _ECIOC(0x0062)
#define EC_IOCTL_VOE_READ_NOSYNC      _ECIOC(0x0063)
#define EC_IOCTL_VOE_WRITE            _ECIOC(0x0064)
#define EC_IOCTL_VOE_EXEC             _ECIOC(0x0065)
#define EC_IOCTL_VOE_DATA             _ECIOC(0x0066)
#define EC_IOCTL_SET_SEND_INTERVAL    _ECIOC(0x0067)

/****************************************************************************/

#define EC_IOCTL_STRING_SIZE 64

/****************************************************************************/

typedef struct {
    uint32_t ioctl_version_magic;
    uint32_t master_count;
} ec_ioctl_module_t;

/****************************************************************************/

#if 0

typedef struct {
    uint32_t slave_count;
    uint32_t scan_index;
    uint32_t config_count;
    uint32_t domain_count;
    uint32_t eoe_handler_count;
    uint8_t phase;
    uint8_t active;
    uint8_t scan_busy;
    struct ec_ioctl_device {
        uint8_t address[6];
        uint8_t attached;
        uint8_t link_state;
        uint64_t tx_count;
        uint64_t rx_count;
        uint64_t tx_bytes;
        uint64_t rx_bytes;
        uint64_t tx_errors;
        int32_t tx_frame_rates[EC_RATE_COUNT];
        int32_t rx_frame_rates[EC_RATE_COUNT];
        int32_t tx_byte_rates[EC_RATE_COUNT];
        int32_t rx_byte_rates[EC_RATE_COUNT];
    } devices[EC_MAX_NUM_DEVICES];
    uint32_t num_devices;
    uint64_t tx_count;
    uint64_t rx_count;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    int32_t tx_frame_rates[EC_RATE_COUNT];
    int32_t rx_frame_rates[EC_RATE_COUNT];
    int32_t tx_byte_rates[EC_RATE_COUNT];
    int32_t rx_byte_rates[EC_RATE_COUNT];
    int32_t loss_rates[EC_RATE_COUNT];
    uint64_t app_time;
    uint64_t dc_ref_time;
    uint16_t ref_clock;
} ec_ioctl_master_t;

#else

#define MAX_SLAVE_COUNT 16

struct soem_slave {
	uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
    uint16_t alias;
    uint16_t boot_rx_mailbox_offset;
    uint16_t boot_rx_mailbox_size;
    uint16_t boot_tx_mailbox_offset;
    uint16_t boot_tx_mailbox_size;
    uint16_t mailbox_protocols;
    ec_sii_coe_details_t coe_details;
    int16_t current_on_ebus;
    uint8_t fmmu_bit;
    uint8_t dc_supported;
    int32_t transmission_delay;
    uint16_t next_slave;
    struct {
        uint32_t receive_time;                                
    } ports[EC_MAX_PORTS];
	char name[EC_IOCTL_STRING_SIZE];
};

typedef struct {
    uint32_t slave_count;
    struct soem_slave soem_slave_array[MAX_SLAVE_COUNT];
} ec_ioctl_master_t;

#endif


/****************************************************************************/

typedef struct {
    // input
    uint16_t position;
    
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
    uint16_t alias;
    uint16_t boot_rx_mailbox_offset;
    uint16_t boot_rx_mailbox_size;
    uint16_t boot_tx_mailbox_offset;
    uint16_t boot_tx_mailbox_size;
    uint16_t mailbox_protocols;
    ec_sii_coe_details_t coe_details;
    int16_t current_on_ebus;
    uint8_t fmmu_bit;
    uint8_t dc_supported;
    int32_t transmission_delay;
    uint16_t next_slave;
    struct {
        uint32_t receive_time;                                
    } ports[EC_MAX_PORTS];
	char name[EC_IOCTL_STRING_SIZE];

} ec_ioctl_slave_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;

    // outputs
    uint16_t physical_start_address;
    uint16_t default_size;
    uint8_t control_register;
    uint8_t enable;
    uint8_t pdo_count;
} ec_ioctl_slave_sync_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    
    #if 0
    
    uint32_t sync_index;
    uint32_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
    
    #endif
    
} ec_ioctl_slave_sync_pdo_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;
    uint32_t pdo_pos;
    uint32_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sync_pdo_entry_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t index;

    // outputs
    uint32_t data_size;
    uint32_t logical_base_address;
    uint16_t working_counter[EC_MAX_NUM_DEVICES];
    uint16_t expected_working_counter;
    uint32_t fmmu_count;
} ec_ioctl_domain_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t domain_index;
    uint32_t fmmu_index;

    // outputs
    uint16_t slave_config_alias;
    uint16_t slave_config_position;
    uint8_t sync_index;
    ec_direction_t dir;
    uint32_t logical_address;
    uint32_t data_size;
} ec_ioctl_domain_fmmu_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t domain_index;
    uint32_t data_size;
    uint8_t *target;
} ec_ioctl_domain_data_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint8_t al_state;
} ec_ioctl_slave_state_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    
    #if 0
    
    uint16_t sdo_position;

    // outputs
    uint16_t sdo_index;
    uint8_t max_subindex;
    int8_t name[EC_IOCTL_STRING_SIZE];
    
    #endif
    
} ec_ioctl_slave_sdo_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    int sdo_spec; // positive: index, negative: list position
    uint8_t sdo_entry_subindex;

    // outputs
    uint16_t data_type;
    uint16_t bit_length;
    uint8_t read_access[EC_SDO_ENTRY_ACCESS_COUNT];
    uint8_t write_access[EC_SDO_ENTRY_ACCESS_COUNT];
    int8_t description[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sdo_entry_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    size_t target_size;
    uint8_t *target;

    // outputs
    size_t data_size;
    uint32_t abort_code;
} ec_ioctl_slave_sdo_upload_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    uint8_t complete_access;
    size_t data_size;
    uint8_t *data;

    // outputs
    uint32_t abort_code;
} ec_ioctl_slave_sdo_download_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t is_alias;
    uint16_t offset;
    uint32_t nwords;
    uint16_t *words;
} ec_ioctl_slave_sii_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint8_t emergency;
    uint16_t address;
    size_t size;
    uint8_t *data;
} ec_ioctl_slave_reg_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t offset;
    size_t buffer_size;
    uint8_t *buffer;

    // outputs
    size_t data_size;
    uint32_t result;
    uint32_t error_code;
    char file_name[32];
} ec_ioctl_slave_foe_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint8_t drive_no;
    uint16_t idn;
    size_t mem_size;
    uint8_t *data;

    // outputs
    size_t data_size;
    uint16_t error_code;
} ec_ioctl_slave_soe_read_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint8_t drive_no;
    uint16_t idn;
    size_t data_size;
    uint8_t *data;

    // outputs
    uint16_t error_code;
} ec_ioctl_slave_soe_write_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // outputs
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    struct {
        ec_direction_t dir;
        ec_watchdog_mode_t watchdog_mode;
        uint32_t pdo_count;
        uint8_t config_this;
    } syncs[EC_MAX_SYNC_MANAGERS];
    uint16_t watchdog_divider;
    uint16_t watchdog_intervals;
    uint32_t sdo_count;
    uint32_t idn_count;
    uint32_t flag_count;
    int32_t slave_position;
    uint16_t dc_assign_activate;
    ec_sync_signal_t dc_sync[EC_SYNC_SIGNAL_COUNT];
} ec_ioctl_config_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;
    uint8_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_entry_t;

/****************************************************************************/

/** Maximum size for displayed SDO data.
 * \todo Make this dynamic.
 */
#define EC_MAX_SDO_DATA_SIZE 1024

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t sdo_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    size_t size;
    uint8_t data[EC_MAX_SDO_DATA_SIZE];
    uint8_t complete_access;
} ec_ioctl_config_sdo_t;

/****************************************************************************/

/** Maximum size for displayed IDN data.
 * \todo Make this dynamic.
 */
#define EC_MAX_IDN_DATA_SIZE 1024

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t idn_pos;

    // outputs
    uint8_t drive_no;
    uint16_t idn;
    ec_al_state_t state;
    size_t size;
    uint8_t data[EC_MAX_IDN_DATA_SIZE];
} ec_ioctl_config_idn_t;

/****************************************************************************/

/** Maximum size for key.
 */
#define EC_MAX_FLAG_KEY_SIZE 128

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t flag_pos;

    // outputs
    char key[EC_MAX_FLAG_KEY_SIZE];
    int32_t value;
} ec_ioctl_config_flag_t;

/****************************************************************************/

//#ifdef EC_EOE
#if 1 

typedef struct {
    // input
    uint16_t eoe_index;

    // outputs
    char name[EC_DATAGRAM_NAME_SIZE];
    uint16_t slave_position;
    uint8_t open;
    uint32_t rx_bytes;
    uint32_t rx_rate;
    uint32_t tx_bytes;
    uint32_t tx_rate;
    uint32_t tx_queued_frames;
    uint32_t tx_queue_size;
} ec_ioctl_eoe_handler_t;

#endif

/****************************************************************************/

#define EC_ETH_ALEN 6
#ifdef ETH_ALEN
#if ETH_ALEN != EC_ETH_ALEN
#error Ethernet address length mismatch
#endif
#endif

typedef struct {
    // input
    uint16_t slave_position;
    uint16_t config_index; // alternatively

    uint8_t mac_address_included;
    uint8_t ip_address_included;
    uint8_t subnet_mask_included;
    uint8_t gateway_included;
    uint8_t dns_included;
    uint8_t name_included;

    unsigned char mac_address[EC_ETH_ALEN];
    struct in_addr ip_address;
    struct in_addr subnet_mask;
    struct in_addr gateway;
    struct in_addr dns;
    char name[EC_MAX_HOSTNAME_SIZE];

	// output
	uint16_t result;
} ec_ioctl_eoe_ip_t;

/*****************************************************************************/

typedef struct {
    // outputs
    void *process_data;
    size_t process_data_size;
} ec_ioctl_master_activate_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t pdo_index;
    uint16_t entry_index;
    uint8_t entry_subindex;
    uint8_t entry_bit_length;
} ec_ioctl_add_pdo_entry_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t entry_index;
    uint8_t entry_subindex;
    uint32_t domain_index;

    // outputs
    unsigned int bit_position;
} ec_ioctl_reg_pdo_entry_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t sync_index;
    uint32_t pdo_pos;
    uint32_t entry_pos;
    uint32_t domain_index;

    // outputs
    unsigned int bit_position;
} ec_ioctl_reg_pdo_pos_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t index;
    uint8_t subindex;
    const uint8_t *data;
    size_t size;
    uint8_t complete_access;
} ec_ioctl_sc_sdo_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    size_t size;
    uint8_t *target;

    // outputs
    int32_t overruns;
} ec_ioctl_sc_emerg_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // outputs
    ec_slave_config_state_t *state;
} ec_ioctl_sc_state_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t drive_no;
    uint16_t idn;
    ec_al_state_t al_state;
    const uint8_t *data;
    size_t size;
} ec_ioctl_sc_idn_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    size_t key_size;
    char *key;
    int32_t value;
} ec_ioctl_sc_flag_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    ec_al_state_t from_state;
    ec_al_state_t to_state;
    uint32_t timeout_ms;
} ec_ioctl_sc_state_timeout_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t domain_index;

    // outputs
    ec_domain_state_t *state;
} ec_ioctl_domain_state_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // inputs/outputs
    uint32_t request_index;
    uint16_t sdo_index;
    uint8_t sdo_subindex;
    size_t size;
    uint8_t *data;
    uint32_t timeout;
    ec_request_state_t state;
} ec_ioctl_sdo_request_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // inputs/outputs
    uint32_t request_index;
    uint8_t drive_no;
    uint16_t idn;
    size_t size;
    uint8_t *data;
    uint32_t timeout;
    ec_request_state_t state;
} ec_ioctl_soe_request_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    size_t mem_size;

    // inputs/outputs
    uint32_t request_index;
    uint8_t *data;
    ec_request_state_t state;
    uint8_t new_data;
    uint16_t address;
    size_t transfer_size;
} ec_ioctl_reg_request_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // inputs/outputs
    uint32_t voe_index;
    uint32_t *vendor_id;
    uint16_t *vendor_type;
    size_t size;
    uint8_t *data;
    ec_request_state_t state;
} ec_ioctl_voe_t;

/****************************************************************************/

typedef struct {
    // inputs
    uint32_t dev_idx;

    // outputs
    ec_master_link_state_t *state;
} ec_ioctl_link_state_t;

/****************************************************************************/

#ifdef __KERNEL__

/** Context data structure for file handles.
 */
typedef struct {
    unsigned int writable; /**< Device was opened with write permission. */
    unsigned int requested; /**< Master was requested via this file handle. */
    uint8_t *process_data; /**< Total process data area. */
    size_t process_data_size; /**< Size of the \a process_data. */
} ec_ioctl_context_t;

long ec_ioctl(ec_master_t *, ec_ioctl_context_t *, unsigned int,
        void __user *);

#ifdef EC_RTDM

long ec_ioctl_rtdm_rt(ec_master_t *, ec_ioctl_context_t *, unsigned int,
        void __user *);
long ec_ioctl_rtdm_nrt(ec_master_t *, ec_ioctl_context_t *, unsigned int,
        void __user *);

#ifndef EC_RTDM_XENOMAI_V3
int ec_rtdm_mmap(ec_ioctl_context_t *, void **);
#endif

#endif

#endif

/****************************************************************************/

/** \endcond */

#endif
