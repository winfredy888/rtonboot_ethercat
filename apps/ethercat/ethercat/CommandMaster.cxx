/*****************************************************************************
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
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

#include <iostream>
#include <iomanip>

using namespace std;

#include "myostream.h"

#include "CommandMaster.h"
#include "MasterDevice.h"

#define MAX_TIME_STR_SIZE 50

/****************************************************************************/

CommandMaster::CommandMaster():
    Command("master", "Show master and Ethernet device information.")
{
}

/****************************************************************************/

string CommandMaster::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command-specific options:" << endl
        << "  --master -m <indices>  Master indices. A comma-separated" << endl
        << "                         list with ranges is supported." << endl
        << "                         Example: 1,4,5,7-9. Default: - (all)."
        << endl << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandMaster::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int dev_idx, j;
    time_t epoch;
    char time_str[MAX_TIME_STR_SIZE + 1];
    size_t time_str_size;
    
    int retval;
    
    int is_master_empty;
        
    stringstream str4;
    
    if(1)
    {
		g_err.str("");
		
        g_err << "ICOS does not support this command!!!";
        
        return -8;
	}	
	
	#if 0

    if (args.size()) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes no arguments!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    
    is_master_empty = 1;

	masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        
        is_master_empty = 0;
        
        retval = m.open(MasterDevice::Read);
        if(retval)
        {
			return retval;
		}	
			
        retval = m.getMaster(&data);
        if(retval)
			return retval;

        str4
            << "Master" << m.getIndex() << endl
            << "  Phase: ";

        switch (data.phase) {
            case 0:  str4 << "Waiting for device(s)..."; break;
            case 1:  str4 << "Idle"; break;
            case 2:  str4 << "Operation"; break;
            default: str4 << "???";
        }

        str4 << endl
            << "  Active: " << (data.active ? "yes" : "no") << endl
            << "  Slaves: " << data.slave_count << endl
            << "  Ethernet devices:" << endl;

        for (dev_idx = EC_DEVICE_MAIN; dev_idx < data.num_devices;
                dev_idx++) {
            str4 << "    " << (dev_idx == EC_DEVICE_MAIN ? "Main" : "Backup")
                << ": ";
            str4 << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[dev_idx].address[0]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[1]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[2]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[3]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[4]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[5]
                << " ("
                << (data.devices[dev_idx].attached ?
                        "attached" : "waiting...")
                << ")" << endl << dec
                << "      Link: "
                << (data.devices[dev_idx].link_state ? "UP" : "DOWN") << endl
                << "      Tx frames:   "
                << data.devices[dev_idx].tx_count << endl
                << "      Tx bytes:    "
                << data.devices[dev_idx].tx_bytes << endl
                << "      Rx frames:   "
                << data.devices[dev_idx].rx_count << endl
                << "      Rx bytes:    "
                << data.devices[dev_idx].rx_bytes << endl
                << "      Tx errors:   "
                << data.devices[dev_idx].tx_errors << endl
                << "      Tx frame rate [1/s]: "
                << setfill(' ') << setprecision(0) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                str4 << setw(ColWidth)
                    << data.devices[dev_idx].tx_frame_rates[j] / 1000.0;
                if (j < EC_RATE_COUNT - 1) {
                    str4 << " ";
                }
            }
            str4 << endl
                << "      Tx rate [KByte/s]:   "
                << setprecision(1) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                str4 << setw(ColWidth)
                    << data.devices[dev_idx].tx_byte_rates[j] / 1024.0;
                if (j < EC_RATE_COUNT - 1) {
                    str4 << " ";
                }
            }
            str4 << endl
                << "      Rx frame rate [1/s]: "
                << setfill(' ') << setprecision(0) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                str4 << setw(ColWidth)
                    << data.devices[dev_idx].rx_frame_rates[j] / 1000.0;
                if (j < EC_RATE_COUNT - 1) {
                    str4 << " ";
                }
            }
            str4 << endl
                << "      Rx rate [KByte/s]:   "
                << setprecision(1) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                str4 << setw(ColWidth)
                    << data.devices[dev_idx].rx_byte_rates[j] / 1024.0;
                if (j < EC_RATE_COUNT - 1) {
                    str4 << " ";
                }
            }
            str4 << setprecision(0) << endl;
        }
        unsigned int lost = data.tx_count - data.rx_count;
        if (lost == 1) {
            // allow one frame travelling
            lost = 0;
        }
        str4 << "    Common:" << endl
            << "      Tx frames:   "
            << data.tx_count << endl
            << "      Tx bytes:    "
            << data.tx_bytes << endl
            << "      Rx frames:   "
            << data.rx_count << endl
            << "      Rx bytes:    "
            << data.rx_bytes << endl
            << "      Lost frames: " << lost << endl
            << "      Tx frame rate [1/s]: "
            << setfill(' ') << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            str4 << setw(ColWidth)
                << data.tx_frame_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << endl
            << "      Tx rate [KByte/s]:   "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            str4 << setw(ColWidth)
                << data.tx_byte_rates[j] / 1024.0;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << endl
            << "      Rx frame rate [1/s]: "
            << setfill(' ') << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            str4 << setw(ColWidth)
                << data.rx_frame_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << endl
            << "      Rx rate [KByte/s]:   "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            str4 << setw(ColWidth)
                << data.rx_byte_rates[j] / 1024.0;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << endl
            << "      Loss rate [1/s]:     "
            << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            str4 << setw(ColWidth)
                << data.loss_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << endl
            << "      Frame loss [%]:      "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            double perc = 0.0;
            if (data.tx_frame_rates[j]) {
                perc = 100.0 * data.loss_rates[j] / data.tx_frame_rates[j];
            }
            str4 << setw(ColWidth) << perc;
            if (j < EC_RATE_COUNT - 1) {
                str4 << " ";
            }
        }
        str4 << setprecision(0) << endl;

        str4 << "  Distributed clocks:" << endl
            << "    Reference clock:   ";
        if (data.ref_clock != 0xffff) {
            str4 << "Slave " << dec << data.ref_clock;
        } else {
            str4 << "None";
        }
        str4 << endl
            << "    DC reference time: " << data.dc_ref_time << endl
            << "    Application time:  " << data.app_time << endl
            << "                       ";

        epoch = data.app_time / 1000000000 + 946684800ULL;
        time_str_size = strftime(time_str, MAX_TIME_STR_SIZE,
                "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
        str4 << string(time_str, time_str_size) << "."
            << setfill('0') << setw(9) << data.app_time % 1000000000 << endl;
    }
    
    if(is_master_empty)
    {
		g_err.str("");
		
        g_err << "getMasterIndices parse error do nothing";
        
        return -3;
	}	
    
     mycsout << str4.str();
     
     #endif
     
     return 0;
}

/****************************************************************************/
