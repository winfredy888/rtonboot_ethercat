/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include "oshw.h"
#include <stdlib.h>

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_htons(const uint16 host)
{
	uint16 hostlow;
	
	hostlow = host & 0xff;
	
	hostlow <<= 8;
	 
	return ((hostlow) | (host >> 8));
}

/**
 * Network (i.e. big endian) to Host byte order.
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_ntohs(const uint16 network)
{
	uint16 netlow;
	
	netlow = network & 0xff;
	
	netlow <<= 8;
	 
	return ((netlow) | (network >> 8));
}

/* Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert * oshw_find_adapters(void)
{
   ec_adaptert * ret_adapter = NULL;

   /* TODO if needed */

   return ret_adapter;
}

/** Free memory allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters(ec_adaptert * adapter)
{
	   /* TODO if needed */
}
