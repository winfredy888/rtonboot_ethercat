#include <nuttx/config.h>
#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <stdio.h>

#include "../../../nuttx/libs/libsoem/ethercat.h"

#include "icosdev.h"

ec_cdev_t g_cdev;
ec_master_t g_master;

struct file_operations eccdev_fops;

extern int linux_printf(const char *fmt, ...);

extern int pipe_printf(const char *fmt, ...);

#undef DEBUG

int eccdev_open(FAR struct file * filep)
{
    /*ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);*/
     
    ec_cdev_priv_t * priv;  
    struct inode     *inode = filep->f_inode;
    ec_cdev_t * cdev   = inode->i_private;
    
    priv = kmm_malloc(sizeof(ec_cdev_priv_t));
	if (!priv) {
			EC_MASTER_ERR(cdev->master,
                "Failed to allocate memory for private data structure.\n");
			return -ENOMEM;
	}
			
	priv->cdev = cdev;
	/*priv->ctx.writable = (filep->f_mode & FMODE_WRITE) != 0;*/
	priv->ctx.writable = 1;
	priv->ctx.requested = 0;
	priv->ctx.process_data = NULL;
	priv->ctx.process_data_size = 0;	
		
    filep->f_priv = priv;

#ifdef DEBUG
    EC_MASTER_DBG(cdev->master, 0, "File opened.\n");
#endif

    return 0;
}

/****************************************************************************/

/** Called when the cdev is closed.
 */
int eccdev_release(FAR struct file * filep)
{
    /*ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;
    ec_master_t *master = priv->cdev->master;*/
    
    ec_cdev_priv_t * priv = (ec_cdev_priv_t *) filep->f_priv;
    
    #ifdef DEBUG
    
    ec_master_t *master = priv->cdev->master;
    
    #endif

    if (priv->ctx.process_data) 
    {
        kmm_free(priv->ctx.process_data);
    }

#ifdef DEBUG
    EC_MASTER_DBG(master, 0, "File closed.\n");
#endif

    kmm_free(priv);
    
    return 0;
}

ec_ioctl_module_t module_data;

static int ec_ioctl_module(
        void *arg, /**< ioctl() argument. */
        ec_ioctl_context_t *ctx /**< Private data structure of file handle. */
        )
{
    module_data.ioctl_version_magic = EC_IOCTL_VERSION_MAGIC;
    module_data.master_count = 1;
    
    memcpy( (void *)arg, &(module_data), sizeof(module_data) );

    return 0;
}

int has_scan = 0;

ec_ioctl_master_t master_data;

char IOmap_icosdev[4096];

boolean forceByteAlignment_icosdev = FALSE;

int get_slave_para(char *ifname)
{ 
	int i;
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
        
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP; 
            
    ec_writestate(0);
            
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) 
    {
		master_data.slave_count = ec_slavecount;
		
		for(i = 1; i <= ec_slavecount; ++i)
        {
			 strncpy(&(master_data.soem_slave_array[i - 1].name[0]), ec_slave[i].name, EC_IOCTL_STRING_SIZE - 1);
			 
			 master_data.soem_slave_array[i - 1].name[EC_IOCTL_STRING_SIZE - 1] = 0;
			 
			 master_data.soem_slave_array[i - 1].vendor_id = ec_slave[i].eep_man;
			 master_data.soem_slave_array[i - 1].product_code = ec_slave[i].eep_id;
			 master_data.soem_slave_array[i - 1].revision_number = ec_slave[i].eep_rev;
			 master_data.soem_slave_array[i - 1].serial_number = ec_slave[i].eep_sn;
			 
			 master_data.soem_slave_array[i - 1].alias = ec_slave[i].aliasadr;
			 
			 master_data.soem_slave_array[i - 1].boot_rx_mailbox_offset = ec_slave[i].mbx_ro;
			 master_data.soem_slave_array[i - 1].boot_rx_mailbox_size = ec_slave[i].mbx_rl;
			 master_data.soem_slave_array[i - 1].boot_tx_mailbox_offset = ec_slave[i].mbx_wo;
			 master_data.soem_slave_array[i - 1].boot_tx_mailbox_size = ec_slave[i].mbx_l;
			 
			 master_data.soem_slave_array[i - 1].mailbox_protocols = ec_slave[i].mbx_proto;
			 
			 master_data.soem_slave_array[i - 1].coe_details.enable_sdo = ec_slave[i].CoEdetails & 0x1;
			 master_data.soem_slave_array[i - 1].coe_details.enable_sdo_info = ec_slave[i].CoEdetails & 0x2;
			 master_data.soem_slave_array[i - 1].coe_details.enable_pdo_assign = ec_slave[i].CoEdetails & 0x4;
			 master_data.soem_slave_array[i - 1].coe_details.enable_pdo_configuration = ec_slave[i].CoEdetails & 0x8;
			 master_data.soem_slave_array[i - 1].coe_details.enable_upload_at_startup = ec_slave[i].CoEdetails & 0x10;
			 master_data.soem_slave_array[i - 1].coe_details.enable_sdo_complete_access = ec_slave[i].CoEdetails & 0x20;
			 
			 master_data.soem_slave_array[i - 1].current_on_ebus = ec_slave[i].Ebuscurrent;
			 
			 master_data.soem_slave_array[i - 1].fmmu_bit = ec_slave[i].FMMUunused;
			 
			 master_data.soem_slave_array[i - 1].dc_supported = ec_slave[i].hasdc;
			 
			 master_data.soem_slave_array[i - 1].transmission_delay = ec_slave[i].pdelay;
			 
			 master_data.soem_slave_array[i - 1].next_slave = ec_slave[i].DCnext;
			 
			 master_data.soem_slave_array[i - 1].ports[0].receive_time = ec_slave[i].DCrtA;
			 
			 master_data.soem_slave_array[i - 1].ports[1].receive_time = ec_slave[i].DCrtB;
			 
			 master_data.soem_slave_array[i - 1].ports[2].receive_time = ec_slave[i].DCrtC;
			 
			 master_data.soem_slave_array[i - 1].ports[3].receive_time = ec_slave[i].DCrtD;
		} 
		
		ret = 0;
    } 
    else 
    {
         pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
         
         ret = -1;
    }
    
    ec_slave[0].state = EC_STATE_INIT;
    ec_writestate(0);    
    
    ec_close();
    
    return ret;
}

static int ec_ioctl_master(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< Userspace address to store the results. */
        )
{
 
    int ret = get_slave_para("eth0");
    if(ret)
		return ret;
      
    memcpy( (void *)arg, &master_data, sizeof(master_data) );

    return 0;
}

static int ec_ioctl_slave(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_t data;
    int i;

   
    memcpy(&data, (void *) arg, sizeof(data));
     
    if( (data.position)  >=  (master_data.slave_count) )
    {
		pipe_printf("Slave %u does not exist!\n", data.position);
		
		return -1;
	}

	i = data.position;
	
	strncpy( &(data.name[0]), &(master_data.soem_slave_array[i].name[0]), EC_IOCTL_STRING_SIZE - 1);
			 
	data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
			 
	data.vendor_id = master_data.soem_slave_array[i].vendor_id;
	data.product_code = master_data.soem_slave_array[i].product_code;
	data.revision_number = master_data.soem_slave_array[i].revision_number;
	data.serial_number = master_data.soem_slave_array[i].serial_number;
			 
	data.alias = master_data.soem_slave_array[i].alias;
			 
	data.boot_rx_mailbox_offset = master_data.soem_slave_array[i].boot_rx_mailbox_offset;
	data.boot_rx_mailbox_size = master_data.soem_slave_array[i].boot_rx_mailbox_size;
	data.boot_tx_mailbox_offset = master_data.soem_slave_array[i].boot_tx_mailbox_offset;
	data.boot_tx_mailbox_size = master_data.soem_slave_array[i].boot_tx_mailbox_size;
			 
	data.mailbox_protocols = master_data.soem_slave_array[i].mailbox_protocols = ec_slave[i].mbx_proto;
			 
	data.coe_details.enable_sdo = master_data.soem_slave_array[i].coe_details.enable_sdo;
	data.coe_details.enable_sdo_info = master_data.soem_slave_array[i].coe_details.enable_sdo_info;
	data.coe_details.enable_pdo_assign = master_data.soem_slave_array[i].coe_details.enable_pdo_assign;
	data.coe_details.enable_pdo_configuration = master_data.soem_slave_array[i].coe_details.enable_pdo_configuration;
	data.coe_details.enable_upload_at_startup = master_data.soem_slave_array[i].coe_details.enable_upload_at_startup;
	data.coe_details.enable_sdo_complete_access = master_data.soem_slave_array[i].coe_details.enable_sdo_complete_access;
			 
	data.current_on_ebus = master_data.soem_slave_array[i].current_on_ebus;
			 
	data.fmmu_bit = master_data.soem_slave_array[i].fmmu_bit;
			 
	data.dc_supported = master_data.soem_slave_array[i].dc_supported;
	
	data.transmission_delay = master_data.soem_slave_array[i].transmission_delay;
	
	data.next_slave = master_data.soem_slave_array[i].next_slave;
	
	data.ports[0].receive_time = master_data.soem_slave_array[i].ports[0].receive_time;
			 
	data.ports[1].receive_time = master_data.soem_slave_array[i].ports[1].receive_time;
	
	data.ports[2].receive_time = master_data.soem_slave_array[i].ports[2].receive_time;
			 
	data.ports[3].receive_time = master_data.soem_slave_array[i].ports[3].receive_time;
        
    memcpy((void *) arg, &data, sizeof(data)); 

    return 0;
}

