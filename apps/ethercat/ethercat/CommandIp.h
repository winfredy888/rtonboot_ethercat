/*****************************************************************************
 *
 *  Copyright (C) 2006-2014  Florian Pose, Ingenieurgemeinschaft IgH
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

#ifndef __COMMANDIP_H__
#define __COMMANDIP_H__

#include "Command.h"

/****************************************************************************/

class CommandIp:
    public Command
{
    public:
        CommandIp();

        string helpString(const string &) const;
        int execute(const StringVector &);
        
        int my_getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result);
        
        void my_freeaddrinfo(struct addrinfo *result);

    protected:
        int parseMac(unsigned char [6], const string &);
        int parseIpv4Prefix(ec_ioctl_eoe_ip_t *, const string &);
        int resolveIpv4(struct in_addr *, const string &);
};

/****************************************************************************/

#endif
