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

#include "CommandDebug.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandDebug::CommandDebug():
    Command("debug", "Set the master's debug level.")
{
}

/****************************************************************************/

string CommandDebug::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " <LEVEL>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Debug messages are printed to syslog." << endl
        << endl
        << "Arguments:" << endl
        << "  LEVEL can have one of the following values:" << endl
        << "        0 for no debugging output," << endl
        << "        1 for some debug messages, or" << endl
        << "        2 for printing all frame contents (use with caution!)."
        << endl << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandDebug::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    stringstream str;
    int debugLevel;
    int retval;
    
    int is_master_empty;
    
    if(1)
    {
		g_err.str("");
		
        g_err << "ICOS does not support this command!!!";
        
        return -8;
	}	

    if (args.size() != 1) {
        stringstream err;
        
        g_err.str("");
        
        g_err << "'" << getName() << "' takes exactly one argument!";
        
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    str << args[0];
    str >> resetiosflags(ios::basefield) // guess base from prefix
        >> debugLevel;

    if (str.fail()) {
        stringstream err;
        
        g_err.str("");
        
        g_err << "Invalid debug level '" << args[0] << "'!";
        
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
			
        retval =m.setDebug(debugLevel);
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