static int ec_ioctl_master_rescan(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    has_scan = 0;
    
    int ret = get_slave_para("eth0");
    if(!ret)
    {
		has_scan = 1;
	}	
    
    return 0;
}

#define MAXBUF 131072
#define MINBUF 128
#define CRCBUF 14

uint8_t ebuf[MAXBUF];

int wkc_sii;

void calc_crc(uint8_t *crc, uint8_t b)
{
   int j;
   *crc ^= b;
   for(j = 0; j <= 7 ; j++ )
   {
     if(*crc & 0x80)
        *crc = (*crc << 1) ^ 0x07;
     else
        *crc = (*crc << 1);
   }
}

uint16_t SIIcrc(uint8_t *buf)
{
   int i;
   uint8_t crc;

   crc = 0xff;
   for( i = 0 ; i <= 13 ; i++ )
   {
      calc_crc(&crc , *(buf++));
   }
   
   return (uint16_t)crc;
}

int eeprom_read(int slave, int start, int length)
{
   int i, ainc = 4;
   uint16_t estat, aiadr;
   uint32_t b4;
   uint64_t b8;
   uint8_t eepctl;

   if((ec_slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* set Eeprom to master */

      estat = 0x0000;
      aiadr = 1 - slave;
      ec_APRD(aiadr, ECT_REG_EEPSTAT, sizeof(estat), &estat, EC_TIMEOUTRET); /* read eeprom status */
      estat = etohs(estat);
      if (estat & EC_ESTAT_R64)
      {
         ainc = 8;
         for (i = start ; i < (start + length) ; i+=ainc)
         {
            b8 = ec_readeepromAP(aiadr, i >> 1 , EC_TIMEOUTEEP);
            ebuf[i] = b8 & 0xFF;
            ebuf[i+1] = (b8 >> 8) & 0xFF;
            ebuf[i+2] = (b8 >> 16) & 0xFF;
            ebuf[i+3] = (b8 >> 24) & 0xFF;
            ebuf[i+4] = (b8 >> 32) & 0xFF;
            ebuf[i+5] = (b8 >> 40) & 0xFF;
            ebuf[i+6] = (b8 >> 48) & 0xFF;
            ebuf[i+7] = (b8 >> 56) & 0xFF;
         }
      }
      else
      {
         for (i = start ; i < (start + length) ; i+=ainc)
         {
            b4 = ec_readeepromAP(aiadr, i >> 1 , EC_TIMEOUTEEP) & 0xFFFFFFFF;
            ebuf[i] = b4 & 0xFF;
            ebuf[i+1] = (b4 >> 8) & 0xFF;
            ebuf[i+2] = (b4 >> 16) & 0xFF;
            ebuf[i+3] = (b4 >> 24) & 0xFF;
         }
      }

      return 0;
   }

   return -1;
}

int icos_sii_read(char *ifname, int slave, int woff, int * p_nwords)
{
	int w;
	uint16_t * wbuf;
	int esize;
	int start;
	int length;
	int is_all;
	int nwords;
	
	nwords = *(p_nwords);
	
	if (ec_init(ifname))
    {
		w = 0x0000;
        wkc_sii = ec_BRD(0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE);      /* detect number of slaves */
        if (wkc_sii > 0)
        {
			ec_slavecount = wkc_sii;
			
			pipe_printf("%d slaves found.\n",ec_slavecount);
			
            if((ec_slavecount >= slave) && (slave > 0))
            {
				eeprom_read(slave, 0x0000, MINBUF);
				
				wbuf = (uint16_t *)&ebuf[0];
				
				#if 0
                pipe_printf("Slave %d data\n", slave);
                pipe_printf(" PDI Control      : %4.4X\n",*(wbuf + 0x00));
                pipe_printf(" PDI Config       : %4.4X\n",*(wbuf + 0x01));
                pipe_printf(" Config Alias     : %4.4X\n",*(wbuf + 0x04));
                pipe_printf(" Checksum         : %4.4X\n",*(wbuf + 0x07));
                pipe_printf("   calculated     : %4.4X\n",SIIcrc(&ebuf[0]));
                pipe_printf(" Vendor ID        : %8.8X\n",*(uint32 *)(wbuf + 0x08));
                pipe_printf(" Product Code     : %8.8X\n",*(uint32 *)(wbuf + 0x0A));
                pipe_printf(" Revision Number  : %8.8X\n",*(uint32 *)(wbuf + 0x0C));
                pipe_printf(" Serial Number    : %8.8X\n",*(uint32 *)(wbuf + 0x0E));
                pipe_printf(" Mailbox Protocol : %4.4X\n",*(wbuf + 0x1C));
                
                #endif
                
				esize = (*(wbuf + 0x3E) + 1) * 128;
				
                if (esize > MAXBUF)
                { 
					esize = MAXBUF;
				}
					
                pipe_printf(" Size             : %4.4X = %d bytes\n",*(wbuf + 0x3E), esize);
                
                #if 0
                
                pipe_printf(" Version          : %4.4X\n",*(wbuf + 0x3F));
                
                #endif
                
                is_all = 0;       
                if(nwords < 0)
                {
					is_all = 1;
				}	
                
                start = woff * 2;
                
                if(!is_all)
                {
					length = nwords * 2;
				}
				else
				{
					length = esize;
				}		
                
                if( (start + length) > esize)
                {
					pipe_printf("sii offset + nwords exceed max buf size\n");
					
					return -1;
				}
				
				if ( (start + length) > MINBUF)
                  eeprom_read(slave, MINBUF, start + length - MINBUF);
                  
                ec_close();
                
                if(is_all)
                {
					*p_nwords = length / 2;
				}	
                
                return 0;   
			}	 
		}
		else
        {
			pipe_printf("No slaves found!\n");
			
			return -1;
        }	
	}	
	else
	{
		pipe_printf("No socket connection on %s\nExcecute as root\n",ifname);
		
		return -1;
	}	
	
	return -1;
}	

static int ec_ioctl_slave_sii_read(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    int retval;
    int start;
	int length;

    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));

    if (!data.nwords)
    {
        pipe_printf("Invalid SII read offset/size %u/%u for slave SII\n", 
			data.offset, data.nwords);
                
        return -EINVAL;
    }
    
    retval = icos_sii_read("eth0", (data.slave_position) + 1, data.offset, (int *)(&(data.nwords)));
    if(retval)
		return retval;
    
    start = (data.offset) * 2;
    length = (data.nwords) * 2;
    
    memcpy( (void /*__user*/ *) data.words, &ebuf[start], length);
    
    memcpy((void *) arg, &data, sizeof(data)); 

    return 0;
}

int eeprom_writealias(int slave, int alias, uint16 crc)
{
   uint16 aiadr;
   uint8 eepctl;
   int ret;

   if((ec_slavecount >= slave) && (slave > 0) && (alias <= 0xffff))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* set Eeprom to master */

      ret = ec_writeeepromAP(aiadr, 0x04 , alias, EC_TIMEOUTEEP);
      if (ret)
        ret = ec_writeeepromAP(aiadr, 0x07 , crc, EC_TIMEOUTEEP);

      return ret;
   }

   return 0;
}

int icos_writealias(char *ifname, int slave, int alias)
{
	int w;
	uint16_t * wbuf;
	int ret = -1;
	
	if (ec_init(ifname))
    {
		w = 0x0000;
        wkc_sii = ec_BRD(0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE);      /* detect number of slaves */
        if (wkc_sii > 0)
        {
			ec_slavecount = wkc_sii;
			
			pipe_printf("%d slaves found.\n",ec_slavecount);
			
            if((ec_slavecount >= slave) && (slave > 0))
            {
				if(!eeprom_read(slave, 0x0000, CRCBUF) ) // read first 14 bytes
                {
                  wbuf = (uint16 *)&ebuf[0];
                  *(wbuf + 0x04) = alias;
                  if(eeprom_writealias(slave, alias, SIIcrc(&ebuf[0])))
                  {
						pipe_printf("Alias %4.4X written successfully to slave %d\n", alias, slave);
						
						ret = 0;
                  }
                  else
                  {
						pipe_printf("Alias not written\n");
						
						ret = -1;
                  }
               }
               else
               {
                  pipe_printf("Could not read slave EEPROM");
                  
                  ret = -1;
               }
            }	 
		}
		else
        {
			pipe_printf("No slaves found!\n");
			
			ret = -1;
        }	
        
        ec_close();
                
        return ret;
	}	
	else
	{
		pipe_printf("No socket connection on %s\nExcecute as root\n",ifname);
		
		return -1;
	}	
	
	return -1;
}	

int eeprom_write(int slave, uint16_t * wbuf, int start, int length)
{
   int i;
   uint16 aiadr;
   uint8 eepctl;

   if((ec_slavecount >= slave) && (slave > 0) && ((start + length) <= MAXBUF))
   {
      aiadr = 1 - slave;
      eepctl = 2;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* force Eeprom from PDI */
      eepctl = 0;
      ec_APWR(aiadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , EC_TIMEOUTRET); /* set Eeprom to master */

      aiadr = 1 - slave;
      for (i = start ; i < (start + length) ; i+=2)
      {
         ec_writeeepromAP(aiadr, i >> 1 , *(wbuf + ( (i - start) >> 1)), EC_TIMEOUTEEP);
      }

      return 0;
   }

   return -1;
}

