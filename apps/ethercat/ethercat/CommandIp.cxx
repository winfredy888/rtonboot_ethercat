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
 *  vim: expandtab
 *
 ****************************************************************************/

#include <wchar.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include "myostream.h"

#include "CommandIp.h"
#include "MasterDevice.h"

/****************************************************************************/

CommandIp::CommandIp():
    Command("ip", "Set EoE IP parameters.")
{
}

/****************************************************************************/

string CommandIp::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS] <ARGS>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "IP parameters can be appended as argument pairs:" << endl
        << endl
        << "  ip_address <IPv4>[/prefix]  IP address (optionally with" << endl
        << "                                decimal subnet prefix)" << endl
        << "  mac_address <MAC>           Link-layer address (may contain"
        << endl
        << "                                colons or hyphens)" << endl
        << "  default_gateway <IPv4>      Default gateway" << endl
        << "  dns_address <IPv4>          DNS server address" << endl
        << "  hostname <hostname>         Host name (max. 32 byte)" << endl
        << endl
        << "IPv4 adresses can be given either in dot notation or as" << endl
        << "hostnames, which will be automatically resolved." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

int CommandIp::execute(const StringVector &args)
{
	int retval;
	
    if (args.size() <= 0) {
        return -1;
    }

    if (args.size() % 2) {     
        g_err.str("");
		
        g_err << "'" << getName() << "' needs an even number of arguments!";
        
        /*throwInvalidUsageException(err);*/
        
        return -1;
    }

    ec_ioctl_eoe_ip_t io = {};

    for (unsigned int argIdx = 0; argIdx < args.size(); argIdx += 2) {
        string arg = args[argIdx];
        string val = args[argIdx + 1];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "link" or arg == "mac_address") {
            retval = parseMac(io.mac_address, val);
            if(retval)
				return retval;
				
            io.mac_address_included = 1;
        }
        else if (arg == "addr" or arg == "ip_address") {
            retval = parseIpv4Prefix(&io, val);
            if(retval)
				return retval;
				
            io.ip_address_included = 1;
        }
        else if (arg == "default" or arg == "default_gateway") {
            retval = resolveIpv4(&io.gateway, val);
            if(retval)
				return retval;
				
            io.gateway_included = 1;
        }
        else if (arg == "dns" or arg == "dns_adress") {
            retval = resolveIpv4(&io.dns, val);
            if(retval)
				return retval;
				
            io.dns_included = 1;
        }
        else if (arg == "name" or arg == "hostname") {
            if (val.size() > EC_MAX_HOSTNAME_SIZE - 1) {
				
				g_err.str("");
		
				g_err << "Name too long!";
			
				/*throwInvalidUsageException(err);*/
        
				return -1;
            }
            
            unsigned int i;
            for (i = 0; i < val.size(); i++) {
                io.name[i] = val[i];
            }
            io.name[i] = 0;
            io.name_included = 1;
        }
        else {            
            g_err.str("");
		
			g_err << "Unknown argument '" << args[argIdx] << "'!";
			
			/*throwInvalidUsageException(err);*/
        
			return -1;
        }
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::ReadWrite);
    SlaveList slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        /*throwSingleSlaveRequired(slaves.size());*/
        
        g_err.str("");
		
		g_err << "throwSingleSlaveRequired";
			
		return -1;
    }
    
    io.slave_position = slaves.front().position;

    // execute actual request
    /*try {*/
        retval = m.setIpParam(&io);
        if(retval)
			return retval;
			
    /*} catch (MasterDeviceException &e) {
        throw e;
    }*/
    
    return 0;
}

/****************************************************************************/

int CommandIp::parseMac(unsigned char mac[EC_ETH_ALEN], const string &str)
{
    unsigned int pos = 0;

    for (unsigned int i = 0; i < EC_ETH_ALEN; i++) {
        if (pos + 2 > str.size()) {
            stringstream err;
            
            /*err << "Incomplete MAC address!";
            throwInvalidUsageException(err); */
            
            g_err.str("");
		
			g_err << "Incomplete MAC address!";
			
			return -1;
        }

        string byteStr = str.substr(pos, 2);
        pos += 2;

        stringstream s;
        s << byteStr;
        unsigned int byteValue;
        s >> hex >> byteValue;
        if (s.fail() || !s.eof() || byteValue > 0xff) {
            stringstream err;
            /*err << "Invalid MAC address!";
            throwInvalidUsageException(err);*/
            
            g_err.str("");
		
			g_err << "Invalid MAC address!";
			
			return -1;
        }
        mac[i] = byteValue;

        while (pos < str.size() && (str[pos] == ':' || str[pos] == '-')) {
            pos++;
        }
    }
    
    return 0;
}

