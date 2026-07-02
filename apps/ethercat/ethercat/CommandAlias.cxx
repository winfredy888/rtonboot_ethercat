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
#include <sstream>

using namespace std;

#include "myostream.h"

#include "CommandAlias.h"
#include "sii_crc.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandAlias::CommandAlias():
    Command("alias", "Write alias addresses.")
{
}

/****************************************************************************/

string CommandAlias::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS] <ALIAS>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Arguments:" << endl
        << "  ALIAS must be an unsigned 16 bit number. Zero means" << endl
        << "        removing an alias address." << endl
        << endl
        << "If multiple slaves are selected, the --force option" << endl
        << "is required." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --force    -f          Acknowledge writing aliases of" << endl
        << "                         multiple slaves." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
int CommandAlias::execute(const StringVector &args)
{
    uint16_t alias;
    stringstream err, strAlias;
    int number;
    SlaveList slaves;
    SlaveList::const_iterator si;

    if (args.size() != 1) {
		
		g_err.str("");
		
        g_err << "'" << getName() << "' takes exactly one argument!";
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    strAlias << args[0];
    strAlias
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strAlias.fail() || number < 0x0000 || number > 0xffff) {
		g_err.str("");
        g_err << "Invalid alias '" << args[0] << "'!";
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }
    alias = number;
    
    int32_t idx;
    
    idx = (int32_t)(getSingleMasterIndex());
    if(idx < 0)
    {
		g_err.str("");
        
        g_err << "getSingleMasterIndex parse error do nothing";
        
        return -3;
	}
	
    MasterDevice m(idx);
    
    int retval = m.open(MasterDevice::ReadWrite);
    if(retval)
		return retval;
		
    slaves = selectedSlaves(m);

    if (slaves.size() > 1 && !getForce()) {
		g_err.str("");
        g_err << "This will write the alias addresses of "
            << slaves.size() << " slaves to " << alias
            << "! Please specify --force to proceed.";
        /*throwCommandException(err);*/
        
        return -2;
    }

    if (!slaves.size() && getVerbosity() != Quiet) {
        mycsout << "Warning: Selection matches no slaves!" << '\n';
    }
    
    for (si = slaves.begin(); si != slaves.end(); si++) {
        writeSlaveAlias(m, *si, alias);
    }
    
    return 0;
}

/****************************************************************************/

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
int CommandAlias::writeSlaveAlias(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave,
        uint16_t alias
        )
{
    ec_ioctl_slave_sii_t data;
    stringstream err;
    uint8_t crc;

    #if 0 /*Winfred*/
    
    if (slave.sii_nwords < 8) {
		g_err.str("");
        g_err << "Current SII contents are too small to set an alias "
            << "(" << slave.sii_nwords << " words)!";
        /*throwCommandException(err);*/
        return -2;
    }
    
    #endif

    // read first 8 SII words
    data.slave_position = slave.position;
    data.offset = 0;
    data.nwords = 128;
    data.words = new uint16_t[data.nwords];

    /*try {*/
        int retval = m.readSii(&data);
    /*} catch (MasterDeviceException &e) {*/
    if(retval)
    {
        delete [] data.words;
        
        /*g_err.str("");
        
        g_err << "Failed to read SII: " << e.what();*/
        /*throwCommandException(err);*/
        
        return -2;
    }    
    /*}*/

    // write new alias address in word 4
    data.words[4] = cpu_to_le16(alias);

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data.words, 14);

    // write new checksum into first byte of word 7
    *(uint8_t *) (data.words + 7) = crc;
    
    data.is_alias = 1;

    // write first 8 words with new alias and checksum
    /*try {*/
        retval = m.writeSii(&data);
    /*} catch (MasterDeviceException &e) {*/
    if(retval)
    {
        delete [] data.words;
        
        /*g_err.str("");
        g_err << "Failed to read SII: " << e.what();*/
        
        return -2;
        /*throwCommandException(err);*/
    }    
    /*}*/

    delete [] data.words;
    
    return 0;
}

/****************************************************************************/
