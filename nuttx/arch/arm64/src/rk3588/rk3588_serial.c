#include <nuttx/config.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>
#include <nuttx/init.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/semaphore.h>
#include <nuttx/serial/serial.h>

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "rk3588_serial.h"
#include "arm64_arch_timer.h"
#include "rk3588_boot.h"
#include "arm64_gic.h"

#ifdef USE_SERIALDRIVER

/* UART0 Settings should be same as U-Boot Bootloader */

#ifndef CONFIG_UART0_BAUD
#  define CONFIG_UART0_BAUD 115200
#endif

#ifndef CONFIG_UART0_BITS
#  define CONFIG_UART0_BITS 8
#endif

#ifndef CONFIG_UART0_PARITY
#  define CONFIG_UART0_PARITY 0
#endif

#ifndef CONFIG_UART0_2STOP
#  define CONFIG_UART0_2STOP 0
#endif

#ifndef CONFIG_UART0_RXBUFSIZE
#  define CONFIG_UART0_RXBUFSIZE 256
#endif

#ifndef CONFIG_UART0_TXBUFSIZE
#  define CONFIG_UART0_TXBUFSIZE 256
#endif

/* UART0 is console and ttys0, follows U-Boot Bootloader */

#define CONSOLE_DEV     g_uart0port         /* UART0 is console */
#define TTYS0_DEV       g_uart0port         /* UART0 is ttyS0 */
#define UART0_ASSIGNED  1

#define UART_SCLK 24000000

/* Timeout for UART Busy Wait, in milliseconds */

#define UART_TIMEOUT_MS 100

#define CONFIG_16550_REGINCR 4

/* Register offsets *********************************************************/

#define UART_RBR_INCR          0  /* (DLAB =0) Receiver Buffer Register */
#define UART_THR_INCR          0  /* (DLAB =0) Transmit Holding Register */
#define UART_DLL_INCR          0  /* (DLAB =1) Divisor Latch LSB */
#define UART_DLM_INCR          1  /* (DLAB =1) Divisor Latch MSB */
#define UART_IER_INCR          1  /* (DLAB =0) Interrupt Enable Register */
#define UART_IIR_INCR          2  /* Interrupt ID Register */
#define UART_FCR_INCR          2  /* FIFO Control Register */
#define UART_LCR_INCR          3  /* Line Control Register */
#define UART_MCR_INCR          4  /* Modem Control Register */
#define UART_LSR_INCR          5  /* Line Status Register */
#define UART_MSR_INCR          6  /* Modem Status Register */
#define UART_SCR_INCR          7  /* Scratch Pad Register */
#define UART_USR_INCR          31 /* UART Status Register */

#define UART_RBR_OFFSET        (CONFIG_16550_REGINCR*UART_RBR_INCR)
#define UART_THR_OFFSET        (CONFIG_16550_REGINCR*UART_THR_INCR)
#define UART_DLL_OFFSET        (CONFIG_16550_REGINCR*UART_DLL_INCR)
#define UART_DLM_OFFSET        (CONFIG_16550_REGINCR*UART_DLM_INCR)
#define UART_IER_OFFSET        (CONFIG_16550_REGINCR*UART_IER_INCR)
#define UART_IIR_OFFSET        (CONFIG_16550_REGINCR*UART_IIR_INCR)
#define UART_FCR_OFFSET        (CONFIG_16550_REGINCR*UART_FCR_INCR)
#define UART_LCR_OFFSET        (CONFIG_16550_REGINCR*UART_LCR_INCR)
#define UART_MCR_OFFSET        (CONFIG_16550_REGINCR*UART_MCR_INCR)
#define UART_LSR_OFFSET        (CONFIG_16550_REGINCR*UART_LSR_INCR)
#define UART_MSR_OFFSET        (CONFIG_16550_REGINCR*UART_MSR_INCR)
#define UART_SCR_OFFSET        (CONFIG_16550_REGINCR*UART_SCR_INCR)
#define UART_USR_OFFSET        (CONFIG_16550_REGINCR*UART_USR_INCR)