int icos_writesii(char *ifname, int slave, uint16_t * words, int estart, int size)
{
	int w;
	int ret = -1;
	
	if (ec_init(ifname))
    {
		w = 0x0000;
        wkc_sii = ec_BRD(0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE);      /* detect number of slaves */
        if (wkc_sii > 0)
        {
			ec_slavecount = wkc_sii;
			
			pipe_printf("%d slaves found.\n",ec_slavecount);
			
            if((ec_slavecount >= slave) && (slave > 0))
            {
				if(!eeprom_write(slave, words, estart, size))
				{
					ret = 0;
				}
				else
				{
					pipe_printf("eeprom_write fail!\n");
					
					ret = -1;
				}		
            }	 
		}
		else
        {
			pipe_printf("No slaves found!\n");
			
			ret = -1;
        }	
        
        ec_close();
                
        return ret;
	}	
	else
	{
		pipe_printf("No socket connection on %s\nExcecute as root\n",ifname);
		
		return -1;
	}	
	
	return -1;
}

static int ec_ioctl_slave_sii_write(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    unsigned int byte_size;
    uint16_t *words;
    int retval;
    int estart;
    int size;

    /*if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }*/
    
    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));

    if (!data.nwords) {
        return 0;
    }

    byte_size = sizeof(uint16_t) * data.nwords;
    if (!(words = kmm_malloc(byte_size))) {
        pipe_printf("Failed to allocate %u bytes"
                " for SII contents.\n", byte_size);
        return -ENOMEM;
    }
    
    memcpy( words, (void /*__user*/ *) data.words, byte_size);

    if(data.is_alias)
    {
		retval = icos_writealias("eth0", (data.slave_position) + 1, words[4]);
	}	
	else
	{
		estart = (data.offset) * 2;
		size = (data.nwords) * 2;
		
		retval = icos_writesii("eth0", (data.slave_position) + 1, words, estart, size);
	}	

    kmm_free(words);

    return retval;
}

int wkc_reg;

int icos_readreg(char *ifname, int slave, uint16_t address, size_t size)
{
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
        
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP; 
            
    ec_writestate(0);
            
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) 
    { 
		if(slave > ec_slavecount)
		{
			pipe_printf("slave %d not exist \n", slave);
			
			ret = -1;
			
			goto just_return;
			
		}
		
		ec_config_map(&IOmap_icosdev);
		
		wkc_reg = ecx_FPRD(ecx_context.port, ec_slave[slave].configadr, address, size, &ebuf[0], EC_TIMEOUTRET);
		if(wkc_reg > 0)
		{
			ret = 0;
		}	
		else	
		{
			pipe_printf("slave %d ecx_FPRD fail \n", slave);
			
			ret = -1;
		}	
    } 
    else 
    {
         pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
         
         ret = -1;
    }
    
just_return:

    ec_slave[0].state = EC_STATE_INIT;
    ec_writestate(0);    
    
    ec_close();
    
    return ret;
}	

int icos_writereg(char *ifname, int slave, uint8_t * data, uint16_t address, size_t size)
{
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
        
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP; 
            
    ec_writestate(0);
            
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) 
    { 
		if(slave > ec_slavecount)
		{
			pipe_printf("slave %d not exist \n", slave);
			
			ret = -1;
			
			goto just_return;
			
		}
		
		ec_config_map(&IOmap_icosdev);
	
		wkc_reg = ecx_FPWR(ecx_context.port, ec_slave[slave].configadr, address, size, data, EC_TIMEOUTRET);
		if(wkc_reg > 0)
		{
			ret = 0;
		}	
		else	
		{
			pipe_printf("slave %d ecx_FPRD fail \n", slave);
			
			ret = -1;
		}	
    } 
    else 
    {
         pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
         
         ret = -1;
    }
    
just_return:

    ec_slave[0].state = EC_STATE_INIT;
    ec_writestate(0);    
    
    ec_close();
    
    return ret;
}

static int ec_ioctl_slave_reg_read(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_reg_t io;
    int ret;
    
    memcpy(&io, (void /*__user*/ *) arg, sizeof(io));

    if (!io.size) {
        return 0;
    }
    
    if( (io.size) > MAXBUF)
    {
		pipe_printf("io.size is too large\n");
		
		return -1;
	}	
    
    ret = icos_readreg("eth0", (io.slave_position) + 1, io.address, io.size);
    if(ret)
		return ret;
        
    memcpy( (void /*__user*/ *) io.data, &ebuf[0], io.size );

    return 0;
}


static int ec_ioctl_slave_reg_write(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_reg_t io;
    int ret;

    memcpy(&io, (void /*__user*/ *) arg, sizeof(io));

    if (!io.size) {
        return 0;
    }
    
    ret = icos_writereg("eth0", (io.slave_position) + 1, io.data, io.address, io.size);
    if(ret)
		return ret;
    
    return 0;
}

int wkc_sdo;

int icos_sdo_upload(char *ifname, int slave, uint16_t sdo_index, uint8_t sdo_entry_subindex,
	size_t target_size, uint8_t * target, size_t * p_data_size)
{ 
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
        
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP; 
            
    ec_writestate(0);
            
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) 
    {
		if(slave > ec_slavecount)
		{
			pipe_printf("slave %d not exist \n", slave);
			
			ret = -1;
			
			goto return_from_upload;	
		}
	
		*(p_data_size) = target_size;
		
		wkc_sdo = ec_SDOread(slave, sdo_index, sdo_entry_subindex, FALSE, (int *)(p_data_size), target, EC_TIMEOUTRXM);
        if (wkc_sdo > 0)
        {
			ret = 0;
		}
		else
		{
			pipe_printf("ec_SDOread fail!\n");
			
			ret = -1;
		}	
    } 
    else 
    {
         pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
         
         ret = -1;
    }
    
    return_from_upload :
    
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
    
		ec_close();
    
		return ret;
}

int icos_sdo_download(char *ifname, int slave, uint16_t sdo_index, uint8_t sdo_entry_subindex,
	uint8_t * data, size_t data_size, uint8_t complete_access)
{ 
	int ret;
	boolean CA;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
        
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP; 
            
    ec_writestate(0);
            
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP) 
    {
		if(slave > ec_slavecount)
		{
			pipe_printf("slave %d not exist \n", slave);
			
			ret = -1;
			
			goto return_from_upload;	
		}
	
	
		if(complete_access)
		{
			CA = TRUE;
		}	
		else
		{
			CA = FALSE;
		}	
		
		wkc_sdo = ec_SDOwrite(slave, sdo_index, sdo_entry_subindex, CA, 
			(int)data_size, data, EC_TIMEOUTRXM);
        if (wkc_sdo > 0)
        {
			ret = 0;
		}
		else
		{
			pipe_printf("ec_SDOwrite fail!\n");
			
			ret = -1;
		}	
    } 
    else 
    {
         pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
         
         ret = -1;
    }
    
    return_from_upload :
    
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
    
		ec_close();
    
		return ret;
}

static int ec_ioctl_slave_sdo_upload(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_upload_t data;
    int ret;
 
    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));
    
    ret = icos_sdo_upload("eth0", (data.slave_position) + 1, data.sdo_index, 
		data.sdo_entry_subindex, data.target_size, 
		data.target, &data.data_size);
    
    memcpy((void /*__user*/ *) arg, &data, sizeof(data));    

    return ret;
}

static int ec_ioctl_slave_sdo_download(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_download_t data;
    int retval;

    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));
    
    retval = icos_sdo_download("eth0", (data.slave_position) + 1, data.sdo_index, 
		data.sdo_entry_subindex, data.data, data.data_size, data.complete_access);

	memcpy((void /*__user*/ *) arg, &data, sizeof(data));    
	
    return retval;
}

char IOmap_sdoliist[4096];
ec_ODlistt ODlist;
ec_OElistt OElist;
char usdo[128];

#define OTYPE_VAR               0x0007
#define OTYPE_ARRAY             0x0008
#define OTYPE_RECORD            0x0009

#define ATYPE_Rpre              0x01
#define ATYPE_Rsafe             0x02
#define ATYPE_Rop               0x04
#define ATYPE_Wpre              0x08
#define ATYPE_Wsafe             0x10
#define ATYPE_Wop               0x20

char* otype2string(uint16 otype)
{
    static char str[32] = { 0 };

    switch(otype)
    {
        case OTYPE_VAR:
            sprintf(str, "VAR");
            break;
        case OTYPE_ARRAY:
            sprintf(str, "ARRAY");
            break;
        case OTYPE_RECORD:
            sprintf(str, "RECORD");
            break;
        default:
            sprintf(str, "ot:0x%4.4X", otype);
    }
    return str;
}

