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
 *  vim: expandtab
 *
 ****************************************************************************/

#include <iostream>

using namespace std;

#include "myostream.h"

#include "CommandData.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandData::CommandData():
    Command("data", "Output binary domain process data.")
{
}

/****************************************************************************/

string CommandData::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Data of multiple domains are concatenated." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --domain -d <index>  Positive numerical domain index." << endl
        << "                       If omitted, data of all domains" << endl
        << "                       are output." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandData::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
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

        for (di = domains.begin(); di != domains.end(); di++) {
            retval = outputDomainData(m, *di);
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

int CommandData::outputDomainData(
        MasterDevice &m,
        const ec_ioctl_domain_t &domain
        )
{
    ec_ioctl_domain_data_t data;
    unsigned char *processData;
    unsigned int i;
    
    int retval;

    if (!domain.data_size)
        return 0;

    processData = new unsigned char[domain.data_size];

    /*try {*/
    
        retval = m.getData(&data, domain.index, domain.data_size, processData);
    /*} catch (MasterDeviceException &e) */
		if(retval)
		{
			delete [] processData;
			
			return retval;
		}	
    /*    throw e;
    }*/
    
    stringstream str1;

    for (i = 0; i < data.data_size; i++)
        str1 << processData[i];
        
    /*cout.flush();*/
    
    mycsout << str1.str();

    delete [] processData;
    
    return 0;
}

/****************************************************************************/