/* IER (DLAB =0) Interrupt Enable Register */

#define UART_IER_ERBFI               (1 << 0)  /* Bit 0: Enable received data available interrupt */
#define UART_IER_ETBEI               (1 << 1)  /* Bit 1: Enable THR empty interrupt */
#define UART_IER_ELSI                (1 << 2)  /* Bit 2: Enable receiver line status interrupt */
#define UART_IER_EDSSI               (1 << 3)  /* Bit 3: Enable MODEM status interrupt */
                                               /* Bits 4-7: Reserved */
#define UART_IER_ALLIE               (0x0f)

/* IIR Interrupt ID Register */

#define UART_IIR_INTSTATUS           (1 << 0)  /* Bit 0:  Interrupt status (active low) */
#define UART_IIR_INTID_SHIFT         (1)       /* Bits 1-3: Interrupt identification */
#define UART_IIR_INTID_MASK          (7 << UART_IIR_INTID_SHIFT)
#  define UART_IIR_INTID_MSI         (0 << UART_IIR_INTID_SHIFT) /* Modem Status */
#  define UART_IIR_INTID_THRE        (1 << UART_IIR_INTID_SHIFT) /* THR Empty Interrupt */
#  define UART_IIR_INTID_RDA         (2 << UART_IIR_INTID_SHIFT) /* Receive Data Available (RDA) */
#  define UART_IIR_INTID_RLS         (3 << UART_IIR_INTID_SHIFT) /* Receiver Line Status (RLS) */
#  define UART_IIR_INTID_CTI         (6 << UART_IIR_INTID_SHIFT) /* Character Time-out Indicator (CTI) */

                                               /* Bits 4-5: Reserved */
#define UART_IIR_FIFOEN_SHIFT        (6)       /* Bits 6-7: RCVR FIFO interrupt */
#define UART_IIR_FIFOEN_MASK         (3 << UART_IIR_FIFOEN_SHIFT)

/* FCR FIFO Control Register */

#define UART_FCR_FIFOEN              (1 << 0)  /* Bit 0:  Enable FIFOs */
#define UART_FCR_RXRST               (1 << 1)  /* Bit 1:  RX FIFO Reset */
#define UART_FCR_TXRST               (1 << 2)  /* Bit 2:  TX FIFO Reset */
#define UART_FCR_DMAMODE             (1 << 3)  /* Bit 3:  DMA Mode Select */
                                               /* Bits 4-5: Reserved */
#define UART_FCR_RXTRIGGER_SHIFT     (6)       /* Bits 6-7: RX Trigger Level */
#define UART_FCR_RXTRIGGER_MASK      (3 << UART_FCR_RXTRIGGER_SHIFT)
#  define UART_FCR_RXTRIGGER_1       (0 << UART_FCR_RXTRIGGER_SHIFT) /*  Trigger level 0 (1 character) */
#  define UART_FCR_RXTRIGGER_4       (1 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 1 (4 characters) */
#  define UART_FCR_RXTRIGGER_8       (2 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 2 (8 characters) */
#  define UART_FCR_RXTRIGGER_14      (3 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 3 (14 characters) */

/* LCR Line Control Register */

#define UART_LCR_WLS_SHIFT           (0)       /* Bit 0-1: Word Length Select */
#define UART_LCR_WLS_MASK            (3 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_5BIT          (0 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_6BIT          (1 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_7BIT          (2 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_8BIT          (3 << UART_LCR_WLS_SHIFT)
#define UART_LCR_STB                 (1 << 2)  /* Bit 2:  Number of Stop Bits */
#define UART_LCR_PEN                 (1 << 3)  /* Bit 3:  Parity Enable */
#define UART_LCR_EPS                 (1 << 4)  /* Bit 4:  Even Parity Select */
#define UART_LCR_STICKY              (1 << 5)  /* Bit 5:  Stick Parity */
#define UART_LCR_BRK                 (1 << 6)  /* Bit 6: Break Control */
#define UART_LCR_DLAB                (1 << 7)  /* Bit 7: Divisor Latch Access Bit (DLAB) */

