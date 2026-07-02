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
#include <fstream>

using namespace std;

#include "myostream.h"

#include "CommandSiiWrite.h"
#include "sii_crc.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandSiiWrite::CommandSiiWrite():
    Command("sii_write", "Write SII contents to a slave.")
{
}

/****************************************************************************/

string CommandSiiWrite::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <FILENAME>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "The file contents are checked for validity and integrity." << endl
        << "These checks can be overridden with the --force option." << endl
        << endl
        << "Arguments:" << endl
        << "  FILENAME must be a path to a file that contains a" << endl
        << "           positive number of words. If it is '-'," << endl
        << "           data are read from stdin." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --force    -f          Override validity checks." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

extern "C" 
{
	sem_t g_sii_write_sem;
	
	extern uint32_t g_sii_write_file_size;
	
	extern uint8_t * get_bulk_ptr(uint32_t offset);
}	

int CommandSiiWrite::execute(const StringVector &args)
{
    stringstream err;
    ec_ioctl_slave_sii_t data;
    ifstream file;
    SlaveList slaves;
    int retval;

    if (args.size() != 1) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes exactly one argument!";
        
        return -1;
        
        /*throwInvalidUsageException(err);*/
    }
    
    #if 0

    if (args[0] == "-") {
        retval = loadSiiData(&data, cin);
        if(retval)
			return retval;
    } else {
        file.open(args[0].c_str(), ifstream::in | ifstream::binary);
        if (file.fail()) {
			
			g_err.str("");
			
            g_err << "Failed to open '" << args[0] << "'!";
            
            /*throwCommandException(err);*/
            
            return -2;
        }
        
        retval = loadSiiData(&data, file);
        if(retval)
			return retval;
			
        file.close();
    }
    
    #endif
    
    while (sem_wait(&g_sii_write_sem) < 0)
    {
		  ++retval;
		  
		  --retval;
	};
	
	data.is_alias = 0;
    data.offset = 0;
    data.nwords = g_sii_write_file_size / 2;
    data.words = (uint16_t *)(get_bulk_ptr(0));
     
    if (!getForce()) {
        /*try {*/
            retval = checkSiiData(&data);
            if(retval)
            {
				/*delete [] data.words;*/
				
				return retval;
			}	
				
        /*} catch (CommandException &e) {
            delete [] data.words;
            throw e;
        }*/
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
			/*delete [] data.words;*/
			
			return retval;
		}	
		
    /*} catch (MasterDeviceException &e) {
        delete [] data.words;
        throw e;
    }*/

    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        /*delete [] data.words;*/
        
        g_err.str("");
			
        g_err << "throwSingleSlaveRequired";
            
        /*throwSingleSlaveRequired(slaves.size());*/
        
        return -2;
    }
    
    data.slave_position = slaves.front().position;

    // send data to master
    /*data.offset = 0;*/
    /*try {*/
        retval = m.writeSii(&data);
        if(retval)
        {
			/*delete [] data.words;*/
			
			return retval;
		}	
    /*} catch (MasterDeviceException &e) {
        delete [] data.words;
        throw e;
    }*/

    if (getVerbosity() == Verbose) {
        mycsout << "SII writing finished." << '\n';
    }

    /*delete [] data.words;*/
    
    return 0;
}

/****************************************************************************/

int CommandSiiWrite::loadSiiData(
        ec_ioctl_slave_sii_t *data,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        mycsout << "Read " << contents.size() << " bytes of SII data." << '\n';
    }

    if (!contents.size() || contents.size() % 2) {
		
		g_err.str("");
		
        g_err << "Invalid data size " << contents.size() << "! "
            << "Must be non-zero and even.";
            
        /*throwCommandException(err);*/
        
        return -2;
    }
    data->nwords = contents.size() / 2;

    // allocate buffer and read file into buffer
    data->words = new uint16_t[data->nwords];
    contents.copy((char *) data->words, contents.size());
    
    return 0;
}

/****************************************************************************/

int CommandSiiWrite::checkSiiData(
        const ec_ioctl_slave_sii_t *data
        )
{
    stringstream err;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    uint8_t crc;

    if (data->nwords < 0x0041) {
		
		g_err.str("");
		
        g_err << "SII data too short (" << data->nwords << " words)! Mimimum is"
            " 40 fixed words + 1 delimiter. Use --force to write anyway.";
        /*throwCommandException(err);*/
        
        return -2;
    }

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data->words, 14);
    if (crc != ((const uint8_t *) data->words)[14]) {
		
		g_err.str("");
		
        g_err << "CRC incorrect. Must be 0x"
            << hex << setfill('0') << setw(2) << (unsigned int) crc
            << ". Use --force to write anyway.";
        /*throwCommandException(err);*/
        
        return -2;
    }

    // cycle through categories to detect corruption
    categoryHeader = data->words + 0x0040U;
    categoryType = le16_to_cpup(categoryHeader);
    while (categoryType != 0xffff) {
        if (categoryHeader + 1 > data->words + data->nwords) {
			
			g_err.str("");
			
            g_err << "SII data seem to be corrupted! "
                << "Use --force to write anyway.";
                
            /*throwCommandException(err);*/
            
            return -2;
        }
        
        categorySize = le16_to_cpup(categoryHeader + 1);
        if (categoryHeader + 2 + categorySize + 1
                > data->words + data->nwords) {
					
			g_err.str("");
			
            g_err << "SII data seem to be corrupted! "
                "Use --force to write anyway.";
                
            /*throwCommandException(err);*/
            
            return -2;
        }
        categoryHeader += 2 + categorySize;
        categoryType = le16_to_cpup(categoryHeader);
    }
    
    return 0;
}

/****************************************************************************/
