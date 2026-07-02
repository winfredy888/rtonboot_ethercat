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

#include <string.h>

#include <iostream>
#include <iomanip>

using namespace std;

#include "myostream.h"

#include "CommandFoeRead.h"
#include "foe.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandFoeRead::CommandFoeRead():
    FoeCommand("foe_read", "Read a file from a slave via FoE.")
{
}

/****************************************************************************/

string CommandFoeRead::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <SOURCEFILE>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  SOURCEFILE is the name of the source file on the slave." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --output-file -o <file>   Local target filename. If" << endl
        << "                            '-' (default), data are" << endl
        << "                            printed to stdout." << endl
        << "  --alias       -a <alias>  " << endl
        << "  --position    -p <pos>    Slave selection. See the help" << endl
        << "                            of the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

extern "C" 
{	
	extern uint32_t g_foe_read_file_size;
	
	extern uint8_t * get_bulk_ptr(uint32_t offset);
	
	extern void prepare_foe_read_and_wake_up(void);
}	

string getFileName(string fullPath) {
    size_t pos = fullPath.find_last_of("/\\");
    if (pos == std::string::npos) {
        return fullPath;
    }
    
    return fullPath.substr(pos + 1);
}

int CommandFoeRead::execute(const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_t *slave;
    ec_ioctl_slave_foe_t data;
    unsigned int i;
    stringstream err;
    string baseFileName;
    
    int retval;

    if (args.size() != 1) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes exactly one argument!";
        
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
		
        g_err << "throwSingleSlaveRequired!";
        
        return -1;
    }
    
    slave = &slaves.front();
    data.slave_position = slave->position;

    /* FIXME: No good idea to have a fixed buffer size.
     * Read in chunks and fill a buffer instead.
     */
    data.offset = 0;
    /*data.buffer_size = 0x8800;
    data.buffer = new uint8_t[data.buffer_size];*/
    
    data.buffer_size = 2 * 1024 * 1024;
    data.buffer = (uint8_t *)(get_bulk_ptr(0));
    
    baseFileName = getFileName(args[0]);

    strncpy(data.file_name, baseFileName.c_str(), sizeof(data.file_name) - 1);

    /*try {*/
        retval = m.readFoe(&data);
        if(retval)
        {
			/*delete [] data.buffer;*/
			
			return -3;
		}	
		
    /*} catch (MasterDeviceException &e) {
        delete [] data.buffer;
        if (data.result) {
            if (data.result == FOE_OPCODE_ERROR) {
                err << "FoE read aborted with error code 0x"
                    << setw(8) << setfill('0') << hex << data.error_code
                    << ": " << errorText(data.error_code);
            } else {
                err << "Failed to write via FoE: "
                    << resultText(data.result);
            }
            throwCommandException(err);
        } else {
            throw e;
        }
    }*/

    // TODO --output-file
    /*for (i = 0; i < data.data_size; i++) {
        uint8_t *w = data.buffer + i;
        mycsout << *(uint8_t *) w ;
    }*/
    
    g_foe_read_file_size = data.data_size;
	
	/*memcpy( get_bulk_ptr(0), data.buffer, g_foe_read_file_size);

    delete [] data.buffer;*/
    
    prepare_foe_read_and_wake_up();
    
    return 0x88;
}

/****************************************************************************/
