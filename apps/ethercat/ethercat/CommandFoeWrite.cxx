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

/*#include <libgen.h>*/ // basename()
#include <string.h>

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace std;

#include "myostream.h"

#include "CommandFoeWrite.h"
#include "foe.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandFoeWrite::CommandFoeWrite():
    FoeCommand("foe_write", "Store a file on a slave via FoE.")
{
}

/****************************************************************************/

string CommandFoeWrite::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <FILENAME>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  FILENAME can either be a path to a file, or '-'. In" << endl
        << "           the latter case, data are read from stdin and" << endl
        << "           the --output-file option has to be specified." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --output-file -o <file>   Target filename on the slave." << endl
        << "                            If the FILENAME argument is" << endl
        << "                            '-', this is mandatory." << endl
        << "                            Otherwise, the basename() of" << endl
        << "                            FILENAME is used by default." << endl
        << "  --alias       -a <alias>" << endl
        << "  --position    -p <pos>    Slave selection. See the help" << endl
        << "                            of the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

extern "C" 
{
	sem_t g_foe_write_sem;
	
	extern uint32_t g_foe_write_file_size;
	
	extern uint8_t * get_bulk_ptr(uint32_t offset);
}	

std::string getBaseFileName(const char* fullPath) {
    if (!fullPath) return "";
    
    const char* baseName = std::strrchr(fullPath, '/'); 
    
    return baseName ? std::string(baseName + 1) : std::string(fullPath);
}

int CommandFoeWrite::execute(const StringVector &args)
{
    stringstream err;
    ec_ioctl_slave_foe_t data;
    ifstream file;
    SlaveList slaves;
    string storeFileName;
    int retval;

    if (args.size() != 1) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes exactly one argument!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

	#if 0
	
    if (args[0] == "-") {
        loadFoeData(&data, cin);
        if (getOutputFile().empty()) {
			
			g_err.str("");
			
            g_err << "Please specify a filename for the slave side"
                << " with --output-file!";
                
            /*throwCommandException(err);*/
            
            return -2;
        } else {
            storeFileName = getOutputFile();
        }
    } 
    else     
    {
        file.open(args[0].c_str(), ifstream::in | ifstream::binary);
        if (file.fail()) {
			
			g_err.str("");
			
            g_err << "Failed to open '" << args[0] << "'!";
            
            /*throwCommandException(err);*/
            
            return -2;
        }
        loadFoeData(&data, file);
        file.close();
        if (getOutputFile().empty()) {
            char *cpy = strdup(args[0].c_str()); // basename can modify
                                                 // the string contents
            storeFileName = basename(cpy);
            free(cpy);
        } else {
            storeFileName = getOutputFile();
        }
    }
    
    #endif
    
    while (sem_wait(&g_foe_write_sem) < 0)
    {
		  ++retval;
		  
		  --retval;
	};
	
	char *cpy = strdup(args[0].c_str());
                                                 
    storeFileName = getBaseFileName(cpy);
    
    free(cpy);
    
    int32_t idx;
    
    idx = (int32_t)(getSingleMasterIndex());
    if(idx < 0)
    {
		g_err.str("");
        
        g_err << "getSingleMasterIndex parse error do nothing";
        
        return -3;
	}

    MasterDevice m(idx);
    
    /*try {*/
        retval = m.open(MasterDevice::ReadWrite);
        if(retval)
        {
			/*if (data.buffer_size)
				delete [] data.buffer;*/
				
			return retval;	
		}	
    /*} catch (MasterDeviceException &e) {
        if (data.buffer_size)
            delete [] data.buffer;
        throw e;
    }*/

    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
		
		g_err.str("");
			
        g_err << "throwSingleSlaveRequired";
            
        /*if (data.buffer_size)
            delete [] data.buffer;*/
            
        /*throwSingleSlaveRequired(slaves.size());*/
        
        return -2;
    }
    data.slave_position = slaves.front().position;

    // write data via foe to the slave
    data.offset = 0;
    strncpy(data.file_name, storeFileName.c_str(),
            sizeof(data.file_name) - 1);
            
    data.buffer_size = g_foe_write_file_size;
    data.buffer = get_bulk_ptr(0);

    /*try {*/
        retval = m.writeFoe(&data);
        if(retval)
        {
			/*if (data.buffer_size)
				delete [] data.buffer;*/
				
            return retval;
        }    
    /* } catch (MasterDeviceException &e) {
        if (data.buffer_size)
            delete [] data.buffer;
        if (data.result) {
            if (data.result == FOE_OPCODE_ERROR) {
                err << "FoE write aborted with error code 0x"
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

    if (getVerbosity() == Verbose) {
        mycsout << "FoE writing finished." << '\n';
    }

    /*if (data.buffer_size)
        delete [] data.buffer;*/
        
    return 0;    
}

/****************************************************************************/

void CommandFoeWrite::loadFoeData(
        ec_ioctl_slave_foe_t *data,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        mycsout << "Read " << contents.size() << " bytes of FoE data." << '\n';
    }

    data->buffer_size = contents.size();

    if (data->buffer_size) {
        // allocate buffer and read file into buffer
        data->buffer = new uint8_t[data->buffer_size];
        contents.copy((char *) data->buffer, contents.size());
    }
}

/****************************************************************************/
