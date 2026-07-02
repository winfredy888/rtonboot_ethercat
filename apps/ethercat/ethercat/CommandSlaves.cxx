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
#include <list>
#include <string.h>

using namespace std;

#include "myostream.h"

#include "CommandSlaves.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandSlaves::CommandSlaves():
    Command("slaves", "Display slaves on the bus.")
{
}

/****************************************************************************/

string CommandSlaves::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "If the --verbose option is not given, the slaves are" << endl
        << "displayed one-per-line. Example:" << endl
        << endl
        << "1  5555:0  PREOP  +  EL3162 2C. Ana. Input 0-10V" << endl
        << "|  |    |  |      |  |" << endl
        << "|  |    |  |      |  \\- Name from the SII if available," << endl
        << "|  |    |  |      |     otherwise vendor ID and product" << endl
        << "|  |    |  |      |     code (both hexadecimal)." << endl
        << "|  |    |  |      \\- Error flag. '+' means no error," << endl
        << "|  |    |  |         'E' means that scan or" << endl
        << "|  |    |  |         configuration failed." << endl
        << "|  |    |  \\- Current application-layer state." << endl
        << "|  |    \\- Decimal relative position to the last" << endl
        << "|  |       slave with an alias address set." << endl
        << "|  \\- Decimal alias address of this slave (if set)," << endl
        << "|     otherwise of the last slave with an alias set," << endl
        << "|     or zero, if no alias was encountered up to this" << endl
        << "|     position." << endl
        << "\\- Absolute ring position in the bus." << endl
        << endl
        << "If the --verbose option is given, a detailed (multi-line)" << endl
        << "description is output for each slave." << endl
        << endl
        << "Slave selection:" << endl
        << "  Slaves for this and other commands can be selected with" << endl
        << "  the --alias and --position parameters as follows:" << endl
        << endl
        << "  1) If neither the --alias nor the --position option" << endl
        << "     is given, all slaves are selected." << endl
        << "  2) If only the --position option is given, it is" << endl
        << "     interpreted as an absolute ring position and" << endl
        << "     a slave with this position is matched." << endl
        << "  3) If only the --alias option is given, all slaves" << endl
        << "     with the given alias address and subsequent" << endl
        << "     slaves before a slave with a different alias" << endl
        << "     address match (use -p0 if only the slaves" << endl
        << "     with the given alias are desired, see 4))." << endl
        << "  4) If both the --alias and the --position option are" << endl
        << "     given, the latter is interpreted as relative" << endl
        << "     position behind any slave with the given alias." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>  Slave alias (see above)." << endl
        << "  --position -p <pos>    Slave position (see above)." << endl
        << "  --verbose  -v          Show detailed slave information." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandSlaves::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    SlaveList slaves;
    bool doIndent;
    int retval;
    int is_master_empty;
    ec_ioctl_master_t master;

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
					
        /*slaves = selectedSlaves(m);*/	

        if (getVerbosity() == Verbose) {
            retval = showSlaves(m, slaves);
            if(retval)
				return retval;
        } else {	
            /*retval = listSlaves(m, slaves, doIndent);*/
            retval = listSlaves(m, doIndent);            
            if(retval)
				return retval;
        }
    }
    
    if(is_master_empty)
    {
		g_err.str("");
		
        g_err << "getMasterIndices parse error do nothing";
        
        return -3;
	}	
    
    return 0;
}

ec_ioctl_master_t master;

/****************************************************************************/

