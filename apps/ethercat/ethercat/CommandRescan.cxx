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

#include <sstream>
#include <iomanip>

using namespace std;

#include "myostream.h"

#include "CommandRescan.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandRescan::CommandRescan():
    Command("rescan", "Rescan the bus.")
{
}

/****************************************************************************/

string CommandRescan::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command a bus rescan. Gathered slave information will be" << endl
        << "forgotten and slaves will be read in again." << endl
        << endl;

    return str.str();
}

/****************************************************************************/

int CommandRescan::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
	int retval;
	int is_master_empty;

    if (args.size() != 0) {
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
        MasterDevice m(*mi);
        
        is_master_empty = 0;
        
        retval = m.open(MasterDevice::ReadWrite);
        if(retval)
			return retval;
			
        retval = m.rescan();
        if(retval)
			return retval;
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
