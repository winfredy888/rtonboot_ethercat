/*****************************************************************************
 *
 *  Copyright (C) 2006-2024  Florian Pose, Ingenieurgemeinschaft IgH
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
 *  vim: expandtab
 *
 ****************************************************************************/

#include "CommandConfig.h"
#include "MasterDevice.h"

#include <list>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "myostream.h"

using namespace std;

#include <arpa/inet.h>

/****************************************************************************/

CommandConfig::CommandConfig():
    Command("config", "Show slave configurations.")
{
}

/****************************************************************************/

string CommandConfig::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Without the --verbose option, slave configurations are" << endl
        << "output one-per-line. Example:" << endl
        << endl
        << "1001:0  0x0000003b/0x02010000  3  OP" << endl
        << "|       |                      |  |" << endl
        << "|       |                      |  \\- Application-layer" << endl
        << "|       |                      |     state of the attached" << endl
        << "|       |                      |     slave, or '-', if no" << endl
        << "|       |                      |     slave is attached." << endl
        << "|       |                      \\- Absolute decimal ring" << endl
        << "|       |                         position of the attached" << endl
        << "|       |                         slave, or '-' if none" << endl
        << "|       |                         attached." << endl
        << "|       \\- Expected vendor ID and product code (both" << endl
        << "|          hexadecimal)." << endl
        << "\\- Alias address and relative position (both decimal)." << endl
        << endl
        << "With the --verbose option given, the configured PDOs and" << endl
        << "SDOs are output in addition." << endl
        << endl
        << "Configuration selection:" << endl
        << "  Slave configurations can be selected with" << endl
        << "  the --alias and --position parameters as follows:" << endl
        << endl
        << "  1) If neither the --alias nor the --position option" << endl
        << "     is given, all slave configurations are displayed." << endl
        << "  2) If only the --position option is given, an alias" << endl
        << "     of zero is assumed (see 4))." << endl
        << "  3) If only the --alias option is given, all slave" << endl
        << "     configurations with the given alias address" << endl
        << "     are displayed." << endl
        << "  4) If both the --alias and the --position option are" << endl
        << "     given, the selection can match a single" << endl
        << "     configuration, that is displayed, if it exists." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>  Configuration alias (see above)." << endl
        << "  --position -p <pos>    Relative position (see above)." << endl
        << "  --verbose  -v          Show detailed configurations." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

/** Lists the bus configuration.
 */
int CommandConfig::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    bool doIndent;
    ConfigList configs;
    int retval;
    int is_master_empty;
    
    if(1)
    {
		g_err.str("");
		
        g_err << "ICOS does not support this command!!!";
        
        return -8;
	}
    
    if (args.size()) {
        stringstream err;
        
        g_err.str("");
        
        g_err << "'" << getName() << "' takes no arguments!";
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    
    is_master_empty = 1;

    masterIndices = getMasterIndices();
    doIndent = masterIndices.size() > 1;
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        
        is_master_empty = 0;
        
        retval = m.open(MasterDevice::Read);
        if(retval)
			return retval;
			
        configs = selectedConfigs(m);
        
        stringstream str2;

        if (configs.size() && doIndent) {
            str2 << "Master" << dec << m.getIndex() << endl;
        }
        
        mycsout << str2.str();
        
        if(configs.size() == 0)
        {
			g_err.str("");
        
			g_err << "no config get nothing to do!";
			
			return -3;
		}	

        if (getVerbosity() == Verbose) {
            retval = showDetailedConfigs(m, configs, doIndent);
        } else {		
            retval = listConfigs(m, configs, doIndent);
        }
        
        if(retval)
			return retval;
    }
    
    if(is_master_empty)
    {
		g_err.str("");
		
        g_err << "getMasterIndices parse error do nothing";
        
        return -3;
	}
    
    return 0;
}

/****************************************************************************/

/** Lists the complete bus configuration.
 */