/* MCR Modem Control Register */

#define UART_MCR_DTR                 (1 << 0)  /* Bit 0:  DTR Control Source for DTR output */
#define UART_MCR_RTS                 (1 << 1)  /* Bit 1:  Control Source for  RTS output */
#define UART_MCR_OUT1                (1 << 2)  /* Bit 2:  Auxiliary user-defined output 1 */
#define UART_MCR_OUT2                (1 << 3)  /* Bit 3:  Auxiliary user-defined output 2 */
#define UART_MCR_LPBK                (1 << 4)  /* Bit 4:  Loopback Mode Select */
#define UART_MCR_AFCE                (1 << 5)  /* Bit 5:  Auto Flow Control Enable */
                                               /* Bit 6-7: Reserved */

/* LSR Line Status Register */

#define UART_LSR_DR                  (1 << 0)  /* Bit 0:  Data Ready */
#define UART_LSR_OE                  (1 << 1)  /* Bit 1:  Overrun Error */
#define UART_LSR_PE                  (1 << 2)  /* Bit 2:  Parity Error */
#define UART_LSR_FE                  (1 << 3)  /* Bit 3:  Framing Error */
#define UART_LSR_BI                  (1 << 4)  /* Bit 4:  Break Interrupt */
#define UART_LSR_THRE                (1 << 5)  /* Bit 5:  Transmitter Holding Register Empty */
#define UART_LSR_TEMT                (1 << 6)  /* Bit 6:  Transmitter Empty */
#define UART_LSR_RXFE                (1 << 7)  /* Bit 7:  Error in RX FIFO (RXFE) */

/* SCR Scratch Pad Register */

#define UART_SCR_MASK                (0xff)    /* Bits 0-7: SCR data */

/* USR UART Status Register */

#define UART_USR_BUSY                (1 << 0)  /* Bit 0: UART Busy */



/***************************************************************************
 * Private Types
 ***************************************************************************/

/* A64 UART Configuration */

struct a64_uart_config
{
  unsigned long uart;  /* UART Base Address */
};

/* A64 UART Device Data */

struct a64_uart_data
{
  uint32_t baud_rate;  /* UART Baud Rate */
  uint32_t ier;        /* Saved IER value */
  uint8_t  parity;     /* 0=none, 1=odd, 2=even */
  uint8_t  bits;       /* Number of bits (7 or 8) */
  bool     stopbits2;  /* true: Configure with 2 stop bits instead of 1 */
};

/* A64 UART Port */

struct a64_uart_port_s
{
  struct a64_uart_data data;     /* UART Device Data */
  struct a64_uart_config config; /* UART Configuration */
  unsigned int irq_num;          /* UART IRQ Number */
  bool is_console;               /* 1 if this UART is console */
};

static void a64_uart_rxint(struct uart_dev_s *dev, bool enable);
static void a64_uart_txint(struct uart_dev_s *dev, bool enable);

static uint32_t a64_uart_divisor(uint32_t baud)
{
  DEBUGASSERT(baud != 0);
  return UART_SCLK / (baud << 4);
}

extern int rtos_printf(const char *fmt, ...);

