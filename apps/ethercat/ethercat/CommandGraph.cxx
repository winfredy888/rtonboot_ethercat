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
#include <map>
#include <algorithm>

using namespace std;

#include "myostream.h"

#include "CommandGraph.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandGraph::CommandGraph():
    Command("graph", "Output the bus topology as a graph.")
{
}

/****************************************************************************/

string CommandGraph::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str
        << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << binaryBaseName << " " << getName() << " [OPTIONS] <INFO>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "The bus is output in DOT language (see" << endl
        << "http://www.graphviz.org/doc/info/lang.html), which can" << endl
        << "be processed with the tools from the Graphviz" << endl
        << "package. Example:" << endl
        << endl
        << "  ethercat graph | dot -Tsvg > bus.svg" << endl
        << endl
        << "See 'man dot' for more information." << endl
        << endl
        << "Additional information at edges and nodes is selected via" << endl
        << "the first argument:" << endl
        << "  DC  - DC timing" << endl
        << "  CRC - CRC error register information" << endl
        << endl;

    return str.str();
}

/****************************************************************************/

enum Info {
    None,
    DC,
    CRC
};

#define REG_SIZE (20)

struct CrcInfo {
    unsigned int crc[EC_MAX_PORTS];
    unsigned int phy[EC_MAX_PORTS];
    unsigned int fwd[EC_MAX_PORTS];
    unsigned int lnk[EC_MAX_PORTS];
};