int CommandConfig::showDetailedConfigs(
        MasterDevice &m,
        const ConfigList &configList,
        bool doIndent
        )
{
    ConfigList::const_iterator configIter;
    unsigned int i, j, k, l;
    ec_ioctl_slave_t slave;
    ec_ioctl_config_pdo_t pdo;
    ec_ioctl_config_pdo_entry_t entry;
    ec_ioctl_config_sdo_t sdo;
    ec_ioctl_config_idn_t idn;
    ec_ioctl_config_flag_t flag;
#ifdef EC_EOE
    ec_ioctl_eoe_ip_t ip;
#endif
    string indent(doIndent ? "  " : "");
    
    stringstream str1;
    
    int retval;
    
    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        str1 << indent
            << "Alias: "
            << dec << configIter->alias << endl << indent
            << "Position: " << configIter->position << endl << indent
            << "Vendor Id: 0x"
            << hex << setfill('0')
            << setw(8) << configIter->vendor_id << endl << indent
            << "Product code: 0x"
            << setw(8) << configIter->product_code << endl << indent
            << "Attached slave: ";

        if (configIter->slave_position != -1) {
            retval = m.getSlave(&slave, configIter->slave_position);
            if(retval)
				return retval;
			
			#if 0 /*Winfred*/
				
            str1 << dec << configIter->slave_position
                << " (" << alStateString(slave.al_state) << ")" << endl;
                
            #endif    
        } else {
            str1 << "none" << endl;
        }

        str1 << indent << "Watchdog divider: ";
        if (configIter->watchdog_divider) {
            str1 << dec << configIter->watchdog_divider;
        } else {
            str1 << "(Default)";
        }
        str1 << endl << indent
            << "Watchdog intervals: ";
        if (configIter->watchdog_intervals) {
            str1 << dec << configIter->watchdog_intervals;
        } else {
            str1 << "(Default)";
        }
        str1 << endl;

        for (j = 0; j < EC_MAX_SYNC_MANAGERS; j++) {
            if (configIter->syncs[j].pdo_count) {
                str1 << indent << "SM" << dec << j << ", Dir: "
                    << (configIter->syncs[j].dir == EC_DIR_INPUT
                            ? "Input" : "Output") << ", Watchdog: ";
                switch (configIter->syncs[j].watchdog_mode) {
                    case EC_WD_DEFAULT: str1 << "Default"; break;
                    case EC_WD_ENABLE: str1 << "Enable"; break;
                    case EC_WD_DISABLE: str1 << "Disable"; break;
                    default: str1 << "???"; break;
                }
                str1 << endl;

                for (k = 0; k < configIter->syncs[j].pdo_count; k++) {
                    retval = m.getConfigPdo(&pdo, configIter->config_index, j, k);
                    if(retval)
						return retval;

                    str1 << indent << "  PDO 0x" << hex << setfill('0')
                        << setw(4) << pdo.index << endl;

                    for (l = 0; l < pdo.entry_count; l++) {
                        retval = m.getConfigPdoEntry(&entry,
                                configIter->config_index, j, k, l);
                        if(retval)
							return retval;

                        str1 << indent << "    PDO entry 0x"
                            << hex << setfill('0')
                            << setw(4) << entry.index << ":"
                            << setw(2) << (unsigned int) entry.subindex
                            << ", " << dec << setfill(' ')
                            << setw(2) << (unsigned int) entry.bit_length
                            << " bit" << endl;
                    }
                }
            }
        }

        str1 << indent << "SDO configuration:" << endl;
        if (configIter->sdo_count) {
            for (j = 0; j < configIter->sdo_count; j++) {
                retval = m.getConfigSdo(&sdo, configIter->config_index, j);
                if(retval)
					return retval;

                str1 << indent << "  0x"
                    << hex << setfill('0')
                    << setw(4) << sdo.index;
                if (sdo.complete_access) {
                    str1 << " C";
                }
                else {
                    str1 << ":" << setw(2) << (unsigned int) sdo.subindex;
                }
                str1 << ", " << dec << sdo.size << " byte" << endl;

                str1 << indent << "    " << hex;
                for (i = 0; i < min((uint32_t) sdo.size,
                            (uint32_t) EC_MAX_SDO_DATA_SIZE); i++) {
                    str1 << setw(2) << (unsigned int) sdo.data[i];
                    if ((i + 1) % 16 == 0 && i < sdo.size - 1) {
                        str1 << endl << indent << "    ";
                    } else {
                        str1 << " ";
                    }
                }

                str1 << endl;
                if (sdo.size > EC_MAX_SDO_DATA_SIZE) {
                    str1 << indent << "    ..." << endl;
                }
            }
        } else {
            str1 << indent << "  None." << endl;
        }

        str1 << indent << "IDN configuration:" << endl;
        if (configIter->idn_count) {
            for (j = 0; j < configIter->idn_count; j++) {
                retval = m.getConfigIdn(&idn, configIter->config_index, j);
                if(retval)
					return retval;

                str1 << indent << "  Drive " << (unsigned int) idn.drive_no
                    << ", " << outputIdn(idn.idn)
                    << ", " << dec << idn.size << " byte" << endl;

                str1 << indent << "    " << hex << setfill('0');
                for (i = 0; i < min((uint32_t) idn.size,
                            (uint32_t) EC_MAX_IDN_DATA_SIZE); i++) {
                    str1 << setw(2) << (unsigned int) idn.data[i];
                    if ((i + 1) % 16 == 0 && i < idn.size - 1) {
                        str1 << endl << indent << "    ";
                    } else {
                        str1 << " ";
                    }
                }

                str1 << endl;
                if (idn.size > EC_MAX_IDN_DATA_SIZE) {
                    str1 << indent << "    ..." << endl;
                }
            }
        } else {
            str1 << indent << "  None." << endl;
        }

        str1 << indent << "Feature flags:" << endl;
        if (configIter->flag_count) {
            for (j = 0; j < configIter->flag_count; j++) {
                retval = m.getConfigFlag(&flag, configIter->config_index, j);
                if(retval)
					return retval;

                str1 << indent << "  " << flag.key
                    << ": " << flag.value << endl;
            }
        } else {
            str1 << indent << "  None." << endl;
        }

#ifdef EC_EOE
        m.getIpParam(&ip, configIter->config_index);
        if (ip.mac_address_included or ip.ip_address_included or
                ip.subnet_mask_included or ip.gateway_included or
                ip.dns_included or ip.name_included) {
            char addr[32];
            str1 << indent << "EoE IP parameters:" << endl;
            if (ip.mac_address_included) {
                str1 << indent << "  MAC address: "
                    << hex << setfill('0') << setw(2)
                    << ip.mac_address[0] << ":"
                    << ip.mac_address[1] << ":"
                    << ip.mac_address[2] << ":"
                    << ip.mac_address[3] << ":"
                    << ip.mac_address[4] << ":"
                    << ip.mac_address[5] << dec << endl;
            }
            if (ip.ip_address_included) {
                inet_ntop(AF_INET, &ip.ip_address, addr, sizeof(addr));
                str1 << indent << "  IP address: " << addr << endl;
            }
            if (ip.subnet_mask_included) {
                inet_ntop(AF_INET, &ip.subnet_mask, addr, sizeof(addr));
                str1 << indent << "  Subnet mask: " << addr << endl;
            }
            if (ip.gateway_included) {
                inet_ntop(AF_INET, &ip.gateway, addr, sizeof(addr));
                str1 << indent << "  Default gateway: " << addr << endl;
            }
            if (ip.dns_included) {
                inet_ntop(AF_INET, &ip.dns, addr, sizeof(addr));
                str1 << indent << "  DNS server address: " << addr << endl;
            }
            if (ip.name_included) {
                str1 << indent << "  Hostname: " << ip.name << endl;
            }
        }
#endif

        if (configIter->dc_assign_activate) {
            int i;

            str1 << indent << "DC configuration:" << endl
                << indent << "  AssignActivate: 0x" << hex << setfill('0')
                << setw(4) << configIter->dc_assign_activate << endl;

            str1 << indent << "         Cycle [ns]   Shift [ns]" << endl;
            for (i = 0; i < EC_SYNC_SIGNAL_COUNT; i++) {
                str1 << indent << "  SYNC" << dec << i << "  "
                    << setfill(' ') << right
                    << setw(11) << configIter->dc_sync[i].cycle_time
                    << "  "
                    << setw(11) << configIter->dc_sync[i].shift_time
                    << endl;
            }
        }
        str1 << endl;
    }
    
    mycsout << str1.str();
}