int CommandSlaves::listSlaves(
        MasterDevice &m,
        /*const SlaveList &slaves,*/
        bool doIndent
        )
{
    unsigned int i, lastDevice;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    Info info;
    typedef list<Info> InfoList;
    InfoList infoList;
    InfoList::const_iterator iter;
    stringstream str;
    unsigned int maxPosWidth = 0, maxAliasWidth = 0,
                 maxRelPosWidth = 0, maxStateWidth = 0;
    string indent(doIndent ? "  " : "");
    
    int retval;
    
    retval = m.getMaster(&master);
    if(retval)
		return retval;	
	
    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < master.slave_count; i++) {
        retval = m.getSlave(&slave, i);
		if(retval)
			return retval;
		
        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }
        
        /*if (slaveInList(slave, slaves))*/
        if(1) {
            str << dec << i;
            info.pos = str.str();
            str.clear();
            str.str("");
            
            str << dec << lastAlias;
            info.alias = str.str();
            str.clear();
            str.str("");
            
            str << aliasIndex;
            info.relPos = str.str();
            str.str("");
            
            /*info.state = alStateString(slave.al_state);
            info.flag = (slave.error_flag ? 'E' : '+');
            info.device = slave.device_index;*/

            if (strlen(slave.name)) {
                info.name = slave.name;
            } else {
                str << "0x" << hex << setfill('0')
                    << setw(8) << slave.vendor_id << ":0x"
                    << setw(8) << slave.product_code;
                info.name = str.str();
                str.str("");
            }


            infoList.push_back(info);

            if (info.pos.length() > maxPosWidth)
                maxPosWidth = info.pos.length();
            if (info.alias.length() > maxAliasWidth)
                maxAliasWidth = info.alias.length();
            if (info.relPos.length() > maxRelPosWidth)
                maxRelPosWidth = info.relPos.length();
            if (info.state.length() > maxStateWidth)
                maxStateWidth = info.state.length();
                    
        }

        aliasIndex++;
    }
    
    stringstream str1;

    if (infoList.size() && doIndent) {
        str1 << "Master" << dec << m.getIndex() << '\n';
    }
    
    mycsout << str1.str(); 
    
    lastDevice = EC_DEVICE_MAIN;
    for (iter = infoList.begin(); iter != infoList.end(); iter++) {
		
		stringstream str2;
		
        /*if (iter->device != lastDevice) {
            lastDevice = iter->device;
            str2 << "xxx LINK FAILURE xxx" << endl;
        }*/
               
        str2 << indent << setfill(' ') << right
            << setw(maxPosWidth) << iter->pos << "  "
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxRelPosWidth) << iter->relPos << "  "
            << iter->name << endl;
            /*<< setw(maxStateWidth) << iter->state << "  "
            << iter->flag << "  " */
            
            
         mycsout << str2.str(); 
    }
    
    return 0;
}

/****************************************************************************/

