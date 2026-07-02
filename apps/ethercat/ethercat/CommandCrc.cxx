/*****************************************************************************
 *
 *  Copyright (C) 2006-2017  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <algorithm>

using namespace std;

#include "myostream.h"

#include "CommandCrc.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandCrc::CommandCrc():
    Command("crc", "CRC error register diagnosis.")
{
}

/****************************************************************************/

string CommandCrc::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str
        << binaryBaseName << " " << getName() << endl
        << binaryBaseName << " " << getName() << " reset" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "CRC - CRC Error Counter                0x300, 0x302, 0x304, 0x306"
        << endl
        << "PHY - Physical Interface Error Counter 0x301, 0x303, 0x305, 0x307"
        << endl
        << "FWD - Forwarded RX Error Counter       0x308, 0x309, 0x30a, 0x30b"
        << endl
        << "NXT - Next slave" << endl
        << endl;

    return str.str();
}

/****************************************************************************/

#define REG_SIZE  (20)

int CommandCrc::execute(const StringVector &args)
{
    bool reset = false;
    int retval;

    if (args.size() > 1) {
        stringstream err;
        
        g_err.str("");
        
		g_err << "'" << getName() << "' takes either no or 'reset' argument!";
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    if (args.size() == 1) {
        string arg = args[0];
        transform(arg.begin(), arg.end(),
                arg.begin(), (int (*) (int)) std::tolower);
        if (arg != "reset") {
            stringstream err;
            
            g_err.str("");
            
            g_err << "'" << getName() << "' takes either no or 'reset' argument!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }

        reset = true;
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

    ec_ioctl_master_t master;
    retval = m.getMaster(&master);
    if(retval)
		return retval;

    uint8_t data[REG_SIZE];
    ec_ioctl_slave_reg_t io;
    io.emergency = 0;
    io.address = 0x0300;
    io.size = REG_SIZE;
    io.data = data;

    if (reset) {
        for (int j = 0; j < REG_SIZE; j++) {
            data[j] = 0x00;
        }

        for (unsigned int i = 0; i < master.slave_count; i++) {

            io.slave_position = i;
            /*try {*/
                retval = m.writeReg(&io);
                if(retval)
					return retval;
					
            /*} catch (MasterDeviceException &e) {
                throw e;
            }*/
        }
        
        return 0;
    }
    
    stringstream str1;

    str1 << "   |";
    for (unsigned int port = 0; port < EC_MAX_PORTS; port++) {
        str1 << "Port " << port << "         |";
    }
    str1 << endl;

    str1 << "   |";
    for (unsigned int port = 0; port < EC_MAX_PORTS; port++) {
        /*str1 << "CRC PHY FWD NXT|";*/
		str1 << "CRC PHY FWD|";
    }
    str1 << endl;

    for (unsigned int i = 0; i < master.slave_count; i++) {

        ec_ioctl_slave_t slave;
        retval = m.getSlave(&slave, i);
        if(retval)
			return retval;

        io.slave_position = i;
        /*try {*/
            retval = m.readReg(&io);
            if(retval)
				return retval;
        /*} catch (MasterDeviceException &e) {
            throw e;
        }*/

        str1 << setw(3) << i << "|";
          
        for (int port = 0; port < 4; port++) {
            /*if (slave.ports[port].link.loop_closed) {
                str1 << "               |";
                continue;
            }*/

            str1 << setw(3) << (unsigned int) io.data[ 0 + port * 2]; // CRC
            str1 << setw(4) << (unsigned int) io.data[ 1 + port * 2]; // PHY
            str1 << setw(4) << (unsigned int) io.data[ 8 + port]; // FWD
            /*if (slave.ports[port].next_slave == i - 1) {
                str1 << "   ↑";
            }
            else if (slave.ports[port].next_slave == i + 1) {
                str1 << "   ↓";
            }
            else if (slave.ports[port].next_slave != 0xffff) {
                str1 << setw(4) << slave.ports[port].next_slave;
            }
            else {
                str1 << "    ";
            }*/

            str1 << "|";
        }

        std::string slaveName(slave.name);
        slaveName = slaveName.substr(0, 11);
        str1 << slaveName << endl;
    }
    
    mycsout << str1.str();
    
    return 0;
}

/****************************************************************************/
