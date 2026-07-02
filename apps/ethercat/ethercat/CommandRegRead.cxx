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
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "myostream.h"

#include "CommandRegRead.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandRegRead::CommandRegRead():
    Command("reg_read", "Output a slave's register contents.")
{
}

/****************************************************************************/

string CommandRegRead::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <ADDRESS> [SIZE]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  ADDRESS is the register address. Must" << endl
        << "          be an unsigned 16 bit number." << endl
        << "  SIZE    is the number of bytes to read and must also be" << endl
        << "          an unsigned 16 bit number. ADDRESS plus SIZE" << endl
        << "          may not exceed 64k. The size is ignored (and" << endl
        << "          can be omitted), if a selected data type" << endl
        << "          implies a size." << endl
        << endl
        << typeInfo()
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --type     -t <type>   Data type (see above)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandRegRead::execute(const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_reg_t io;
    stringstream strOffset, err;
    const DataType *dataType = NULL;
    
    int retval;

    if (args.size() < 1 || args.size() > 2) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes one or two arguments!";
        
        /*throwInvalidUsageException(err);*/
        
		return -1;
    }

    strOffset << args[0];
    strOffset
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> io.address;
    if (strOffset.fail()) {
		
		g_err.str("");
		
        g_err << "Invalid address '" << args[0] << "'!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    if (args.size() > 1) {
        stringstream strLength;
        strLength << args[1];
        strLength
            >> resetiosflags(ios::basefield) // guess base from prefix
            >> io.size;
        if (strLength.fail()) {
			
			g_err.str("");
			
            g_err << "Invalid size '" << args[1] << "'!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }

        if (!io.size) {
			
			g_err.str("");
			
            g_err << "Length may not be zero!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }
    } else { // no size argument given
        io.size = 0;
    }

    if (!getDataType().empty()) {
        if (!(dataType = findDataType(getDataType()))) {
			
			g_err.str("");
			
            g_err << "Invalid data type '" << getDataType() << "'!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }

        if (dataType->byteSize) {
            // override size argument
            io.size = dataType->byteSize;
        }
    }

    if (!io.size) {
		
		g_err.str("");
		
        g_err << "The size argument is mandatory, if no datatype is " << endl
            << "specified, or the datatype does not imply a size!";
            
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    if ((uint32_t) io.address + io.size > 0xffff) {
		
		g_err.str("");
		
        g_err << "Address and size exceeding 64k!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    
    int32_t idx;
    
    idx = (int32_t)(getSingleMasterIndex());
    if(idx < 0)
    {
		g_err.str("");
        
        g_err << "getSingleMasterIndex parse error do nothing";
        
        return -3;
	}

    MasterDevice m(idx);
    
    retval = m.open(MasterDevice::ReadWrite);
    if(retval)
		return retval;
		
    slaves = selectedSlaves(m);

    if (slaves.size() != 1) {
        /*throwSingleSlaveRequired(slaves.size());*/
        
        g_err.str("");
		
        g_err << "throwSingleSlaveRequired";
        
        return -1;
    }
    
    io.slave_position = slaves.front().position;
    io.emergency = false;

    io.data = new uint8_t[io.size];

    /*try {*/
        retval = m.readReg(&io);
        if(retval)
        {
			delete [] io.data;
			
			return retval;
		}	 
		
    /*} catch (MasterDeviceException &e) {
        delete [] io.data;
        throw e;
    }*/

    /*try {*/
        retval = outputData(cout, dataType, io.data, io.size);
        if(retval)
        {
			delete [] io.data;
			
			return retval;
		}	
    /*} catch (SizeException &e) {
        delete [] io.data;
        throwCommandException(e.what());
    }*/

    delete [] io.data;
    
    return 0;
}

/****************************************************************************/
