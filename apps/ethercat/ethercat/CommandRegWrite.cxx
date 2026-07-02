/*****************************************************************************
 *
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <fstream>

using namespace std;

#include "myostream.h"

#include "CommandRegWrite.h"
#include "sii_crc.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandRegWrite::CommandRegWrite():
    Command("reg_write", "Write data to a slave's registers.")
{
}

/****************************************************************************/

string CommandRegWrite::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <OFFSET> <DATA>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  ADDRESS is the register address to write to." << endl
        << "  DATA    depends on whether a datatype was specified" << endl
        << "          with the --type option: If not, DATA must be" << endl
        << "          either a path to a file with data to write," << endl
        << "          or '-', which means, that data are read from" << endl
        << "          stdin. If a datatype was specified, VALUE is" << endl
        << "          interpreted respective to the given type." << endl
        << endl
        << typeInfo()
        << endl
        << "Command-specific options:" << endl
        << "  --alias     -a <alias>" << endl
        << "  --position  -p <pos>    Slave selection. See the help of"
        << endl
        << "                          the 'slaves' command." << endl
        << "  --type      -t <type>   Data type (see above)." << endl
        << "  --emergency -e          Send as emergency request." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandRegWrite::execute(const StringVector &args)
{
    stringstream strOffset, err;
    ec_ioctl_slave_reg_t io;
    ifstream file;
    int retval;
    long temp;
    

    if (args.size() != 2) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes exactly two arguments!";
        
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

    if (getDataType().empty()) {
		
		#if 0
		
        if (args[1] == "-") {
            retval = loadRegData(&io, cin);
            if(retval)
				return retval;
        } else {
            file.open(args[1].c_str(), ifstream::in | ifstream::binary);
            if (file.fail()) {
				
				g_err.str("");
				
                g_err << "Failed to open '" << args[1] << "'!";
                
                /*throwCommandException(err);*/
                
                return -2;
            }
            retval = loadRegData(&io, file);
            if(retval)
				return retval;
				
            file.close();
        }
        
        #else
        
        g_err.str("");
		
        g_err << "data type must not empty !";
        
        return -2;
        
        #endif
        
        
    } else {
        stringstream strValue;
        const DataType *dataType = findDataType(getDataType());

        if (!dataType) {
			
			g_err.str("");
			
            g_err << "Invalid data type '" << getDataType() << "'!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }

        if (dataType->byteSize) {
            io.size = dataType->byteSize;
        } else {
            io.size = 1024; // FIXME
        }

        io.data = new uint8_t[io.size];

        /*try {*/
            io.size = interpretAsType(
                    dataType, args[1], io.data, io.size);
            temp = (long)(io.size);        
            if(temp < 0)
            {
				delete [] io.data;
				
				return -2;
			}	
			
        /*} catch (SizeException &e) {
            delete [] io.data;
            throwCommandException(e.what());
        } catch (ios::failure &e) {
            delete [] io.data;
            err << "Invalid value argument '" << args[1]
                << "' for type '" << dataType->name << "'!";
            throwInvalidUsageException(err);
        }*/
    }

    if ((uint32_t) io.address + io.size > 0xffff) {
		
		g_err.str("");
		
        g_err << "Address and size exceeding 64k!";
        
        delete [] io.data;
        
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
    
    /*try {*/
        retval = m.open(MasterDevice::ReadWrite);
        if(retval)
        {
			delete [] io.data;
			
			return retval;
		}	
		
    /*} catch (MasterDeviceException &e) {
        delete [] io.data;
        throw e;
    }*/

    /*if (getEmergency())*/
    if(0) {
        io.slave_position = emergencySlave();
        io.emergency = true;
    }
    else {
        SlaveList slaves = selectedSlaves(m);
        if (slaves.size() != 1) {
            delete [] io.data;
            
            /*throwSingleSlaveRequired(slaves.size());*/
            
            g_err.str("");
		
			g_err << "throwSingleSlaveRequired";
			
			return -1;
            
        }
        
        io.slave_position = slaves.front().position;
        io.emergency = false;
    }

    // send data to master
    /*try {*/
        retval = m.writeReg(&io);
        if(retval)
        {
			delete [] io.data;
			
			return retval;
		}	
    /*} catch (MasterDeviceException &e) {
        delete [] io.data;
        throw e;
    }*/

    if (getVerbosity() == Verbose) {
        mycsout << "Register writing finished." << '\n';
    }

    delete [] io.data;
    
    return 0;
}


/****************************************************************************/

int CommandRegWrite::loadRegData(
        ec_ioctl_slave_reg_t *io,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        mycsout << "Read " << contents.size() << " bytes of data." << '\n';
    }

    if (contents.size() > 0xffff) {
		
		g_err.str("");
		
        g_err << "Invalid data size " << contents.size() << "!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    io->size = contents.size();

    // allocate buffer and read file into buffer
    io->data = new uint8_t[io->size];
    contents.copy((char *) io->data, contents.size());
    
    return 0;
}

/****************************************************************************/