char* dtype2string(uint16 dtype, uint16 bitlen)
{
    static char str[32] = { 0 };

    switch(dtype)
    {
        case ECT_BOOLEAN:
            sprintf(str, "BOOLEAN");
            break;
        case ECT_INTEGER8:
            sprintf(str, "INTEGER8");
            break;
        case ECT_INTEGER16:
            sprintf(str, "INTEGER16");
            break;
        case ECT_INTEGER32:
            sprintf(str, "INTEGER32");
            break;
        case ECT_INTEGER24:
            sprintf(str, "INTEGER24");
            break;
        case ECT_INTEGER64:
            sprintf(str, "INTEGER64");
            break;
        case ECT_UNSIGNED8:
            sprintf(str, "UNSIGNED8");
            break;
        case ECT_UNSIGNED16:
            sprintf(str, "UNSIGNED16");
            break;
        case ECT_UNSIGNED32:
            sprintf(str, "UNSIGNED32");
            break;
        case ECT_UNSIGNED24:
            sprintf(str, "UNSIGNED24");
            break;
        case ECT_UNSIGNED64:
            sprintf(str, "UNSIGNED64");
            break;
        case ECT_REAL32:
            sprintf(str, "REAL32");
            break;
        case ECT_REAL64:
            sprintf(str, "REAL64");
            break;
        case ECT_BIT1:
            sprintf(str, "BIT1");
            break;
        case ECT_BIT2:
            sprintf(str, "BIT2");
            break;
        case ECT_BIT3:
            sprintf(str, "BIT3");
            break;
        case ECT_BIT4:
            sprintf(str, "BIT4");
            break;
        case ECT_BIT5:
            sprintf(str, "BIT5");
            break;
        case ECT_BIT6:
            sprintf(str, "BIT6");
            break;
        case ECT_BIT7:
            sprintf(str, "BIT7");
            break;
        case ECT_BIT8:
            sprintf(str, "BIT8");
            break;
        case ECT_VISIBLE_STRING:
            sprintf(str, "VISIBLE_STR(%d)", bitlen);
            break;
        case ECT_OCTET_STRING:
            sprintf(str, "OCTET_STR(%d)", bitlen);
            break;
        default:
            sprintf(str, "dt:0x%4.4X (%d)", dtype, bitlen);
    }
    return str;
}

char* access2string(uint16 access)
{
    static char str[32] = { 0 };

    sprintf(str, "%s%s%s%s%s%s",
            ((access & ATYPE_Rpre) != 0 ? "R" : "_"),
            ((access & ATYPE_Wpre) != 0 ? "W" : "_"),
            ((access & ATYPE_Rsafe) != 0 ? "R" : "_"),
            ((access & ATYPE_Wsafe) != 0 ? "W" : "_"),
            ((access & ATYPE_Rop) != 0 ? "R" : "_"),
            ((access & ATYPE_Wop) != 0 ? "W" : "_"));
    return str;
}

char* SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype)
{
   int i;
   uint8 * my_u8;
   int8 * my_i8;
   uint16 * my_u16;
   int16 * my_i16;
   uint32 * my_u32;
   int32 * my_i32;
   uint64 * my_u64;
   int64 * my_i64;
   float * sr;
   double * dr;
   char es[32];
   int l = sizeof(usdo) - 1;

   memset(&usdo, 0, 128);
   ec_SDOread(slave, index, subidx, FALSE, &l, &usdo, EC_TIMEOUTRXM);
   if (EcatError)
   {
      return ec_elist2string();
   }
   else
   {
      static char str[64] = { 0 };
      switch(dtype)
      {
         case ECT_BOOLEAN:
            my_u8 = (uint8*) &usdo[0];
            if (*my_u8) sprintf(str, "TRUE");
            else sprintf(str, "FALSE");
            break;
         case ECT_INTEGER8:
            my_i8 = (int8*) &usdo[0];
            sprintf(str, "0x%2.2x / %d", *my_i8, *my_i8);
            break;
         case ECT_INTEGER16:
            my_i16 = (int16*) &usdo[0];
            sprintf(str, "0x%4.4x / %d", *my_i16, *my_i16);
            break;
         case ECT_INTEGER32:
         case ECT_INTEGER24:
            my_i32 = (int32*) &usdo[0];
            sprintf(str, "0x%8.8x / %d", *my_i32, *my_i32);
            break;
         case ECT_INTEGER64:
            my_i64 = (int64*) &usdo[0];
            sprintf(str, "0x%16.16"PRIx64" / %"PRId64, *my_i64, *my_i64);
            break;
         case ECT_UNSIGNED8:
            my_u8 = (uint8*) &usdo[0];
            sprintf(str, "0x%2.2x / %u", *my_u8, *my_u8);
            break;
         case ECT_UNSIGNED16:
            my_u16 = (uint16*) &usdo[0];
            sprintf(str, "0x%4.4x / %u", *my_u16, *my_u16);
            break;
         case ECT_UNSIGNED32:
         case ECT_UNSIGNED24:
            my_u32 = (uint32*) &usdo[0];
            sprintf(str, "0x%8.8x / %u", *my_u32, *my_u32);
            break;
         case ECT_UNSIGNED64:
            my_u64 = (uint64*) &usdo[0];
            sprintf(str, "0x%16.16"PRIx64" / %"PRIu64, *my_u64, *my_u64);
            break;
         case ECT_REAL32:
            sr = (float*) &usdo[0];
            sprintf(str, "%f", *sr);
            break;
         case ECT_REAL64:
            dr = (double*) &usdo[0];
            sprintf(str, "%f", *dr);
            break;
         case ECT_BIT1:
         case ECT_BIT2:
         case ECT_BIT3:
         case ECT_BIT4:
         case ECT_BIT5:
         case ECT_BIT6:
         case ECT_BIT7:
         case ECT_BIT8:
            my_u8 = (uint8*) &usdo[0];
            sprintf(str, "0x%x / %u", *my_u8, *my_u8);
            break;
         case ECT_VISIBLE_STRING:
            strcpy(str, "\"");
            strcat(str, usdo);
            strcat(str, "\"");
            break;
         case ECT_OCTET_STRING:
            str[0] = 0x00;
            for (i = 0 ; i < l ; i++)
            {
               sprintf(es, "0x%2.2x ", usdo[i]);
               strcat( str, es);
            }
            break;
         default:
            sprintf(str, "Unknown type");
      }
      return str;
   }
}

void si_sdo(int cnt)
{
    int i, j;

    ODlist.Entries = 0;
    memset(&ODlist, 0, sizeof(ODlist));
    
    if( ec_readODlist(cnt, &ODlist))
    {
        pipe_printf(" CoE Object Description found, %d entries.\n",ODlist.Entries);
        for( i = 0 ; i < ODlist.Entries ; i++)
        {
            uint8_t max_sub;
            char name[128] = { 0 };

            ec_readODdescription(i, &ODlist);
            
            while(EcatError) pipe_printf(" - %s\n", ec_elist2string());
            
            snprintf(name, sizeof(name) - 1, "\"%s\"", ODlist.Name[i]);
            
            if (ODlist.ObjectCode[i] == OTYPE_VAR)
            {
                pipe_printf("0x%04x      %-40s      [%s]\n", ODlist.Index[i], name,
                       otype2string(ODlist.ObjectCode[i]));
            }
            else
            {
                pipe_printf("0x%04x      %-40s      [%s  maxsub(0x%02x / %d)]\n",
                       ODlist.Index[i], name, otype2string(ODlist.ObjectCode[i]),
                       ODlist.MaxSub[i], ODlist.MaxSub[i]);
            }
            
            memset(&OElist, 0, sizeof(OElist));
            ec_readOE(i, &ODlist, &OElist);
            
            while(EcatError) pipe_printf("- %s\n", ec_elist2string());

            if(ODlist.ObjectCode[i] != OTYPE_VAR)
            {
                int l = sizeof(max_sub);
                ec_SDOread(cnt, ODlist.Index[i], 0, FALSE, &l, &max_sub, EC_TIMEOUTRXM);
            }
            else {
                max_sub = ODlist.MaxSub[i];
            }

            for( j = 0 ; j < max_sub+1 ; j++)
            {
                if ((OElist.DataType[j] > 0) && (OElist.BitLength[j] > 0))
                {
                    snprintf(name, sizeof(name) - 1, "\"%s\"", OElist.Name[j]);
                    pipe_printf("    0x%02x      %-40s      [%-16s %6s]      ", j, name,
                           dtype2string(OElist.DataType[j], OElist.BitLength[j]),
                           access2string(OElist.ObjAccess[j]));
                    if ((OElist.ObjAccess[j] & 0x0007))
                    {
                        pipe_printf("%s", SDO2string(cnt, ODlist.Index[i], j, OElist.DataType[j]));
                    }
                    pipe_printf("\n");
                }
            }
        }
    }
    else
    {
        while(EcatError) pipe_printf("%s", ec_elist2string());
    }
}

