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
#include <cstring>

using namespace std;

#include "myostream.h"

#include "CommandPdos.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandPdos::CommandPdos():
    Command("pdos", "List Sync managers, PDO assignment and mapping.")
{
}

/****************************************************************************/

string CommandPdos::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "For the default skin (see --skin option) the information" << endl
        << "is displayed in three layers, which are" << endl
        << "indented accordingly:" << endl
        << endl
        << "1) Sync managers - Contains the sync manager information" << endl
        << "   from the SII: Index, physical start address, default" << endl
        << "   size, control register and enable word. Example:" << endl
        << endl
        << "   SM3: PhysAddr 0x1100, DefaultSize 0, ControlRegister 0x20, "
        << "Enable 1" << endl
        << endl
        << "2) Assigned PDOs - PDO direction, hexadecimal index and" << endl
        << "   the PDO name, if available. Note that a 'Tx' and 'Rx'" << endl
        << "   are seen from the slave's point of view. Example:" << endl
        << endl
        << "   TxPDO 0x1a00 \"Channel1\"" << endl
        << endl
        << "3) Mapped PDO entries - PDO entry index and subindex (both" << endl
        << "   hexadecimal), the length in bit and the description, if" << endl
        << "   available. Example:" << endl
        << endl
        << "   PDO entry 0x3101:01, 8 bit, \"Status\"" << endl
        << endl
        << "Note, that the displayed PDO assignment and PDO mapping" << endl
        << "information can either originate from the SII or from the" << endl
        << "CoE communication area." << endl
        << endl
        << "The \"etherlab\" skin outputs a template configuration" << endl
        << "for EtherLab's generic EtherCAT slave block." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --skin     -s <skin>   Choose output skin. Possible values are"
        << endl
        << "                         \"default\" and \"etherlab\"." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandPdos::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    SlaveList slaves;
    SlaveList::const_iterator si;
    bool showHeader, multiMaster;
    
    int retval;
    
    int is_master_empty;
    

    if (args.size()) {
        stringstream err;
        
        g_err.str("");
        
        g_err << "'" << getName() << "' takes no arguments!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    
    is_master_empty = 1;
    
    /*if (getSkin().empty() || getSkin() == "default")*/
    if(1) {
        masterIndices = getMasterIndices();
        multiMaster = masterIndices.size() > 1;
        MasterIndexList::const_iterator mi;
        for (mi = masterIndices.begin();
                mi != masterIndices.end(); mi++) {
            MasterDevice m(*mi);
            
            is_master_empty = 0;
            
            retval = m.open(MasterDevice::Read);
            if(retval)
				return retval;
				
            slaves = selectedSlaves(m);
            showHeader = multiMaster || slaves.size() > 1;

            for (si = slaves.begin(); si != slaves.end(); si++) {
                retval = listSlavePdos(m, *si, showHeader);
                if(retval)
					return retval;
            }
        }
    }
    else if (getSkin() == "etherlab") {
        masterIndices = getMasterIndices();
        MasterIndexList::const_iterator mi;
        for (mi = masterIndices.begin();
                mi != masterIndices.end(); mi++) {
            MasterDevice m(*mi);
            
            is_master_empty = 0;
            
            retval = m.open(MasterDevice::Read);
            if(retval)
				return retval;
            
            slaves = selectedSlaves(m);

            for (si = slaves.begin(); si != slaves.end(); si++) {
                retval = etherlabConfig(m, *si);
                if(retval)
					return retval;
            }
        }
    }
    else {
        stringstream err;
        
        g_err.str("");
        
        g_err << "Invalid skin '" << getSkin() << "'!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
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

int CommandPdos::listSlavePdos(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave,
        bool showHeader
        )
{
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;
    
    stringstream str1;
    
    int retval;
    

    #if 0 /*Winfred*/
      
    if (showHeader && slave.sync_count)
        str1 << "=== Master " << m.getIndex()
            << ", Slave " << slave.position << " ===" << endl;
    
    for (i = 0; i < slave.sync_count; i++) {
        retval = m.getSync(&sync, slave.position, i);
        if(retval)
			return retval;

        str1 << "SM" << i << ":"
            << " PhysAddr 0x"
            << hex << setfill('0')
            << setw(4) << sync.physical_start_address
            << ", DefaultSize "
            << dec << setfill(' ') << setw(4) << sync.default_size
            << ", ControlRegister 0x"
            << hex << setfill('0') << setw(2)
            << (unsigned int) sync.control_register
            << ", Enable " << dec << (unsigned int) sync.enable
            << endl;

        for (j = 0; j < sync.pdo_count; j++) {
            retval = m.getPdo(&pdo, slave.position, i, j);
            if(retval)
				return retval;

            str1 << "  " << (sync.control_register & 0x04 ? "R" : "T")
                << "xPDO 0x"
                << hex << setfill('0')
                << setw(4) << pdo.index
                << " \"" << pdo.name << "\"" << endl;

            if (getVerbosity() == Quiet)
                continue;

            for (k = 0; k < pdo.entry_count; k++) {
                retval = m.getPdoEntry(&entry, slave.position, i, j, k);
                if(retval)
					return retval;

                str1 << "    PDO entry 0x"
                    << hex << setfill('0')
                    << setw(4) << entry.index
                    << ":" << setw(2) << (unsigned int) entry.subindex
                    << ", " << dec << setfill(' ')
                    << setw(2) << (unsigned int) entry.bit_length
                    << " bit, \"" << entry.name << "\"" << endl;
            }
        }
    }
    
    #else
    
		retval = m.getPdo(&pdo, slave.position, i, j);
        if(retval)
			return retval;
    
    #endif
    
    mycsout << str1.str();
    
    return 0;
}

/****************************************************************************/

int CommandPdos::etherlabConfig(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave
        )
{
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;
    
    stringstream str2;
    
    int retval;
    
    #if 0 /*Winfred*/    

    str2 << "%" << endl
        << "% Master " << m.getIndex() << ", Slave " << slave.position;
    if (strlen(slave.order)) {
        str2 << ", \"" << slave.order << "\"";
    }
    str2 << endl
        << "%" << endl
        << "function rv = slave" << slave.position << "()" << endl << endl;

    str2 << "% Slave configuration" << endl << endl;

    str2 << "rv.SlaveConfig.vendor = " << slave.vendor_id << ";" << endl
        << "rv.SlaveConfig.product = hex2dec('"
        << hex << setfill('0') << setw(8) << slave.product_code << "');"
        << endl
        << "rv.SlaveConfig.description = '" << slave.order << "';" << endl
        << "rv.SlaveConfig.sm = { ..." << endl;

    /* slave configuration */
    for (i = 0; i < slave.sync_count; i++) {
        retval = m.getSync(&sync, slave.position, i);
        if(retval)
				return retval;

        str2 << "    {" << dec << i << ", "
            << (sync.control_register & 0x04 ? 0 : 1) << ", {" << endl;

        for (j = 0; j < sync.pdo_count; j++) {
            retval = m.getPdo(&pdo, slave.position, i, j);
            if(retval)
				return retval;

            str2 << "        {hex2dec('" <<
                hex << setfill('0') << setw(4) << pdo.index << "'), ["
                << endl;

            for (k = 0; k < pdo.entry_count; k++) {
                retval = m.getPdoEntry(&entry, slave.position, i, j, k);
                if(retval)
					return retval;

                str2 << "            hex2dec('"
                    << hex << setfill('0') << setw(4) << entry.index
                    << "'), hex2dec('"
                    << setw(2) << (unsigned int) entry.subindex
                    << "'), " << dec << setfill(' ') << setw(3)
                    << (unsigned int) entry.bit_length
                    << "; ..." << endl;
            }

            str2 << "            ]}, ..." << endl;
        }
        str2 << "        }}, ..." << endl;
    }

    str2 << "    };" << endl << endl;

    str2 << "% Port configuration" << endl << endl;

    unsigned int input = 1, output = 1;
    for (i = 0; i < slave.sync_count; i++) {
        retval = m.getSync(&sync, slave.position, i);
        if(retval)
				return retval;

        for (j = 0; j < sync.pdo_count; j++) {
            retval = m.getPdo(&pdo, slave.position, i, j);
            if(retval)
				return retval;

            for (k = 0; k < pdo.entry_count; k++) {
                retval = m.getPdoEntry(&entry, slave.position, i, j, k);
                if(retval)
					return retval;

                if (!entry.index) {
                    continue;
                }

                string dir;
                unsigned int port;

                if (sync.control_register & 0x04) {
                    dir = "input";
                    port = input++;
                }
                else {
                    dir = "output";
                    port = output++;
                }

                stringstream var;
                var << "rv.PortConfig." << dir << "(" << dec << port
                    << ")";

                str2 << var.str() << ".pdo = ["
                    << i << ", " << j << ", " << k << ", 0];" << endl;
                str2 << var.str() << ".pdo_data_type = "
                    << 1000 + (unsigned int) entry.bit_length
                    << ";" << endl << endl;
            }
        }
    }
    
    #endif

    str2 << "end" << endl;
    
    mycsout << str2.str();
    
    return 0;
}

/****************************************************************************/