/****************************************************************************/

/** Lists the bus configuration.
 */
int CommandConfig::listConfigs(
        MasterDevice &m,
        const ConfigList &configList,
        bool doIndent
        )
{
    ConfigList::const_iterator configIter;
    stringstream str;
    Info info;
    typedef list<Info> InfoList;
    InfoList list;
    InfoList::const_iterator iter;
    unsigned int maxAliasWidth = 0, maxPosWidth = 0,
                 maxSlavePosWidth = 0, maxStateWidth = 0;
    ec_ioctl_slave_t slave;
    string indent(doIndent ? "  " : "");
    
    int retval;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        str << dec << configIter->alias;
        info.alias = str.str();
        str.clear();
        str.str("");

        str << configIter->position;
        info.pos = str.str();
        str.clear();
        str.str("");

        str << hex << setfill('0')
            << "0x" << setw(8) << configIter->vendor_id
            << "/0x" << setw(8) << configIter->product_code;
        info.ident = str.str();
        str.clear();
        str.str("");

        if (configIter->slave_position != -1) {
            retval = m.getSlave(&slave, configIter->slave_position);
            if(retval)
					return retval;

            str << dec << configIter->slave_position;
            info.slavePos = str.str();
            str.clear();
            str.str("");

            #if 0 /*Winfred*/
            
            info.state = alStateString(slave.al_state);
            
            #endif
            
        } else {
            str << "-";
            info.slavePos = str.str();
            str.clear();
            str.str("");

            str << "-";
            info.state = str.str();
            str.clear();
            str.str("");
        }

        list.push_back(info);

        if (info.alias.length() > maxAliasWidth)
            maxAliasWidth = info.alias.length();
        if (info.pos.length() > maxPosWidth)
            maxPosWidth = info.pos.length();
        if (info.slavePos.length() > maxSlavePosWidth)
            maxSlavePosWidth = info.slavePos.length();
        if (info.state.length() > maxStateWidth)
            maxStateWidth = info.state.length();
    }

    for (iter = list.begin(); iter != list.end(); iter++) {
		
		stringstream str3;
		
        str3 << indent << setfill(' ') << right
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxPosWidth) << iter->pos
            << "  "
            << iter->ident
            << "  "
            << setw(maxSlavePosWidth) << iter->slavePos << "  "
            << setw(maxStateWidth) << iter->state << "  "
            << endl;
            
        mycsout << str3.str();    
    }
    
    pipe_printf("listConfigs success\n");
}

/****************************************************************************/