int icos_sdo_list(char *ifname, int cnt)
{
   int ret = -1;
   int expectedWKC;
   uint16 ssigen;
   
   if (ec_init(ifname))
   {
      pipe_printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */
      if ( ec_config(FALSE, &IOmap_sdoliist) > 0 )
      {
         ec_configdc();
         
         while(EcatError) pipe_printf("%s", ec_elist2string());
         
         pipe_printf("%d slaves found and configured.\n",ec_slavecount);
         
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         
         pipe_printf("Calculated workcounter %d\n", expectedWKC);
      
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 3);
         
         if (ec_slave[0].state != EC_STATE_SAFE_OP )
         {
            pipe_printf("Not all slaves reached safe operational state.\n");
            
            ret = -1;
 
            goto return_from_sdolist;
         }


         ec_readstate();
         
         ssigen = ec_siifind(cnt, ECT_SII_GENERAL);
            
         if (ssigen)
         {
               ec_slave[cnt].CoEdetails = ec_siigetbyte(cnt, ssigen + 0x07);
               ec_slave[cnt].FoEdetails = ec_siigetbyte(cnt, ssigen + 0x08);
               ec_slave[cnt].EoEdetails = ec_siigetbyte(cnt, ssigen + 0x09);
               ec_slave[cnt].SoEdetails = ec_siigetbyte(cnt, ssigen + 0x0a);
               
               if((ec_siigetbyte(cnt, ssigen + 0x0d) & 0x02) > 0)
               {
                  ec_slave[cnt].blockLRW = 1;
                  ec_slave[0].blockLRW++;
               }
               
               ec_slave[cnt].Ebuscurrent = ec_siigetbyte(cnt, ssigen + 0x0e);
               ec_slave[cnt].Ebuscurrent += ec_siigetbyte(cnt, ssigen + 0x0f) << 8;
		}
       
		if(!(ec_slave[cnt].mbx_proto & ECT_MBXPROT_COE))
		{
		   pipe_printf("Not ECT_MBXPROT_COE not print sdo.\n");
		   
		   ret = 0;
		   
		   goto return_from_sdolist;
		}
	   
		si_sdo(cnt);
		
		ret = 0;
     }
     else
     {
         pipe_printf("No slaves found!\n");
         
         ret = -1; 
     }
      
     return_from_sdolist:
      
			ec_slave[0].state = EC_STATE_INIT;
			ec_writestate(0);    
      
			ec_close();
   }
   else
   {
      pipe_printf("No socket connection on %s\nExcecute as root\n", ifname);
      
      ret = -1;
   }
   
   return ret;
}	

int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset)
{
    uint16 idxloop, nidx, subidxloop, rdat, idx, subidx;
    uint8 subcnt;
    int wkc, bsize = 0, rdl;
    int32 rdat2;
    uint8 bitlen, obj_subidx;
    uint16 obj_idx;
    int abs_offset, abs_bit;

    rdl = sizeof(rdat); rdat = 0;
    /* read PDO assign subindex 0 ( = number of PDO's) */
    wkc = ec_SDOread(slave, PDOassign, 0x00, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
    rdat = etohs(rdat);
    /* positive result from slave ? */
    if ((wkc > 0) && (rdat > 0))
    {
        /* number of available sub indexes */
        nidx = rdat;
        bsize = 0;
        /* read all PDO's */
        for (idxloop = 1; idxloop <= nidx; idxloop++)
        {
            rdl = sizeof(rdat); rdat = 0;
            /* read PDO assign */
            wkc = ec_SDOread(slave, PDOassign, (uint8)idxloop, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
            /* result is index of PDO */
            idx = etohs(rdat);
            if (idx > 0)
            {
                rdl = sizeof(subcnt); subcnt = 0;
                /* read number of subindexes of PDO */
                wkc = ec_SDOread(slave,idx, 0x00, FALSE, &rdl, &subcnt, EC_TIMEOUTRXM);
                subidx = subcnt;
                /* for each subindex */
                for (subidxloop = 1; subidxloop <= subidx; subidxloop++)
                {
                    rdl = sizeof(rdat2); rdat2 = 0;
                    /* read SDO that is mapped in PDO */
                    wkc = ec_SDOread(slave, idx, (uint8)subidxloop, FALSE, &rdl, &rdat2, EC_TIMEOUTRXM);
                    rdat2 = etohl(rdat2);
                    /* extract bitlength of SDO */
                    bitlen = LO_BYTE(rdat2);
                    bsize += bitlen;
                    obj_idx = (uint16)(rdat2 >> 16);
                    obj_subidx = (uint8)((rdat2 >> 8) & 0x000000ff);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;
                    ODlist.Slave = slave;
                    ODlist.Index[0] = obj_idx;
                    OElist.Entries = 0;
                    wkc = 0;
                    /* read object entry from dictionary if not a filler (0x0000:0x00) */
                    if(obj_idx || obj_subidx)
                        wkc = ec_readOEsingle(0, obj_subidx, &ODlist, &OElist);
                    pipe_printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
                    if((wkc > 0) && OElist.Entries)
                    {
                        pipe_printf(" %-12s %s\n", dtype2string(OElist.DataType[obj_subidx], bitlen), OElist.Name[obj_subidx]);
                    }
                    else
                        pipe_printf("\n");
                    bitoffset += bitlen;
                };
            };
        };
    };
    /* return total found bitlength (PDO) */
    return bsize;
}

int si_map_sdo(int slave)
{
    int wkc, rdl;
    int retVal = 0;
    uint8 nSM, iSM, tSM;
    int Tsize, outputs_bo, inputs_bo;
    uint8 SMt_bug_add;

    pipe_printf("PDO mapping according to CoE :\n");
    SMt_bug_add = 0;
    outputs_bo = 0;
    inputs_bo = 0;
    rdl = sizeof(nSM); nSM = 0;
    /* read SyncManager Communication Type object count */
    wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, 0x00, FALSE, &rdl, &nSM, EC_TIMEOUTRXM);
    /* positive result from slave ? */
    if ((wkc > 0) && (nSM > 2))
    {
        /* make nSM equal to number of defined SM */
        nSM--;
        /* limit to maximum number of SM defined, if true the slave can't be configured */
        if (nSM > EC_MAXSM)
            nSM = EC_MAXSM;
        /* iterate for every SM type defined */
        for (iSM = 2 ; iSM <= nSM ; iSM++)
        {
            rdl = sizeof(tSM); tSM = 0;
            /* read SyncManager Communication Type */
            wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, iSM + 1, FALSE, &rdl, &tSM, EC_TIMEOUTRXM);
            if (wkc > 0)
            {
                if((iSM == 2) && (tSM == 2)) // SM2 has type 2 == mailbox out, this is a bug in the slave!
                {
                    SMt_bug_add = 1; // try to correct, this works if the types are 0 1 2 3 and should be 1 2 3 4
                    pipe_printf("Activated SM type workaround, possible incorrect mapping.\n");
                }
                if(tSM)
                    tSM += SMt_bug_add; // only add if SMt > 0

                if (tSM == 3) // outputs
                {
                    /* read the assign RXPDO */
                    pipe_printf("  SM%1d outputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ec_slave[slave].outputs - (uint8 *)&IOmap_sdoliist[0]), outputs_bo );
                    outputs_bo += Tsize;
                }
                if (tSM == 4) // inputs
                {
                    /* read the assign TXPDO */
                    pipe_printf("  SM%1d inputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ec_slave[slave].inputs - (uint8 *)&IOmap_sdoliist[0]), inputs_bo );
                    inputs_bo += Tsize;
                }
            }
        }
    }

    /* found some I/O bits ? */
    if ((outputs_bo > 0) || (inputs_bo > 0))
        retVal = 1;
    return retVal;
}

int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset)
{
    uint16 a , w, c, e, er;
    uint8 eectl;
    uint16 obj_idx;
    uint8 obj_subidx;
    uint8 obj_name;
    uint8 obj_datatype;
    uint8 bitlen;
    int totalsize;
    ec_eepromPDOt eepPDO;
    ec_eepromPDOt *PDO;
    int abs_offset, abs_bit;
    char str_name[EC_MAXNAME + 1];

    eectl = ec_slave[slave].eep_pdi;

    totalsize = 0;
    PDO = &eepPDO;
    PDO->nPDO = 0;
    PDO->Length = 0;
    PDO->Index[1] = 0;
    for (c = 0 ; c < EC_MAXSM ; c++) PDO->SMbitsize[c] = 0;
    if (t > 1)
        t = 1;
    PDO->Startpos = ec_siifind(slave, ECT_SII_PDO + t);
    if (PDO->Startpos > 0)
    {
        a = PDO->Startpos;
        w = ec_siigetbyte(slave, a++);
        w += (ec_siigetbyte(slave, a++) << 8);
        PDO->Length = w;
        c = 1;
        /* traverse through all PDOs */
        do
        {
            PDO->nPDO++;
            PDO->Index[PDO->nPDO] = ec_siigetbyte(slave, a++);
            PDO->Index[PDO->nPDO] += (ec_siigetbyte(slave, a++) << 8);
            PDO->BitSize[PDO->nPDO] = 0;
            c++;
            /* number of entries in PDO */
            e = ec_siigetbyte(slave, a++);
            PDO->SyncM[PDO->nPDO] = ec_siigetbyte(slave, a++);
            a++;
            obj_name = ec_siigetbyte(slave, a++);
            a += 2;
            c += 2;
            if (PDO->SyncM[PDO->nPDO] < EC_MAXSM) /* active and in range SM? */
            {
                str_name[0] = 0;
                if(obj_name)
                  ec_siistring(str_name, slave, obj_name);
                if (t)
                  pipe_printf("  SM%1d RXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                else
                  pipe_printf("  SM%1d TXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                pipe_printf("     addr b   index: sub bitl data_type    name\n");
                /* read all entries defined in PDO */
                for (er = 1; er <= e; er++)
                {
                    c += 4;
                    obj_idx = ec_siigetbyte(slave, a++);
                    obj_idx += (ec_siigetbyte(slave, a++) << 8);
                    obj_subidx = ec_siigetbyte(slave, a++);
                    obj_name = ec_siigetbyte(slave, a++);
                    obj_datatype = ec_siigetbyte(slave, a++);
                    bitlen = ec_siigetbyte(slave, a++);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;

                    PDO->BitSize[PDO->nPDO] += bitlen;
                    a += 2;

                    /* skip entry if filler (0x0000:0x00) */
                    if(obj_idx || obj_subidx)
                    {
                       str_name[0] = 0;
                       if(obj_name)
                          ec_siistring(str_name, slave, obj_name);

                       pipe_printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
                       pipe_printf(" %-12s %s\n", dtype2string(obj_datatype, bitlen), str_name);
                    }
                    bitoffset += bitlen;
                    totalsize += bitlen;
                }
                PDO->SMbitsize[ PDO->SyncM[PDO->nPDO] ] += PDO->BitSize[PDO->nPDO];
                c++;
            }
            else /* PDO deactivated because SM is 0xff or > EC_MAXSM */
            {
                c += 4 * e;
                a += 8 * e;
                c++;
            }
            if (PDO->nPDO >= (EC_MAXEEPDO - 1)) c = PDO->Length; /* limit number of PDO entries in buffer */
        }
        while (c < PDO->Length);
    }
    if (eectl) ec_eeprom2pdi(slave); /* if eeprom control was previously pdi then restore */
    return totalsize;
}

int si_map_sii(int slave)
{
    int retVal = 0;
    int Tsize, outputs_bo, inputs_bo;

    pipe_printf("PDO mapping according to SII :\n");

    outputs_bo = 0;
    inputs_bo = 0;
    /* read the assign RXPDOs */
    Tsize = si_siiPDO(slave, 1, (int)(ec_slave[slave].outputs - (uint8*)&IOmap_sdoliist), outputs_bo );
    outputs_bo += Tsize;
    /* read the assign TXPDOs */
    Tsize = si_siiPDO(slave, 0, (int)(ec_slave[slave].inputs - (uint8*)&IOmap_sdoliist), inputs_bo );
    inputs_bo += Tsize;
    /* found some I/O bits ? */
    if ((outputs_bo > 0) || (inputs_bo > 0))
        retVal = 1;
    return retVal;
}

int icos_pdo_list(char *ifname, int cnt)
{
   int ret = -1;
   int expectedWKC;
   uint16 ssigen;
   
   if (ec_init(ifname))
   {
      pipe_printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */
      if ( ec_config(FALSE, &IOmap_sdoliist) > 0 )
      {
         ec_configdc();
         
         while(EcatError) pipe_printf("%s", ec_elist2string());
         
         pipe_printf("%d slaves found and configured.\n",ec_slavecount);
         
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         
         pipe_printf("Calculated workcounter %d\n", expectedWKC);
      
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 3);
         
         if (ec_slave[0].state != EC_STATE_SAFE_OP )
         {
            pipe_printf("Not all slaves reached safe operational state.\n");
            
            ret = -1;
 
            goto return_from_pdolist;
         }


         ec_readstate();
         
         ssigen = ec_siifind(cnt, ECT_SII_GENERAL);
            
         if (ssigen)
         {
               ec_slave[cnt].CoEdetails = ec_siigetbyte(cnt, ssigen + 0x07);
               ec_slave[cnt].FoEdetails = ec_siigetbyte(cnt, ssigen + 0x08);
               ec_slave[cnt].EoEdetails = ec_siigetbyte(cnt, ssigen + 0x09);
               ec_slave[cnt].SoEdetails = ec_siigetbyte(cnt, ssigen + 0x0a);
               
               if((ec_siigetbyte(cnt, ssigen + 0x0d) & 0x02) > 0)
               {
                  ec_slave[cnt].blockLRW = 1;
                  ec_slave[0].blockLRW++;
               }
               ec_slave[cnt].Ebuscurrent = ec_siigetbyte(cnt, ssigen + 0x0e);
               ec_slave[cnt].Ebuscurrent += ec_siigetbyte(cnt, ssigen + 0x0f) << 8;
		}
		
		if (ec_slave[cnt].mbx_proto & ECT_MBXPROT_COE)
             si_map_sdo(cnt);
        else
             si_map_sii(cnt);
             
        ret = 0;
		   
		goto return_from_pdolist;
     }
     else
     {
         pipe_printf("No slaves found!\n");
         
         ret = -1; 
     }
      
     return_from_pdolist:
      
			ec_slave[0].state = EC_STATE_INIT;
			ec_writestate(0);    
      
			ec_close();
   }
   else
   {
      pipe_printf("No socket connection on %s\nExcecute as root\n", ifname);
      
      ret = -1;
   }
   
   return ret;
}

int icos_soeread(char *ifname, int slave, uint8_t drive_no, uint16_t idn,
	size_t mem_size, uint8_t *data, size_t * p_data_size)
{
	int wkc;
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Error: Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
         
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP;
    
    ec_writestate(0);
    
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) != EC_STATE_PRE_OP) 
    {
		pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
	}	
			
    ec_config_map(&IOmap_sdoliist);
    
    ec_configdc();
    
    ec_slave[0].state = EC_STATE_SAFE_OP;
    ec_writestate(0);
    
    if (ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4) != EC_STATE_SAFE_OP) {
		pipe_printf("Failed to change to SAFE_OP state\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
    }
    
	/*ec_readstate(); 
	
	ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);

    ec_slave[0].state = EC_STATE_OPERATIONAL;
    ec_writestate(0);
    
    if ((ec_statecheck(0, EC_STATE_OPERATIONAL, 5 * EC_TIMEOUTSTATE)) != EC_STATE_OPERATIONAL)
    {
		pipe_printf("Failed to change to OP state\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
	}*/
		
	*p_data_size = mem_size;
	
	wkc = ec_SoEread(slave, drive_no, EC_SOE_ATTRIBUTE_B, idn, (int *)(p_data_size), data, EC_TIMEOUTRXM);
	if (wkc > 0)
	{
		ret = 0;
	}	
	else
	{
		pipe_printf("ec_SoEread Failed!\n");
		
		ret = -1;
	}
	
	ec_slave[0].state = EC_STATE_INIT;
	ec_writestate(0);    
      
	ec_close();
		
	return ret;
}	