static int a64_uart_irq_handler(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = (struct uart_dev_s *)arg;
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;
  uint32_t status;
  int passes;

  DEBUGASSERT(dev != NULL && dev->priv != NULL);
  
  for (passes = 0; passes < 256; passes++)
    {
      /* Get the current UART status and check for loop
       * termination conditions
       */

	  status = getreg32( (config->uart) + UART_IIR_OFFSET );

      /* The UART_IIR_INTSTATUS bit should be zero if there are pending
       * interrupts
       */

      if ((status & UART_IIR_INTSTATUS) != 0)
        {
          /* Break out of the loop when there is no longer a
           * pending interrupt
           */

          break;
        }

      /* Handle the interrupt by its interrupt ID field */

      switch (status & UART_IIR_INTID_MASK)
        {
          /* Handle incoming, receive bytes (with or without timeout) */

          case UART_IIR_INTID_RDA:
          case UART_IIR_INTID_CTI:
            {
              uart_recvchars(dev);
              break;
            }

          /* Handle outgoing, transmit bytes */

          case UART_IIR_INTID_THRE:
            {
              uart_xmitchars(dev);
              break;
            }

          /* Just clear modem status interrupts (UART1 only) */

          case UART_IIR_INTID_MSI:
            {
              /* Read the modem status register (MSR) to clear */

			  status = getreg32( (config->uart) + UART_MSR_OFFSET );
              
              break;
            }

          /* Just clear any line status interrupts */

          case UART_IIR_INTID_RLS:
            {
              /* Read the line status register (LSR) to clear */

              status = getreg32( (config->uart) + UART_LSR_OFFSET );
              
              break;
            }

          /* There should be no other values */

          default:
            {
              _err("ERROR: Unexpected IIR: %02" PRIx32 "\n", status);
              
              break;
            }
        }
    }

  return OK;

}

static int a64_uart_wait(struct uart_dev_s *dev)
{
  struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;
  int i;
  
  for (i = 0; i < UART_TIMEOUT_MS; i++)
    {
      uint32_t status = getreg32( (config->uart) + UART_USR_OFFSET);

      if ((status & UART_USR_BUSY) == 0)
        {
          return OK;
        }

      up_mdelay(1);
    }

  _err("UART timeout\n");
  return ERROR;
}

extern int rtos_printf(const char *fmt, ...);

static int a64_uart_setup(struct uart_dev_s *dev)
{		
#ifndef CONFIG_SUPPRESS_UART_CONFIG

  struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;
  struct a64_uart_data *data = &port->data;
  uint16_t dl;
  uint32_t lcr;
  int ret;

  DEBUGASSERT(data != NULL);
  
  /* Clear fifos */
  
   putreg32( (UART_FCR_RXRST | UART_FCR_TXRST), (config->uart) + UART_FCR_OFFSET );

  /* Set trigger */

  putreg32( (UART_FCR_FIFOEN | UART_FCR_RXTRIGGER_8) , (config->uart) + UART_FCR_OFFSET );

  data->ier = getreg32( (config->uart) + UART_IER_OFFSET );
  
    /* Set up the LCR */

  lcr = 0;
  switch (data->bits)
    {
      case 5 :
        lcr |= UART_LCR_WLS_5BIT;
        break;

      case 6 :
        lcr |= UART_LCR_WLS_6BIT;
        break;

      case 7 :
        lcr |= UART_LCR_WLS_7BIT;
        break;

      default:
      case 8 :
        lcr |= UART_LCR_WLS_8BIT;
        break;
    }

  if (data->stopbits2)
    {
      lcr |= UART_LCR_STB;
    }

  if (data->parity == 1)
    {
      lcr |= UART_LCR_PEN;
    }
  else if (data->parity == 2)
    {
      lcr |= (UART_LCR_PEN | UART_LCR_EPS);
    }

	  /* Set DLAB when UART is not busy */

  ret = a64_uart_wait(dev);

  if (ret < 0)
    {
      _err("UART wait failed, ret=%d\n", ret);
      return ret;
    }
    
    /* Enter DLAB=1 */
  
  putreg32( lcr | UART_LCR_DLAB, (config->uart) +  UART_LCR_OFFSET );
  
  /* Set the BAUD divisor */

  dl = a64_uart_divisor(data->baud_rate);
  putreg8(dl >> 8,   (config->uart) + UART_DLM_OFFSET);
  putreg8(dl & 0xff, (config->uart) + UART_DLL_OFFSET);

  
  ret = a64_uart_wait(dev);

  if (ret < 0)
    {
      _err("UART wait failed, ret=%d\n", ret);
      return ret;
    }

	  /* Clear DLAB */
  
  putreg32(lcr, config->uart + UART_LCR_OFFSET );


  /* Configure the FIFOs */
                   
  putreg32(   (UART_FCR_RXTRIGGER_8 | UART_FCR_TXRST | UART_FCR_RXRST |
                    UART_FCR_FIFOEN), config->uart + UART_FCR_OFFSET );                  

	#if defined(CONFIG_SERIAL_IFLOWCONTROL) || defined(CONFIG_SERIAL_OFLOWCONTROL)
		#  warning Missing logic
	#endif	

#endif

	return OK;
	
}

