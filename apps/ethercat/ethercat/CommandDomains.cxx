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

#include "CommandDomains.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandDomains::CommandDomains():
    Command("domains", "Show configured domains.")
{
}

/****************************************************************************/

string CommandDomains::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Without the --verbose option, the domains are displayed" << endl
        << "one-per-line. Example:" << endl
        << endl
        << "Domain0: LogBaseAddr 0x00000000, Size   6, WorkingCounter 0/1"
        << endl << endl
        << "The domain's base address for the logical datagram" << endl
        << "(LRD/LWR/LRW) is displayed followed by the domain's" << endl
        << "process data size in byte. The last values are the current" << endl
        << "datagram working counter sum and the expected working" << endl
        << "counter sum. If the values are equal, all PDOs were" << endl
        << "exchanged during the last cycle." << endl
        << endl
        << "If the --verbose option is given, the participating slave" << endl
        << "configurations/FMMUs and the current process data are" << endl
        << "additionally displayed:" << endl
        << endl
        << "Domain1: LogBaseAddr 0x00000006, Size   6, WorkingCounter 0/1"
        << endl
        << "  SlaveConfig 1001:0, SM3 ( Input), LogAddr 0x00000006, Size 6"
        << endl
        << "    00 00 00 00 00 00" << endl
        << endl
        << "The process data are displayed as hexadecimal bytes." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --domain  -d <index>  Positive numerical domain index." << endl
        << "                        If omitted, all domains are" << endl
        << "                        displayed." << endl
        << endl
        << "  --verbose -v          Show FMMUs and process data" << endl
        << "                        in addition." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

extern "C" 
{
	extern int linux_printf(const char *fmt, ...);
}	

int CommandDomains::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    bool doIndent;
    DomainList domains;
    DomainList::const_iterator di;
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
        ec_ioctl_master_t io;
        MasterDevice m(*mi);
        
        is_master_empty = 0;
         
        retval = m.open(MasterDevice::Read);
        if(retval)
			return retval;
			
        retval = m.getMaster(&io);
        if(retval)
			return retval;
			
        domains = selectedDomains(m, io);

        if (domains.size() && doIndent) {
			stringstream str;
			str << "Master" << dec << *mi << '\n';
            mycsout << str.str();
        }

        for (di = domains.begin(); di != domains.end(); di++) {
            retval = showDomain(m, io, *di, doIndent);
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

/****************************************************************************/

int CommandDomains::showDomain(
        MasterDevice &m,
        const ec_ioctl_master_t &master,
        const ec_ioctl_domain_t &domain,
        bool doIndent
        )
{
    unsigned char *processData;
    ec_ioctl_domain_data_t data;
    unsigned int i, j;
    ec_ioctl_domain_fmmu_t fmmu;
    unsigned int dataOffset;
    string indent(doIndent ? "  " : "");
    unsigned int wc_sum = 0, dev_idx;
    
    int retval;

    #if 0
    
    for (dev_idx = EC_DEVICE_MAIN; dev_idx < master.num_devices; dev_idx++) {
        wc_sum += domain.working_counter[dev_idx];
    }
    
    stringstream str;

    str << indent << "Domain" << dec << domain.index << ":"
        << " LogBaseAddr 0x"
        << hex << setfill('0')
        << setw(8) << domain.logical_base_address
        << ", Size " << dec << setfill(' ')
        << setw(3) << domain.data_size
        << ", WorkingCounter "
        << wc_sum << "/"
        << domain.expected_working_counter;
    
    mycsout << str.str();    
        
    if (master.num_devices > 1) {
        mycsout << " (";
        for (dev_idx = EC_DEVICE_MAIN; dev_idx < master.num_devices;
                dev_idx++) {
            mycsout << domain.working_counter[dev_idx];
            if (dev_idx + 1 < master.num_devices) {
                mycsout << "+";
            }
        }
        mycsout << ")";
    }
    mycsout << '\n';

    if (!domain.data_size || getVerbosity() != Verbose)
        return 0;

    processData = new unsigned char[domain.data_size];

    /* try {*/
        retval = m.getData(&data, domain.index, domain.data_size, processData);
        if(retval)
        {
    
			delete [] processData;
			
			return retval;
		}	
        /*throw e;
    }*/

    for (i = 0; i < domain.fmmu_count; i++) {
        retval = m.getFmmu(&fmmu, domain.index, i);
        if(retval)
			return retval;

		stringstream str2;
		
        str2 << indent << "  SlaveConfig "
            << dec << fmmu.slave_config_alias
            << ":" << fmmu.slave_config_position
            << ", SM" << (unsigned int) fmmu.sync_index << " ("
            << setfill(' ') << setw(6)
            << (fmmu.dir == EC_DIR_INPUT ? "Input" : "Output")
            << "), LogAddr 0x"
            << hex << setfill('0')
            << setw(8) << fmmu.logical_address
            << ", Size " << dec << fmmu.data_size << endl;
            
        mycsout << str2.str();    

        dataOffset = fmmu.logical_address - domain.logical_base_address;
        if (dataOffset + fmmu.data_size > domain.data_size) {
            stringstream err;
            
            delete [] processData;
            
            g_err.str("");
            
            g_err << "Fmmu information corrupted!";
            
            /*(throwCommandException(err);*/
            
            return -2;
        }
        
        stringstream str1;

        str1 << indent << "    " << hex << setfill('0');
        for (j = 0; j < fmmu.data_size; j++) {
            if (j && !(j % BreakAfterBytes))
                str1 << endl << indent << "    ";
            str1 << setw(2)
                << (unsigned int) *(processData + dataOffset + j) << " ";
        }
        str1 << endl;
        
        mycsout << str1.str(); 
    }

    delete [] processData;
    
    #endif
    
    return 0;
}

/****************************************************************************/