int icos_soewrite(char *ifname, int slave, uint8_t drive_no, uint16_t idn,
	uint8_t *data, size_t data_size)
{
	int wkc;
	int ret;
	
	if (ec_init(ifname) <= 0)
    {
		pipe_printf("No socket connection on %s\nExecute as root\n",ifname);
			
		return -1;
	}
	
	if (ec_config_init(FALSE) <= 0) 
	{
        pipe_printf("Error: Cannot find EtherCAT slaves!\n");
        
        master_data.slave_count = 0;
        
        ec_close();
         
        return 0;
    }
    
    ec_readstate();
    
    ec_slave[0].state = EC_STATE_PRE_OP;
    
    ec_writestate(0);
    
    if ((ec_statecheck(0, EC_STATE_PRE_OP,  3 * EC_TIMEOUTSTATE)) != EC_STATE_PRE_OP) 
    {
		pipe_printf("State EC_STATE_PRE_OP cannot be changed\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
	}	
			
    ec_config_map(&IOmap_sdoliist);
    
    ec_configdc();
    
    ec_slave[0].state = EC_STATE_SAFE_OP;
    ec_writestate(0);
    
    if (ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4) != EC_STATE_SAFE_OP) {
		pipe_printf("Failed to change to SAFE_OP state\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
    }
    
	/*ec_readstate(); 
	
	ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);

    ec_slave[0].state = EC_STATE_OPERATIONAL;
    ec_writestate(0);
    
    if ((ec_statecheck(0, EC_STATE_OPERATIONAL, 5 * EC_TIMEOUTSTATE)) != EC_STATE_OPERATIONAL)
    {
		pipe_printf("Failed to change to OP state\n");
		
		ec_slave[0].state = EC_STATE_INIT;
		ec_writestate(0);    
      
		ec_close();
		
		return -1;
	}*/
		
    wkc = ec_SoEwrite(slave, drive_no, EC_SOE_VALUE_B, idn, (int)data_size, data, EC_TIMEOUTRXM);
	if (wkc > 0)
	{
		ret = 0;
	}	
	else
	{
		pipe_printf("ec_SoEwrite Failed!\n");
		
		ret = -1;
	}	
	
	ec_slave[0].state = EC_STATE_INIT;
	ec_writestate(0);    
      
	ec_close();
		
	return ret;
}

static int ec_ioctl_slave_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_t data;
    int retval;

    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));
    
    retval = icos_sdo_list("eth0", (data.slave_position) + 1);
    if(retval)
		return retval; 

    return 0;
}

static int ec_ioctl_slave_sync_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_pdo_t data;
    int retval;
    
    memcpy(&data, (void /*__user*/ *) arg, sizeof(data));

    retval = icos_pdo_list("eth0", (data.slave_position) + 1);
    if(retval)
		return retval;

    return 0;
}

static int ec_ioctl_slave_soe_read(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_soe_read_t ioctl;
    int retval = 0;

    memcpy(&ioctl, (void /*__user*/ *) arg, sizeof(ioctl));

	ioctl.error_code = 0;
	
	retval = icos_soeread("eth0", (ioctl.slave_position) + 1, ioctl.drive_no, ioctl.idn,
		ioctl.mem_size, ioctl.data, &ioctl.data_size);
    if (retval) 
    {
        return retval;
    }

    memcpy((void /*__user*/ *) arg, &ioctl, sizeof(ioctl));

    return 0;
}