static void a64_uart_shutdown(struct uart_dev_s *dev)
{
  /* Disable the Receive and Transmit Interrupts */

  a64_uart_rxint(dev, false);
  a64_uart_txint(dev, false);
}

static int a64_uart_attach(struct uart_dev_s *dev)
{
  int ret;
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;

  DEBUGASSERT(port != NULL);

  /* Attach UART Interrupt Handler */

  ret = irq_attach(port->irq_num, a64_uart_irq_handler, dev);

  /* Set Interrupt Priority in Generic Interrupt Controller v2 */

  arm64_gic_irq_set_priority(port->irq_num, 0, IRQ_TYPE_LEVEL);

  /* Enable UART Interrupt */

  if (ret == OK)
    {
      up_enable_irq(port->irq_num);
    }
  else
    {
      _err("IRQ attach failed, ret=%d\n", ret);
    }

  return ret;
}

static void a64_uart_detach(struct uart_dev_s *dev)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;

  DEBUGASSERT(port != NULL);

  /* Disable UART Interrupt */

  up_disable_irq(port->irq_num);

  /* Detach UART Interrupt Handler */

  irq_detach(port->irq_num);
}

static int a64_uart_ioctl(struct file *filep, int cmd, unsigned long arg)
{
  int ret = OK;

  UNUSED(filep);
  UNUSED(arg);

  switch (cmd)
    {
      case TIOCSBRK:  /* BSD compatibility: Turn break on, unconditionally */
      case TIOCCBRK:  /* BSD compatibility: Turn break off, unconditionally */
      default:
        {
          ret = -ENOTTY;
          break;
        }
    }

  return ret;
}

static int a64_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;
  uint32_t rbr;

  *status = getreg8( (config->uart) + UART_LSR_OFFSET);
  rbr     = getreg8( (config->uart) + UART_RBR_OFFSET );
  
  return rbr;
}

static void a64_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;

  /* Write to Interrupt Enable Register (UART_IER) */

  if (enable)
    {
      /* Set ERBFI bit (Enable Rx Data Available Interrupt) */

      modreg8( UART_IER_ERBFI, UART_IER_ERBFI, (config->uart) + UART_IER_OFFSET );
    }
  else
    {
      /* Clear ERBFI bit (Disable Rx Data Available Interrupt) */

      modreg8(0, UART_IER_ERBFI, (config->uart) + UART_IER_OFFSET );
    }
}

static bool a64_uart_rxavailable(struct uart_dev_s *dev)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;

  /* Data Ready Bit (Line Status Register) is 1 if Rx Data is ready */

  return getreg8( (config->uart) + UART_LSR_OFFSET ) & UART_LSR_DR;
}

static void a64_uart_send(struct uart_dev_s *dev, int ch)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;
  
  /* Write char to Transmit Holding Register (UART_THR) */

  putreg8(ch, (config->uart) + UART_THR_OFFSET );
}

static void a64_uart_txint(struct uart_dev_s *dev, bool enable)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;

  /* Write to Interrupt Enable Register (UART_IER) */

  if (enable)
    {
      /* Set ETBEI bit (Enable Tx Holding Register Empty Interrupt) */

      modreg8(UART_IER_ETBEI, UART_IER_ETBEI, (config->uart) + UART_IER_OFFSET );
    }
  else
    {
      /* Clear ETBEI bit (Disable Tx Holding Register Empty Interrupt) */

      modreg8(0, UART_IER_ETBEI, (config->uart) + UART_IER_OFFSET );
    }
}

