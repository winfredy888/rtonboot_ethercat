/*****************************************************************************
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master userspace library.
 *
 *  The IgH EtherCAT master userspace library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************/

#include "ecrt.h"

/****************************************************************************/

struct ec_master {
    int fd;
    uint8_t *process_data;
    size_t process_data_size;

    ec_domain_t *first_domain;
    ec_slave_config_t *first_config;
};

/****************************************************************************/

void lib_ec_master_clear(ec_master_t *);

/****************************************************************************/

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

static void my_fprintf(struct file_struct * fp, const char * format, ...)
{
	va_list args;
	
	va_start(args, format);
	linux_vprintf(format, args);
	va_end(args);
}