static int ec_ioctl_slave_soe_write(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_soe_write_t ioctl;
    int retval ;

    memcpy(&ioctl, (void /*__user*/ *) arg, sizeof(ioctl));
    
    ioctl.error_code = 0;

    retval = icos_soewrite("eth0", (ioctl.slave_position) + 1, ioctl.drive_no, ioctl.idn,
		ioctl.data, ioctl.data_size);
	if (retval) 
    {
        return retval;
    }        
    
    memcpy((void /*__user*/ *) arg, &ioctl, sizeof(ioctl));

    return 0;
}

int icos_foe_read(char *ifname, int slave, char *filename, size_t inputsize, 
	uint8_t * filebuffer, size_t * p_data_size)
{
	uint32 data;
	int wkc;
	int ret = -1;
	
	if (ec_init(ifname))
	{
	    if ( ec_config_init(FALSE) > 0 )
		{
			/* wait for all slaves to reach PRE_OP state */
			ec_statecheck(0, EC_STATE_PRE_OP,  EC_TIMEOUTSTATE * 4);

			ec_slave[slave].state = EC_STATE_INIT;
			ec_writestate(slave);
			
			ec_statecheck(slave, EC_STATE_INIT,  EC_TIMEOUTSTATE * 4);

			/* read BOOT mailbox data, master -> slave */
			data = ec_readeeprom(slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
            ec_slave[slave].SM[0].SMlength = (uint16)HI_WORD(data);
			/* store boot write mailbox address */
			ec_slave[slave].mbx_wo = (uint16)LO_WORD(data);
			/* store boot write mailbox size */
			ec_slave[slave].mbx_l = (uint16)HI_WORD(data);

			/* read BOOT mailbox data, slave -> master */
			data = ec_readeeprom(slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
            ec_slave[slave].SM[1].SMlength = (uint16)HI_WORD(data);
			/* store boot read mailbox address */
			ec_slave[slave].mbx_ro = (uint16)LO_WORD(data);
			/* store boot read mailbox size */
			ec_slave[slave].mbx_rl = (uint16)HI_WORD(data);


			/* program SM0 mailbox in for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ec_slave[slave].SM[0], EC_TIMEOUTRET);
			/* program SM1 mailbox out for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ec_slave[slave].SM[1], EC_TIMEOUTRET);

			ec_slave[slave].state = EC_STATE_BOOT;
			ec_writestate(slave);

			/* wait for slave to reach BOOT state */
			if (ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
			{
				pipe_printf("Slave %d state to BOOT.\n", slave);
				
				*p_data_size = inputsize;
				
			    wkc = ec_FOEread(slave, filename, 0, (int *)p_data_size, filebuffer, EC_TIMEOUTSTATE);
			    if(wkc <= 0)
			    {
					pipe_printf("ec_FOEread fail!\n"); 
				}	
				else
				{
					ret = 0;
				}	

				ec_slave[slave].state = EC_STATE_INIT;
				ec_writestate(slave);
			}
		}
		else
		{
			pipe_printf("No slaves found!\n");
		}
		
		ec_close();
	}
	else
	{
		pipe_printf("No socket connection on %s\nExcecute as root\n",ifname);
	}
	
	return ret;
}

int icos_foe_write(char *ifname, int slave, char *filename, size_t inputsize, uint8_t * filebuffer)
{
	uint32 data;
	int wkc;
	int ret = -1;
	
	if (ec_init(ifname))
	{
	    if ( ec_config_init(FALSE) > 0 )
		{
			/* wait for all slaves to reach PRE_OP state */
			ec_statecheck(0, EC_STATE_PRE_OP,  EC_TIMEOUTSTATE * 4);

			ec_slave[slave].state = EC_STATE_INIT;
			ec_writestate(slave);
			
			ec_statecheck(slave, EC_STATE_INIT,  EC_TIMEOUTSTATE * 4);

			/* read BOOT mailbox data, master -> slave */
			data = ec_readeeprom(slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
            ec_slave[slave].SM[0].SMlength = (uint16)HI_WORD(data);
			/* store boot write mailbox address */
			ec_slave[slave].mbx_wo = (uint16)LO_WORD(data);
			/* store boot write mailbox size */
			ec_slave[slave].mbx_l = (uint16)HI_WORD(data);

			/* read BOOT mailbox data, slave -> master */
			data = ec_readeeprom(slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
            ec_slave[slave].SM[1].SMlength = (uint16)HI_WORD(data);
			/* store boot read mailbox address */
			ec_slave[slave].mbx_ro = (uint16)LO_WORD(data);
			/* store boot read mailbox size */
			ec_slave[slave].mbx_rl = (uint16)HI_WORD(data);


			/* program SM0 mailbox in for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ec_slave[slave].SM[0], EC_TIMEOUTRET);
			/* program SM1 mailbox out for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ec_slave[slave].SM[1], EC_TIMEOUTRET);

			ec_slave[slave].state = EC_STATE_BOOT;
			ec_writestate(slave);

			/* wait for slave to reach BOOT state */
			if (ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
			{
				pipe_printf("Slave %d state to BOOT.\n", slave);
				
				wkc = ec_FOEwrite(slave, filename, 0, inputsize , filebuffer, EC_TIMEOUTSTATE);
				
			    if(wkc <= 0)
			    {
					pipe_printf("ec_FOEwrite fail!\n"); 
				}	
				else
				{
					ret = 0;
				}	

				ec_slave[slave].state = EC_STATE_INIT;
				ec_writestate(slave);
			}
		}
		else
		{
			pipe_printf("No slaves found!\n");
		}
		
		ec_close();
	}
	else
	{
		pipe_printf("No socket connection on %s\nExcecute as root\n",ifname);
	}
	
	return ret;
}		

static int ec_ioctl_slave_foe_read(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_foe_t io;
    int ret =  0;

    memcpy(&io, (void /*__user*/ *) arg, sizeof(io));
    
    io.result = 0;
    io.error_code = 0;
    
    ret = icos_foe_read("eth0", (io.slave_position) + 1, io.file_name,
		io.buffer_size, io.buffer, &(io.data_size) );

    memcpy((void /*__user*/ *) arg, &io, sizeof(io));

    return ret;
}

static int ec_ioctl_slave_foe_write(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_foe_t io;
    int ret = 0;
 
    memcpy(&io, (void /*__user*/ *) arg, sizeof(io));
    
    ret = icos_foe_write("eth0", (io.slave_position) + 1, io.file_name, io.buffer_size, io.buffer);

    memcpy((void /*__user*/ *) arg, &io, sizeof(io));

    return ret;
}

int icos_set_ipaddr(char *ifname, int slave, ec_ioctl_eoe_ip_t * p_io)
{
   eoe_param_t ipsettings;
   int ret = -1;
   int chk;
   
   memset(&ipsettings, 0, sizeof(ipsettings));
   
   if(p_io->mac_address_included)
   {
	   ipsettings.mac_set = 1;
	   
	   memcpy(&(ipsettings.mac.addr[0]), &(p_io->mac_address[0]), EOE_ETHADDR_LENGTH);
   }
   
   if(p_io->ip_address_included)
   {
	   ipsettings.ip_set = 1;
	   
	   ipsettings.ip.addr = p_io->ip_address.s_addr;
   }
   
   if(p_io->subnet_mask_included)
   {
	   ipsettings.subnet_set = 1;
	   
	   ipsettings.subnet.addr = p_io->subnet_mask.s_addr;
   }	
   
   if(p_io->gateway_included)
   {
	   ipsettings.default_gateway_set = 1;
	   
	   ipsettings.default_gateway.addr = p_io->gateway.s_addr;
   }  
   
   if(p_io->dns_included)
   {
	   ipsettings.dns_ip_set = 1;
	   
	   ipsettings.dns_ip.addr = p_io->dns.s_addr;
   }	
   
   if(p_io->name_included)
   {
	   ipsettings.dns_name_set = 1;
	   
	   memcpy(&(ipsettings.dns_name[0]), &(p_io->name[0]), EOE_DNS_NAME_LENGTH);
   }   
   
   if (ec_init(ifname))
   {
      /* find and auto-config slaves */
      if ( ec_config_init(FALSE) > 0 )
      {
         ec_config_map(&IOmap_icosdev);

         ec_configdc();

         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

       
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);
  
         ecx_EOEsetIp(&ecx_context, slave, 0, &ipsettings, EC_TIMEOUTRXM);
  
         ec_writestate(0);
         chk = 200;
         
         do
         {
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            pipe_printf("Operational state reached for all slaves.\n");

			ret = 0;
         }
         
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ec_writestate(0);
      }
      else
      {
         pipe_printf("No slaves found!\n");
      }
    
      ec_close();
   }
   else
   {
	  pipe_printf("No socket connection on %s\nExcecute as root\n", ifname);
   }
   
   return ret;	   
}	

static int ec_ioctl_slave_eoe_ip_param(
        ec_master_t *master, /**< EtherCAT master. */
        void *arg /**< ioctl() argument. */
        )
{
    ec_ioctl_eoe_ip_t io;
    int ret;
    
    memcpy(&io, (void /*__user*/ *) arg, sizeof(io));
    
    io.result = 0;
    
    ret = icos_set_ipaddr("eth0", (io.slave_position) + 1, &io);

    memcpy((void /*__user*/ *) arg, &io, sizeof(io));
    
    return ret;
}
 
static long ec_ioctl_nrt
(
        ec_master_t *master, /**< EtherCAT master. */
        ec_ioctl_context_t *ctx, /**< Device context. */
        unsigned int cmd, /**< ioctl() command identifier. */
        void *arg /**< ioctl() argument. */
        )
{
    int ret = -1;

    switch (cmd) {
        case EC_IOCTL_MASTER:
        
            if(!has_scan)
            {
				ret = ec_ioctl_master(master, arg);
				if(!ret)
				{
					has_scan = 1;
				}	
			}
			else
			{
				 memcpy( (void *)arg, &master_data, sizeof(master_data) );
				 
				 ret = 0;
			}		
            
            break;
            
        case EC_IOCTL_SLAVE:
       
            if(!has_scan)
            {
				ret = get_slave_para("eth0");
				if(!ret)
				{
					has_scan = 1;
				}
				else
				{
					break;
				}		
			}
			
		    ret = ec_ioctl_slave(master, arg);	
            
            break;
            
        case EC_IOCTL_SLAVE_SYNC:
            
            break;
        case EC_IOCTL_SLAVE_SYNC_PDO:
        
			ret = ec_ioctl_slave_sync_pdo(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_SYNC_PDO_ENTRY:
            
            break;
        case EC_IOCTL_DOMAIN:
            
            break;
        case EC_IOCTL_DOMAIN_FMMU:
            
            break;
        case EC_IOCTL_DOMAIN_DATA:
            
            break;
        case EC_IOCTL_MASTER_DEBUG:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SLAVE_STATE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SLAVE_SDO:
        
			ret = ec_ioctl_slave_sdo(master, arg);
            
            break;
            
        case EC_IOCTL_SLAVE_SDO_ENTRY:
            
            break;
        case EC_IOCTL_SLAVE_SDO_UPLOAD:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_sdo_upload(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_SDO_DOWNLOAD:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_sdo_download(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_SII_READ:
        
			ret = ec_ioctl_slave_sii_read(master, arg);
            
            break;
            
        case EC_IOCTL_SLAVE_SII_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_sii_write(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_REG_READ:
            
            ret = ec_ioctl_slave_reg_read(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_REG_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_reg_write(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_FOE_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_foe_read(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_FOE_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_foe_write(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_SOE_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_soe_read(master, arg);
            
            break;
        case EC_IOCTL_SLAVE_SOE_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_soe_write(master, arg);
            
            break;
            
/*#ifdef EC_EOE*/
#if 1
        case EC_IOCTL_SLAVE_EOE_IP_PARAM:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            ret = ec_ioctl_slave_eoe_ip_param(master, arg);
            
            break;
#endif
        case EC_IOCTL_CONFIG:
            
            break;
        case EC_IOCTL_CONFIG_PDO:
            
            break;
        case EC_IOCTL_CONFIG_PDO_ENTRY:
            
            break;
        case EC_IOCTL_CONFIG_SDO:
            
            break;
        case EC_IOCTL_CONFIG_IDN:
            
            break;
        case EC_IOCTL_CONFIG_FLAG:
            
            break;
#ifdef EC_EOE
        case EC_IOCTL_CONFIG_EOE_IP_PARAM:
            
            break;
        case EC_IOCTL_EOE_HANDLER:
            
            break;
#endif

        /* Application interface */

        case EC_IOCTL_REQUEST:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_CREATE_DOMAIN:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_CREATE_SLAVE_CONFIG:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SELECT_REF_CLOCK:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_ACTIVATE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_DEACTIVATE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_SYNC:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_WATCHDOG:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_ADD_PDO:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_CLEAR_PDOS:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_ADD_ENTRY:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_CLEAR_ENTRIES:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_REG_PDO_ENTRY:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_REG_PDO_POS:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_DC:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_SDO:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_EMERG_SIZE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_SDO_REQUEST:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_SOE_REQUEST:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_REG_REQUEST:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_VOE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_IDN:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_FLAG:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_STATE_TIMEOUT:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
#ifdef EC_EOE
        case EC_IOCTL_SC_EOE_IP_PARAM:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
#endif
        case EC_IOCTL_DOMAIN_SIZE:
            
            break;
        case EC_IOCTL_DOMAIN_OFFSET:
            
            break;
        case EC_IOCTL_SET_SEND_INTERVAL:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        default:

            ret = -ENOTTY;
            
            break;
    }

    return ret;
}

static long ec_ioctl_both(
        ec_master_t *master, /**< EtherCAT master. */
        ec_ioctl_context_t *ctx, /**< Device context. */
        unsigned int cmd, /**< ioctl() command identifier. */
        void *arg /**< ioctl() argument. */
        )
{
    int ret = -1;

    switch (cmd) {
        case EC_IOCTL_MODULE:
        
			ret = ec_ioctl_module(arg, ctx);
            
            break;
            
        case EC_IOCTL_MASTER_RESCAN:
            
            ret = ec_ioctl_master_rescan(master, arg);
            
            break;
            
        case EC_IOCTL_MASTER_STATE:
            
            break;
        case EC_IOCTL_MASTER_LINK_STATE:
            
            break;
        case EC_IOCTL_SDO_REQUEST_TIMEOUT:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SDO_REQUEST_DATA:
            
            break;
        case EC_IOCTL_SOE_REQUEST_TIMEOUT:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SOE_REQUEST_DATA:
            
            break;
        case EC_IOCTL_REG_REQUEST_DATA:
            
            break;
        case EC_IOCTL_VOE_SEND_HEADER:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_VOE_DATA:
            
            break;
        default:

            /* chain non-rt commands for normal cdev */
            ret = ec_ioctl_nrt(master, ctx, cmd, arg);
            
            break;
    }

    return ret;
}

long ec_ioctl
        (
        ec_master_t *master, /**< EtherCAT master. */
        ec_ioctl_context_t *ctx, /**< Device context. */
        unsigned int cmd, /**< ioctl() command identifier. */
        void *arg /**< ioctl() argument. */
        )
{
    long ret = -1;

    switch (cmd) {
        case EC_IOCTL_SEND:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_RECEIVE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_APP_TIME:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SYNC_REF:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SYNC_REF_TO:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SYNC_SLAVES:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_REF_CLOCK_TIME:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SYNC_MON_QUEUE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SYNC_MON_PROCESS:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_RESET:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_EMERG_POP:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_EMERG_CLEAR:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SC_EMERG_OVERRUNS:
            
            break;
        case EC_IOCTL_SC_STATE:
            
            break;
        case EC_IOCTL_DOMAIN_PROCESS:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_DOMAIN_QUEUE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_DOMAIN_STATE:
            
            break;
        case EC_IOCTL_SDO_REQUEST_INDEX:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SDO_REQUEST_STATE:
            
            break;
        case EC_IOCTL_SDO_REQUEST_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SDO_REQUEST_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SOE_REQUEST_IDN:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SOE_REQUEST_STATE:
            
            break;
        case EC_IOCTL_SOE_REQUEST_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_SOE_REQUEST_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_REG_REQUEST_STATE:
            
            break;
        case EC_IOCTL_REG_REQUEST_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_REG_REQUEST_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_VOE_REC_HEADER:
            
            break;
        case EC_IOCTL_VOE_READ:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_VOE_READ_NOSYNC:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_VOE_WRITE:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        case EC_IOCTL_VOE_EXEC:
            if (!ctx->writable) {
                ret = -EPERM;
                break;
            }
            
            break;
        default:
            ret = ec_ioctl_both(master, ctx, cmd, arg);
            break;
    }

    return ret;
}


/****************************************************************************/

/** Called when an ioctl() command is issued.
 */
int eccdev_ioctl(FAR struct file * filep, int cmd, unsigned long arg)
{
    ec_cdev_priv_t * priv = (ec_cdev_priv_t *) filep->f_priv;

#ifdef DEBUG
    EC_MASTER_DBG(priv->cdev->master, 0,
            "ioctl(filp = 0x%p, cmd = 0x%08x (0x%02x), arg = 0x%lx)\n",
            filep, cmd, _IOC_NR(cmd), arg);
#endif

	void * argp =
            (void *)((uintptr_t)arg);

    return ec_ioctl(priv->cdev->master, &priv->ctx, cmd, argp);
}

int ec_cdev_init(void)
{
    int ret;
    char devpath[20];
    
    g_master.index = 0;
    g_master.p_cdev = &g_cdev;

    g_cdev.master = &g_master;
    
    eccdev_fops.open = &eccdev_open;
	eccdev_fops.close = &eccdev_release;
	eccdev_fops.ioctl = &eccdev_ioctl; 
	eccdev_fops.read = NULL;
	eccdev_fops.write = NULL;
	eccdev_fops.seek = NULL;
	eccdev_fops.mmap = NULL;
	eccdev_fops.truncate = NULL;
	eccdev_fops.poll = NULL;
	
    sprintf(&devpath[0], "%s%d", "EtherCAT", g_master.index);
    
    ret = register_driver(devpath, &eccdev_fops, 0666, &g_cdev);
    if (ret < 0)
    {
		EC_MASTER_ERR((&g_master), "Failed to add character device!\n");
		
		return -EFAULT;
    }

    return ret;
}

/****************************************************************************/

/** Destructor.
 */
void ec_cdev_clear(void)
{
	char devpath[20];
    
    sprintf(&devpath[0], "%s%d", "EtherCAT", g_cdev.master->index);
    
    unregister_driver(&devpath[0]);   
}