static bool a64_uart_txready(struct uart_dev_s *dev)
{
  const struct a64_uart_port_s *port = (struct a64_uart_port_s *)dev->priv;
  const struct a64_uart_config *config = &port->config;

  /* Tx FIFO is ready if THRE Bit is 1 (Tx Holding Register Empty) */

  return (getreg8( ((config->uart) + UART_LSR_OFFSET) ) & UART_LSR_THRE) != 0;
}

static bool a64_uart_txempty(struct uart_dev_s *dev)
{
  /* Tx FIFO is empty if Tx FIFO is not full (for now) */

  return a64_uart_txready(dev);
}

static void a64_uart_wait_send(struct uart_dev_s *dev, int ch)
{
  DEBUGASSERT(dev != NULL);
  
  while (!a64_uart_txready(dev));
  a64_uart_send(dev, ch);
}

/* UART Operations for Serial Driver */

static const struct uart_ops_s g_uart_ops =
{
  .setup    = a64_uart_setup,
  .shutdown = a64_uart_shutdown,
  .attach   = a64_uart_attach,
  .detach   = a64_uart_detach,
  .ioctl    = a64_uart_ioctl,
  .receive  = a64_uart_receive,
  .rxint    = a64_uart_rxint,
  .rxavailable = a64_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol    = NULL,
#endif
  .send     = a64_uart_send,
  .txint    = a64_uart_txint,
  .txready  = a64_uart_txready,
  .txempty  = a64_uart_txempty,
};

/* UART0 Port State */

static struct a64_uart_port_s g_uart0priv =
{
  .data   =
    {
      .baud_rate  = CONFIG_UART0_BAUD,
      .parity     = CONFIG_UART0_PARITY,
      .bits       = CONFIG_UART0_BITS,
      .stopbits2  = CONFIG_UART0_2STOP
    },

  .config =
    {
      .uart       = RK3588_UART0_ADDR
    },

    .irq_num      = RK3588_UART0_IRQ,
    .is_console   = 1
};

/* UART0 I/O Buffers */

static char g_uart0rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0txbuffer[CONFIG_UART0_TXBUFSIZE];

/* UART0 Port Definition */

static struct uart_dev_s g_uart0port =
{
  .recv  =
    {
      .size   = CONFIG_UART0_RXBUFSIZE,
      .buffer = g_uart0rxbuffer,
    },

  .xmit  =
    {
      .size   = CONFIG_UART0_TXBUFSIZE,
      .buffer = g_uart0txbuffer,
    },

  .ops   = &g_uart_ops,
  .priv  = &g_uart0priv,
};

void arm64_earlyserialinit(void)
{
  int ret;

  /* NOTE: This function assumes that UART0 low level hardware configuration
   * -- including all clocking and pin configuration -- was performed
   * earlier by U-Boot Bootloader.
   */

#ifdef CONSOLE_DEV
  /* Enable the console at UART0 */

  CONSOLE_DEV.isconsole = true;
  a64_uart_setup(&CONSOLE_DEV);
#endif

  UNUSED(ret);
}

extern int is_main_first_kick_received;
extern void linux_serial_putc(int ch);

int up_putc(int ch)
{
	if(is_main_first_kick_received >= 2)
	{
		linux_serial_putc(ch);
		
		return ch;
	}	
	
#ifdef CONSOLE_DEV
  struct uart_dev_s *dev = &CONSOLE_DEV;

  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      a64_uart_wait_send(dev, '\r');
    }

  a64_uart_wait_send(dev, ch);
#endif
  return ch;
}

void arm64_serialinit(void)
{
#ifdef CONSOLE_DEV
  int ret;

  ret = uart_register("/dev/console", &CONSOLE_DEV);
  if (ret < 0)
    {
      _err("Register /dev/console failed, ret=%d\n", ret);
    }

  ret = uart_register("/dev/ttyS0", &TTYS0_DEV);

  if (ret < 0)
    {
      _err("Register /dev/ttyS0 failed, ret=%d\n", ret);
    }

#endif

	return;
}


#endif /* USE_SERIALDRIVER */

