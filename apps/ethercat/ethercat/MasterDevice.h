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

#ifndef __MASTER_DEVICE_H__
#define __MASTER_DEVICE_H__

#include <stdexcept>
#include <sstream>
using namespace std;

#include "ecrt.h"
#include "myioctl.h"

/****************************************************************************/

class MasterDeviceException:
    public runtime_error
{
    friend class MasterDevice;

    protected:
        /** Constructor with string parameter. */
        MasterDeviceException(
                const string &s /**< Message. */
                ): runtime_error(s) {}

        /** Constructor with stringstream parameter. */
        MasterDeviceException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class MasterDeviceSdoAbortException:
    public MasterDeviceException
{
    friend class MasterDevice;

    public:
        uint32_t abortCode;

    protected:
        /** Constructor with abort code parameter. */
        MasterDeviceSdoAbortException(uint32_t code):
            MasterDeviceException("SDO transfer aborted.") {
                abortCode = code;
            };
};

/****************************************************************************/

class MasterDeviceSoeException:
    public MasterDeviceException
{
    friend class MasterDevice;

    public:
        uint16_t errorCode;

    protected:
        /** Constructor with error code parameter. */
        MasterDeviceSoeException(uint16_t code):
            MasterDeviceException("SoE transfer aborted.") {
                errorCode = code;
            };
};

/****************************************************************************/

#ifdef EC_EOE
class MasterDeviceEoeException:
    public MasterDeviceException
{
    friend class MasterDevice;

    public:
        uint16_t result;

    protected:
        /** Constructor with error code parameter. */
        MasterDeviceEoeException(uint16_t result):
            MasterDeviceException("EoE set IP parameter failed."),
            result(result) {};
};
#endif

/****************************************************************************/

class MasterDevice
{
    public:
        MasterDevice(unsigned int = 0U);
        ~MasterDevice();

        void setIndex(unsigned int);
        unsigned int getIndex() const;

        enum Permissions {Read, ReadWrite};
        int open(Permissions);
        void close();

        int getModule(ec_ioctl_module_t *);

        int getMaster(ec_ioctl_master_t *);
        int getConfig(ec_ioctl_config_t *, unsigned int);
        int getConfigPdo(ec_ioctl_config_pdo_t *, unsigned int, uint8_t,
                uint16_t);
        int getConfigPdoEntry(ec_ioctl_config_pdo_entry_t *, unsigned int,
                uint8_t, uint16_t, uint8_t);
        int getConfigSdo(ec_ioctl_config_sdo_t *, unsigned int, unsigned int);
        int getConfigIdn(ec_ioctl_config_idn_t *, unsigned int, unsigned int);
        int getConfigFlag(ec_ioctl_config_flag_t *, unsigned int,
                unsigned int);
        int getDomain(ec_ioctl_domain_t *, unsigned int);
        int getFmmu(ec_ioctl_domain_fmmu_t *, unsigned int, unsigned int);
        int getData(ec_ioctl_domain_data_t *, unsigned int, unsigned int,
                unsigned char *);
        int getSlave(ec_ioctl_slave_t *, uint16_t);
        int getSync(ec_ioctl_slave_sync_t *, uint16_t, uint8_t);
        int getPdo(ec_ioctl_slave_sync_pdo_t *, uint16_t, uint8_t, uint8_t);
        int getPdoEntry(ec_ioctl_slave_sync_pdo_entry_t *, uint16_t, uint8_t,
                uint8_t, uint8_t);
        int getSdo(ec_ioctl_slave_sdo_t *, uint16_t, uint16_t);
        int getSdoEntry(ec_ioctl_slave_sdo_entry_t *, uint16_t, int, uint8_t);
        int readSii(ec_ioctl_slave_sii_t *);
        int writeSii(ec_ioctl_slave_sii_t *);
        int readReg(ec_ioctl_slave_reg_t *);
        int writeReg(ec_ioctl_slave_reg_t *);
        int setDebug(unsigned int);
        int rescan();
        int sdoDownload(ec_ioctl_slave_sdo_download_t *);
        int sdoUpload(ec_ioctl_slave_sdo_upload_t *);
        int requestState(uint16_t, uint8_t);
        int readFoe(ec_ioctl_slave_foe_t *);
        int writeFoe(ec_ioctl_slave_foe_t *);
        int readSoe(ec_ioctl_slave_soe_read_t *);
        int writeSoe(ec_ioctl_slave_soe_write_t *);
/*#ifdef EC_EOE
        void getEoeHandler(ec_ioctl_eoe_handler_t *, uint16_t);
        void getIpParam(ec_ioctl_eoe_ip_t *, uint16_t);
#endif*/

		int setIpParam(ec_ioctl_eoe_ip_t *);

        unsigned int getMasterCount() const {return masterCount;}

    private:
        unsigned int index;
        unsigned int masterCount;
        int fd;
};

/****************************************************************************/

inline unsigned int MasterDevice::getIndex() const
{
    return index;
}

/****************************************************************************/

#endif
