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

#include <iomanip>
using namespace std;

#include "myostream.h"

#include "SoeCommand.h"
#include "soe_errors.c"

/****************************************************************************/

uint16_t SoeCommand::parseIdn(const string &str, int * p_return_code)
{
    uint16_t idn = 0x0000;
    stringstream s, err;
    
    *p_return_code = 0;

    if (!str.length()) {
		
		g_err.str("");
		
        g_err << "Zero-size string not allowed!";
        
        /* throw runtime_error(err.str()); */
        
        *p_return_code = -1;
        
        return idn;
    }

    if (str[0] == 'S' || str[0] == 'P') {
        unsigned int num;
        unsigned char c;

        s << str;

        s >> c;
        if (c == 'P') {
            idn |= 0x8000;
        }

        s >> c;
        if (s.fail() || c != '-') {
			
			g_err.str("");
			
            g_err << "'-' expected!";
            
            /* throw runtime_error(err.str());*/
            
            *p_return_code = -1;
            
            return idn;
        }

        s >> num;
        if (s.fail() || num > 7) {
            
            
            g_err.str("");
			
             g_err << "Invalid parameter set number!";
             
             *p_return_code = -1;
             
             return idn;
             
            /*throw runtime_error(err.str());*/
            
        }
        idn |= num << 12;

        s >> c;
        if (s.fail() || c != '-') {
			
			g_err.str("");
			
             g_err << "'-' expected!";
             
             *p_return_code = -1;
             
             /*throw runtime_error(err.str());*/
             
             return idn;
        }

        s >> num;
        if (s.fail() || num > 4095) {
			
			g_err.str("");
			
            g_err << "Invalid data block number!";
            
            *p_return_code = -1;
            
            /*throw runtime_error(err.str());*/
            
            return idn;
        }
        idn |= num;

        s.peek();
        if (!s.eof()) {
			
			g_err.str("");
			
            g_err << "Additional input!";
            
            *p_return_code = -1;
            
            /*throw runtime_error(err.str());*/
            
            return idn;
        }
    } else {
        s << str;
        s >> resetiosflags(ios::basefield) >> idn;
        if (s.fail()) {
			
			g_err.str("");
			
            g_err << "Invalid number!";
            
            *p_return_code = -1;
            
            /*throw runtime_error(err.str());*/
            
            return idn;
        }
    }

    return idn;
}

/****************************************************************************/

string SoeCommand::outputIdn(uint16_t idn)
{
    stringstream str;

    str << ((idn & 0x8000) ? 'P' : 'S')
        << "-" << ((idn >> 12) & 0x07)
        << "-" << setfill('0') << setw(4) << (idn & 0x0fff);

    return str.str();
}

/****************************************************************************/

/** Outputs an SoE error code.
*/
std::string SoeCommand::errorMsg(uint16_t code)
{
    const ec_code_msg_t *error_msg;
	stringstream str;

	str << "0x" << hex << setfill('0') << setw(4) << code << ": ";

    for (error_msg = soe_error_codes; error_msg->code; error_msg++) {
        if (error_msg->code == code) {
			str << error_msg->message;
			return str.str();
        }
    }

	str << "(Unknown)";
	return str.str();
}

/****************************************************************************/