int CommandGraph::execute(const StringVector &args)
{
    Info info = None;
    ec_ioctl_master_t master;
    typedef vector<ec_ioctl_slave_t> SlaveVector;
    SlaveVector slaves;
    typedef vector<CrcInfo> CrcInfoVector;
    CrcInfoVector crcInfos;
    ec_ioctl_slave_t slave;
    SlaveVector::const_iterator si;
    map<int, string> portMedia;
    map<int, string>::const_iterator mi;
    map<int, int> mediaWeights;
    map<int, int>::const_iterator wi;
    
    int retval;

    portMedia[EC_PORT_MII] = "MII";
    mediaWeights[EC_PORT_MII] = 1;

    portMedia[EC_PORT_EBUS] = "EBUS";
    mediaWeights[EC_PORT_EBUS] = 5;

    if (args.size() > 1) {
        stringstream err;
        
        g_err.str("");
        
        g_err << "'" << getName() << "' takes either one or no arguments!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    if (args.size() == 1) {
        string arg = args[0];
        transform(arg.begin(), arg.end(),
                arg.begin(), (int (*) (int)) std::toupper);
        if (arg == "DC") {
            info = DC;
        }
        else if (arg == "CRC") {
            info = CRC;
        }
        else {
            stringstream err;
            
            g_err.str("");
            
            err << "Unknown argument \"" << args[0] << "\"!";
            
            /*throwInvalidUsageException(err);*/
            
            return -1;
        }
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
		
    retval = m.getMaster(&master);
    if(retval)
		return retval;

    for (unsigned int i = 0; i < master.slave_count; i++) {
        retval = m.getSlave(&slave, i);
        if(retval)
			return retval;
			
        slaves.push_back(slave);

    }

    if (info == CRC) {
        uint8_t data[REG_SIZE];
        ec_ioctl_slave_reg_t io;
        io.emergency = 0;
        io.address = 0x0300;
        io.size = REG_SIZE;
        io.data = data;

        for (unsigned int i = 0; i < master.slave_count; i++) {
            io.slave_position = i;
            /*try {*/
                retval = m.readReg(&io);
                if(retval)
					return retval;
					
            /*} catch (MasterDeviceException &e) {
                throw e;
            }*/

            CrcInfo crcInfo;
            for (int port = 0; port < EC_MAX_PORTS; port++) {
                crcInfo.crc[port] = io.data[ 0 + port * 2];
                crcInfo.phy[port] = io.data[ 1 + port * 2];
                crcInfo.fwd[port] = io.data[ 8 + port];
                crcInfo.lnk[port] = io.data[16 + port];
            }
            crcInfos.push_back(crcInfo);
        }
    }
    
    stringstream str1;
    
    str1 << "/* EtherCAT bus graph. Generated by 'ethercat graph'. */" << endl
        << endl
        << "strict graph bus {" << endl
        << "    rankdir=\"LR\"" << endl
        << "    ranksep=0.8" << endl
        << "    nodesep=0.8" << endl
        << "    node [fontname=\"Helvetica\"]" << endl
        << "    edge [fontname=\"Helvetica\",fontsize=\"10\"]" << endl
        << endl
        << "    master [label=\"EtherCAT\\nMaster\"]" << endl;

    /*if (slaves.size()) {
        str1 << "    master -- slave0";
        mi = portMedia.find(slaves.front().ports[0].desc);
        if (mi != portMedia.end())
            str1 << "[label=\"" << mi->second << "\"]";

        str1 << endl;
    }
    str1 << endl;*/

    uint16_t alias = 0x0000;
    uint16_t pos = 0;

    for (si = slaves.begin(); si != slaves.end(); si++) {
        if (si->alias) {
            alias = si->alias;
            pos = 0;
        }

        str1 << "    slave" << si->position << " [shape=\"box\""
            << ",label=\"" << si->position
            << " / " << alias << ":" << pos;
        /*if (string(si->order).size())
           str1 << "\\n" << si->order;*/
        if (info == DC && si->dc_supported) {
            str1 << "\\nDC: ";
            /*if (si->has_dc_system_time)*/
            if(0) {
                /*switch (si->dc_range) {
                    case EC_DC_32:
                        str1 << "32 bit";
                        break;
                    case EC_DC_64:
                        str1 << "64 bit";
                        break;
                    default:
                        break;
                }*/
            } else {
                str1 << "Delay meas.";
            }
            str1 << "\\nDelay: " << si->transmission_delay << " ns";
        }
        str1 << "\"]" << endl;
        
        uint16_t next_pos = si->next_slave;

        if (next_pos >= slaves.size()) {
             str1 << "Invalid next slave pointer." << endl;
        }
        
        str1 << "    slave" << si->position << " -- "
                << "slave" << next_pos << " [taillabel=]" << endl;

        for (int port = 0; port < EC_MAX_PORTS; port++) {
            /*uint16_t next_pos = si->ports[port].next_slave;

            if (next_pos == 0xffff) {
                continue;
            }

            if (next_pos >= slaves.size()) {
                str1 << "Invalid next slave pointer." << endl;
                continue;
            }

            ec_ioctl_slave_t *next = &slaves[next_pos];

            str1 << "    slave" << si->position << " -- "
                << "slave" << next_pos << " [taillabel=\"" << port;*/
            
            if(!port)
            {
				str1 << "\"headlabel=\"0";
			}	    
			else
			{
				str1 << "\",headlabel=\"0";
			}	    

            if (info == DC && si->dc_supported) {
                str1 << " [" << si->ports[port].receive_time << "]";
            }
            if (info == CRC) {
                CrcInfo *crcInfo = &crcInfos[si->position];
                str1 << " [" << crcInfo->crc[port] << "/"
                    << crcInfo->fwd[port] << "]";
            }

            /*if (info == DC && next->dc_supported) {
                str1 << " [" << next->ports[0].delay_to_next_dc << "]";
            }*/
            
            if (info == CRC) {
                CrcInfo *crcInfo = &crcInfos[next_pos];
                str1 << " [" << crcInfo->crc[0] << "/"
                    << crcInfo->fwd[0] << "]";
            }
            
            if(port == (EC_MAX_PORTS - 1))
            {
				str1 << "\"" << "\"";
			}
			else
			{
				/*str1 << "\"";*/
			}		
            
            #if 0
            
            str1 << "\"";

            mi = portMedia.find(si->ports[port].desc);
            if (mi == portMedia.end() && next) {
                /* Try medium of next-hop slave. */
                mi = portMedia.find(next->ports[0].desc);
            }

            if (mi != portMedia.end())
                str1 << ",label=\"" << mi->second << "\"";

            wi = mediaWeights.find(si->ports[port].desc);
            if (wi != mediaWeights.end())
                str1 << ",weight=\"" << wi->second << "\"";

            str1 << "]" << endl;
            
            #endif
        }

        str1 << endl;
        pos++;
    }

    str1 << "}" << endl;
    
    mycsout << str1.str();
    
    return 0;
}

/****************************************************************************/