int CommandSlaves::showSlaves(
        MasterDevice &m,
        const SlaveList &slaves
        )
{
    /*SlaveList::const_iterator si;*/
    int i;
    int retval;
    ec_ioctl_slave_t slave;
    stringstream str3;
    
    retval = m.getMaster(&master);
    if(retval)
		return retval;
    
    /*for (si = slaves.begin(); si != slaves.end(); si++) {*/
    for (i = 0; i < master.slave_count; i++) {
		
		retval = m.getSlave(&slave, i);
		if(retval)
			return retval;
			
        str3 << "=== Master " << dec << m.getIndex()
            << ", Slave " << dec << slave.position << " ===" << endl;

        if (slave.alias)
            str3 << "Alias: " << slave.alias << endl;

        str3
            /*<< "Device: " << (si->device_index ? "Backup" : "Main") << endl
            << "State: " << alStateString(si->al_state) << endl
            << "Flag: " << (si->error_flag ? 'E' : '+') << endl*/
            << "Identity:" << endl
            << "  Vendor Id:       0x"
            << hex << setfill('0')
            << setw(8) << slave.vendor_id << endl
            << "  Product code:    0x"
            << setw(8) << slave.product_code << endl
            << "  Revision number: 0x"
            << setw(8) << slave.revision_number << endl
            << "  Serial number:   0x"
            << setw(8) << slave.serial_number << endl;

        /*str3 << "DL information:" << endl
            << "  FMMU bit operation: "
            << (si->fmmu_bit ? "yes" : "no") << endl
            << "  Distributed clocks: ";
        if (si->dc_supported) {
            if (si->has_dc_system_time) {
                str3 << "yes, ";
                switch (si->dc_range) {
                    case EC_DC_32:
                        str3 << "32 bit";
                        break;
                    case EC_DC_64:
                        str3 << "64 bit";
                        break;
                    default:
                        str3 << "???";
                }
                str3 << endl;
            } else {
                str3 << "yes, delay measurement only" << endl;
            }
            str3 << "  DC system time transmission delay: "
                << dec << si->transmission_delay << " ns" << endl;
        } else {
            str3 << "no" << endl;
        }*/

        /*str3 << "Port  Type  Link  Loop    Signal  NextSlave";
        if (slave.dc_supported)
            str3 << "  RxTime [ns]  Diff [ns]   NextDc [ns]";
        str3 << endl;*/

        /*for (i = 0; i < EC_MAX_PORTS; i++) {
            str3 << "   " << i << "  " << setfill(' ') << left << setw(4);
            switch (si->ports[i].desc) {
                case EC_PORT_NOT_IMPLEMENTED:
                    str3 << "N/A";
                    break;
                case EC_PORT_NOT_CONFIGURED:
                    str3 << "N/C";
                    break;
                case EC_PORT_EBUS:
                    str3 << "EBUS";
                    break;
                case EC_PORT_MII:
                    str3 << "MII";
                    break;
                default:
                    str3 << "???";
            }

            str3 << "  " << setw(4)
                << (si->ports[i].link.link_up ? "up" : "down")
                << "  " << setw(6)
                << (si->ports[i].link.loop_closed ? "closed" : "open")
                << "  " << setw(6)
                << (si->ports[i].link.signal_detected ? "yes" : "no")
                << "  " << setw(9) << right;

            if (si->ports[i].next_slave != 0xffff) {
                str3 << dec << si->ports[i].next_slave;
            } else {
                str3 << "-";
            }

            if (si->dc_supported) {
                str3 << "  " << setw(11) << right;
                if (!si->ports[i].link.loop_closed) {
                    str3 << dec << si->ports[i].receive_time;
                } else {
                    str3 << "-";
                }
                str3 << "  " << setw(10);
                if (!si->ports[i].link.loop_closed) {
                    str3 << si->ports[i].receive_time -
                        si->ports[0].receive_time;
                } else {
                    str3 << "-";
                }
                str3 << "  " << setw(10);
                if (!si->ports[i].link.loop_closed) {
                    str3 << si->ports[i].delay_to_next_dc;
                } else {
                    str3 << "-";
                }
            }

            str3 << endl;
        }*/

        if (slave.mailbox_protocols) {
            list<string> protoList;
            list<string>::const_iterator protoIter;

            str3 << "Mailboxes:" << endl
                << "  Bootstrap RX: 0x" << setfill('0')
                << hex << setw(4) << slave.boot_rx_mailbox_offset << "/"
                << dec << slave.boot_rx_mailbox_size
                << ", TX: 0x"
                << hex << setw(4) << slave.boot_tx_mailbox_offset << "/"
                << dec << slave.boot_tx_mailbox_size << endl
                /*<< "  Standard  RX: 0x"
                << hex << setw(4) << si->std_rx_mailbox_offset << "/"
                << dec << si->std_rx_mailbox_size
                << ", TX: 0x"
                << hex << setw(4) << si->std_tx_mailbox_offset << "/"
                << dec << si->std_tx_mailbox_size << endl*/
                << "  Supported protocols: ";

            if (slave.mailbox_protocols & EC_MBOX_AOE) {
                protoList.push_back("AoE");
            }
            if (slave.mailbox_protocols & EC_MBOX_EOE) {
                protoList.push_back("EoE");
            }
            if (slave.mailbox_protocols & EC_MBOX_COE) {
                protoList.push_back("CoE");
            }
            if (slave.mailbox_protocols & EC_MBOX_FOE) {
                protoList.push_back("FoE");
            }
            if (slave.mailbox_protocols & EC_MBOX_SOE) {
                protoList.push_back("SoE");
            }
            if (slave.mailbox_protocols & EC_MBOX_VOE) {
                protoList.push_back("VoE");
            }

            for (protoIter = protoList.begin(); protoIter != protoList.end();
                    protoIter++) {
                if (protoIter != protoList.begin())
                    str3 << ", ";
                str3 << *protoIter;
            }
            str3 << endl;
        }

        /*if (si->has_general_category) {
            str3 << "General:" << endl
                << "  Group: " << si->group << endl
                << "  Image name: " << si->image << endl
                << "  Order number: " << si->order << endl
                << "  Device name: " << si->name << endl;

            if (si->mailbox_protocols & EC_MBOX_COE) {
                str3 << "  CoE details:" << endl
                    << "    Enable SDO: "
                    << (si->coe_details.enable_sdo ? "yes" : "no") << endl
                    << "    Enable SDO Info: "
                    << (si->coe_details.enable_sdo_info ? "yes" : "no")
                    << endl
                    << "    Enable PDO Assign: "
                    << (si->coe_details.enable_pdo_assign
                            ? "yes" : "no") << endl
                    << "    Enable PDO Configuration: "
                    << (si->coe_details.enable_pdo_configuration
                            ? "yes" : "no") << endl
                    << "    Enable Upload at startup: "
                    << (si->coe_details.enable_upload_at_startup
                            ? "yes" : "no") << endl
                    << "    Enable SDO complete access: "
                    << (si->coe_details.enable_sdo_complete_access
                            ? "yes" : "no") << endl;
            }

            str3 << "  Flags:" << endl
                << "    Enable SafeOp: "
                << (si->general_flags.enable_safeop ? "yes" : "no") << endl
                << "    Enable notLRW: "
                << (si->general_flags.enable_not_lrw ? "yes" : "no") << endl
                << "  Current consumption: "
                << dec << si->current_on_ebus << " mA" << endl;
        }*/
    }
    
     mycsout << str3.str();
     
     return 0;
}

/****************************************************************************/

bool CommandSlaves::slaveInList(
        const ec_ioctl_slave_t &slave,
        const SlaveList &slaves
        )
{
    SlaveList::const_iterator si;

    for (si = slaves.begin(); si != slaves.end(); si++) {
        if (si->position == slave.position) {
            return true;
        }
    }

    return false;
}

/****************************************************************************/