uint32_t ip_to_uint32(const char *ip_str, int * p_ret) {
    uint32_t ip = 0;
    int octet;
    int shift = 24;
    const char *start = ip_str;
    char *end = NULL;
    
    *p_ret = 0;
    
    for (int i = 0; i < 4; i++) {
        
        octet = strtol(start, &end, 10);
        
        if (octet < 0 || octet > 255) {
			
			*p_ret = -1;
			
			return 0;
        }
        
        ip |= (octet << shift);
        shift -= 8;
        
        if (*end == '.' || *end == '\0') {
            start = end + 1;
        } else {
            *p_ret = -1;
			
			return 0;
        }
    }
 
        
    return ip;
}    

/****************************************************************************/

int CommandIp::parseIpv4Prefix(ec_ioctl_eoe_ip_t *io,
        const string &str)
{
    size_t pos = str.find('/');
    string host;
    int retval;

    io->subnet_mask_included = pos != string::npos;

    if (pos == string::npos) { // no prefix found
        host = str;
    }
    else {
        host = str.substr(0, pos);
        string prefixStr = str.substr(pos + 1, string::npos);
        stringstream s;
        s << prefixStr;
        unsigned int prefix;
        s >> prefix;
        if (s.fail() || !s.eof() || prefix > 32) {
            stringstream err;
            /*err << "Invalid prefix '" << prefixStr << "'!";
            throwInvalidUsageException(err);*/
            
            g_err.str("");
		
			g_err << "Invalid prefix '" << prefixStr << "'!";
			
			return -1;
        }
        uint32_t mask = (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF;
        io->subnet_mask.s_addr = htonl(mask);
    }

    retval = resolveIpv4(&io->ip_address, host);
    if(retval)
		return retval;
		
	return 0;	
}

int CommandIp::my_getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    // 这里只是一个简单的示例实现，实际中需要调用操作系统提供的真正的网络功能
    // 如果 hostname 是 "localhost"，服务名是 "80"，则模拟返回一个 addrinfo 结构
    if (strcmp(hostname, "localhost") == 0 && strcmp(service, "80") == 0) {
        struct addrinfo *ai = (struct addrinfo *)(malloc(sizeof(struct addrinfo)));
        if (ai == NULL) return -1; // 内存分配失败
 
        ai->ai_family = AF_INET;
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = IPPROTO_TCP;
        ai->ai_addrlen = sizeof(struct sockaddr_in);
        struct sockaddr_in *addr = (struct sockaddr_in *)(malloc(sizeof(struct sockaddr_in)));
        if (addr == NULL) {
            free(ai);
            return -1; // 内存分配失败
        }
        addr->sin_family = AF_INET;
        addr->sin_port = htons(80);
        addr->sin_addr.s_addr = inet_addr("127.0.0.1");
        ai->ai_addr = (struct sockaddr *)addr;
        ai->ai_next = NULL; // 模拟只返回一个 addrinfo 结构
 
        *result = ai;
        return 0; // 成功
    }
 
    // 如果输入不是 "localhost" 和 "80"，则模拟失败
    *result = NULL;
    return -1;
}

void CommandIp::my_freeaddrinfo(struct addrinfo *result)
{
	free(result->ai_addr);
    free(result);
}    

/****************************************************************************/

int CommandIp::resolveIpv4(struct in_addr *dst, const string &str)
{
	#if 0
	
    struct addrinfo hints = {};
    struct addrinfo *res;

    hints.ai_family = AF_INET; // only IPv4

    int ret = my_getaddrinfo(str.c_str(), NULL, &hints, &res);
    if (ret) {
        stringstream err;
        
        /*err << "Lookup of '" << str << "' failed: "
            << strerror(ret) << endl;
        throwCommandException(err.str());*/
        
        g_err.str("");
		
		g_err << "Lookup of '" << str << "' failed: "
            << strerror(ret) << endl;
			
		return -1;
    }

    if (!res) { // returned list is empty
        stringstream err;
        /*err << "Lookup of '" << str << "' failed." << endl;
        throwCommandException(err.str());*/
        
        g_err.str("");
		
		g_err << "Lookup of '" << str << "' failed." << endl;
			
		return -1;
    }

    const struct sockaddr_in *in_addr =
        (const struct sockaddr_in *) res->ai_addr;
    *dst = in_addr->sin_addr;
    my_freeaddrinfo(res);
    
    #else
    
    int ret;
    
    uint32_t ip = ip_to_uint32(str.c_str(), &ret);
    if(!ret)
    {
		(*dst).s_addr = htonl(ip);
	}	
    
    #endif
    
    return ret;
}

/****************************************************************************/
