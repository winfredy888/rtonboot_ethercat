/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : simple_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "../../../nuttx/libs/libsoem/ethercat.h"

extern int linux_printf(const char *fmt, ...);
extern int linux_vprintf(const char *fmt, va_list ap);

extern int g_simpletest_quit;

#define EC_TIMEOUTMON 500

#define SYNC_CYCLE 125

uint32_t cycletime_ns; 

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
boolean forceByteAlignment = FALSE;

extern uint64_t maincore_arm64_arch_timer_count(void);

extern void append_sleep_lat(uint16_t idx, uint64_t ns);

extern void print_sleep_lat(void);

void simpletest(char *ifname)
{
    int i, j, oloop, iloop, chk;
    needlf = FALSE;
    inOP = FALSE;
    
    cycletime_ns = (uint32_t)1000 * SYNC_CYCLE;

   linux_printf("Starting simple test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      linux_printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */


      if ( ec_config_init(FALSE) > 0 )
      {
         linux_printf("%d slaves found and configured.\n",ec_slavecount);

         if (forceByteAlignment)
         {
            ec_config_map_aligned(&IOmap);
         }
         else
         {
            ec_config_map(&IOmap);
         }

         ec_configdc();

         linux_printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

         oloop = ec_slave[0].Obytes;
         if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = ec_slave[0].Ibytes;
         if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;

         linux_printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

         linux_printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         linux_printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);
         /* request OP state for all slaves */
         ec_writestate(0);
         chk = 200;
         /* wait for all slaves to reach OP state */
         do
         {
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            linux_printf("Operational state reached for all slaves.\n");
            inOP = TRUE;
                /* cyclic loop */
            /*r(i = 1; i <= 10000; i++)
            {
               ec_send_processdata();
               wkc = ec_receive_processdata(EC_TIMEOUTRET);

                    if(wkc >= expectedWKC)
                    {
                        linux_printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            linux_printf(" %2.2x", *(ec_slave[0].outputs + j));
                        }

                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            linux_printf(" %2.2x", *(ec_slave[0].inputs + j));
                        }
                        linux_printf(" T:%"PRId64"\r\n",ec_DCtime);
                        needlf = TRUE;
                    }
                    osal_usleep(5000);

          }*/
          
          osal_timert t;
          
          /*uint64_t old_cnt;
		  uint64_t cur_cnt;
          uint64_t temp;*/
          
		  /*osal_timer_start(&t, SYNC_CYCLE);*/
		  ec_dcsync0(1, TRUE, cycletime_ns, 0);
		  
		  for(i = 1; (i <= 100000) && (!g_simpletest_quit); i++)
		  {
			    if (i % 1000 == 0)
			    { 
					linux_printf("!%d\n", i);
				}	
			    
			    j = j;
			    
				/*while(osal_timer_is_expired(&t) == FALSE);*/
				
				osal_usleep (SYNC_CYCLE);
				
				/*osal_timer_start(&t, SYNC_CYCLE);*/

				/*old_cnt = maincore_arm64_arch_timer_count();*/
				
				ec_send_processdata();
				wkc = ec_receive_processdata(EC_TIMEOUTRET);
				
				/*cur_cnt = maincore_arm64_arch_timer_count();
  
				temp = ((unsigned long long)(cur_cnt - old_cnt) ) * 41;
  
				append_sleep_lat(1, temp);*/
		}
		
		ec_dcsync0(1, FALSE, cycletime_ns, 0); // SYNC0 on slave 1
		osal_timer_start(&t, SYNC_CYCLE);
		
		while(osal_timer_is_expired(&t)==FALSE);
		
		ec_send_processdata();
		ec_receive_processdata(EC_TIMEOUTRET);
		
		linux_printf("emergency stop!\r\n"); 
          
        inOP = FALSE;
        
       }
       else
       {
                linux_printf("Not all slaves reached operational state.\n");
                ec_readstate();
                for(i = 1; i<=ec_slavecount ; i++)
                {
                    if(ec_slave[i].state != EC_STATE_OPERATIONAL)
                    {
                        linux_printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
       }
            linux_printf("\nRequest init state for all slaves\n");
            ec_slave[0].state = EC_STATE_INIT;
            /* request INIT state for all slaves */
            ec_writestate(0);
        }
     else
     {
            linux_printf("No slaves found!\n");
     }
     
     linux_printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
     ec_close();
  }
  else
  {
     linux_printf("No socket connection on %s\nExecute as root\n",ifname);
  }
  
  /*print_sleep_lat();*/
}

OSAL_THREAD_FUNC ecatcheck( void *ptr )
{
    int slave;
    (void)ptr;                  /* Not used */

    while((!!g_simpletest_quit))
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               linux_printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
               if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
               {
                  ec_group[currentgroup].docheckstate = TRUE;
                  if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                  {
                     linux_printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                  {
                     linux_printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     ec_slave[slave].state = EC_STATE_OPERATIONAL;
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state > EC_STATE_NONE)
                  {
                     if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        linux_printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!ec_slave[slave].islost)
                  {
                     /* re-check state */
                     ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                     if (ec_slave[slave].state == EC_STATE_NONE)
                     {
                        ec_slave[slave].islost = TRUE;
                        linux_printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(ec_slave[slave].state == EC_STATE_NONE)
                  {
                     if (ec_recover_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        linux_printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     ec_slave[slave].islost = FALSE;
                     linux_printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!ec_group[currentgroup].docheckstate)
               linux_printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}

int main(int argc, char *argv[])
{
   linux_printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");

   /*if (argc > 1)*/
   if(1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
      /* start cyclic part */
      /*simpletest(argv[1]);*/
      
      simpletest("eth0");
   }
   else
   {
      ec_adaptert * adapter = NULL;
      ec_adaptert * head = NULL;
      linux_printf("Usage: simple_test ifname1\nifname = eth0 for example\n");

      linux_printf("\nAvailable adapters:\n");
      head = adapter = ec_find_adapters ();
      while (adapter != NULL)
      {
         linux_printf ("    - %s  (%s)\n", adapter->name, adapter->desc);
         adapter = adapter->next;
      }
      ec_free_adapters(head);
   }

   linux_printf("End program\n");
   return (0);
}
