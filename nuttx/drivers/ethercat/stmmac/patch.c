#include <nuttx/semaphore.h>
//#include <nuttx/condvar.h>
#include <nuttx/wqueue.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include "patch.h"

extern uint16_t g_is_link_up;

int is_print_pkt = 0;

extern int linux_vprintf(const char *fmt, va_list ap);

int log_level = KERN_DEBUG;

void printk(int level, const char *format, ...) 
{
    va_list args;

	if(level <= log_level)
    {
		va_start(args, format);
		linux_vprintf(format, args);
		va_end(args);
	}	
}

void pr_debug(const char *format, ...) 
{
	va_list args;
	int level = KERN_DEBUG;

	if(level <= log_level)
    {
		va_start(args, format);
		linux_vprintf(format, args);
		va_end(args);
	}
}

void pr_info(const char *format, ...) 
{
	va_list args;
	int level = KERN_INFO;

	if(level <= log_level)
    {
		va_start(args, format);
		linux_vprintf(format, args);
		va_end(args);
	}
}	

void pr_err(const char *format, ...) 
{
	va_list args;
	int level = KERN_ERR;

	if(level <= log_level)
    {
		va_start(args, format);
		linux_vprintf(format, args);
		va_end(args);
	}
}

void pr_warn(const char *format, ...) 
{
	va_list args;
	int level = KERN_WARNING;

	if(level <= log_level)
    {
		va_start(args, format);
		linux_vprintf(format, args);
		va_end(args);
	}
}

void ecdev_set_link(
        ec_device_t *device, /**< EtherCAT device */
        uint8_t state /**< new link state */
        )
{    
    if(state)
    {
		g_is_link_up = 1;
	}
	else
	{
		g_is_link_up = 0;
	}	
}

void ecdev_receive(
        ec_device_t *device, /**< EtherCAT device */
        const void *data, /**< pointer to received data */
        size_t size /**< number of bytes received */
        )
{
	uint8_t * buf = (device->recv_buf) + (device->total_bytes);
	
	if( (device->total_bytes) >= (device->recv_len) )
		return;
	
	if( ((device->total_bytes) + size) > (device->recv_len) )
	{
		size = (device->recv_len) - (device->total_bytes);
	}
	
	memcpy(buf, data, size);
	
	device->total_bytes += size;
}	

ec_device_t g_device;

ec_device_t * ecdev_offer(struct net_driver_s *net_dev, ec_pollfunc_t poll /*,
        struct module_s * module*/)        
{
	g_device.dev = net_dev;
	g_device.poll = poll;
	g_device.open = 0;	
		
	return (&g_device);	
}	        

int ecdev_open(ec_device_t *device /**< EtherCAT device */)
{
	int ret;
	
	if (!device->dev) {
        pr_err("No net_device to open!\n");
        
        return -ENODEV;
    }

    if (device->open) {
        pr_warn("Device already opened!\n");
        
        return 0;
    }

    ret = device->dev->d_ifup(device->dev);
    if (!ret)
        device->open = 1;
	
    return 0;
}

void ecdev_close(ec_device_t *device /**< EtherCAT device */)
{
	int ret;
	
	if (!device->dev) {
		
        pr_err("No device to close!\n");
        
        return;
    }

    if (!device->open) {
		
        pr_warn("Device already closed!\n");
        
        return;
    }

    ret = device->dev->d_ifdown(device->dev);
    if (!ret)
        device->open = 0;

    return;
}

void ecdev_withdraw(ec_device_t *device /**< EtherCAT device */)
{
	device->dev = NULL;
    device->poll = NULL;
    device->open = 0;
}

sem_t rtnl_sem;
 
void rtnl_lock(void)
{
    sem_wait(&rtnl_sem);
}
 
void rtnl_unlock(void)
{
    sem_post(&rtnl_sem);
}
 
void rtnl_init(void)
{
    sem_init(&rtnl_sem, 0, 1);
}

static void timer_work_callback(void * arg);
 
static void timer_callback(wdparm_t arg)
{
    FAR struct timer_wdparm * p_parm = (FAR struct timer_wdparm *)arg;
    
    p_parm->pending = 0;
    
    DEBUGASSERT(work_available(&(p_parm->timer_work)));

	DEBUGVERIFY(work_queue(HPWORK, &(p_parm->timer_work),
			timer_work_callback,
			(void *)p_parm, 0));
}

static void timer_work_callback(void * arg)
{
	FAR struct timer_wdparm * p_parm = (FAR struct timer_wdparm *)arg;
	
	p_parm->timeout = (p_parm->real_callback)(p_parm->real_priv);
 
    if(p_parm->timeout)
    {
		p_parm->pending = 1;
		
		wd_start(&p_parm->wdog, p_parm->timeout, timer_callback, (wdparm_t)(p_parm));
	}
}
 
void timer_init(FAR struct timer_dev_s *dev, unsigned int timeout, real_timer_callback real_callback,
	void * real_priv)
{
    dev->wdparm.timeout = timeout;
    dev->wdparm.real_callback = real_callback;
    dev->wdparm.real_priv = real_priv;
    
    memset(&(dev->wdparm.timer_work), 0, sizeof(struct work_s) );
    
    dev->wdparm.pending = 1;
 
    wd_start(&(dev->wdparm.wdog), dev->wdparm.timeout, timer_callback, (wdparm_t)(&(dev->wdparm)) );
}

void del_timer_sync(FAR struct timer_dev_s *dev)
{
	if(dev->wdparm.pending)
	{
		wd_cancel(&dev->wdparm.wdog);
		
		dev->wdparm.pending = 0;
	}	
}

void mod_timer(FAR struct timer_dev_s *dev, unsigned int new_timeout)
{
	if(dev->wdparm.pending)
	{
		wd_cancel(&dev->wdparm.wdog);
		
		dev->wdparm.pending = 0;
	}	
	
	dev->wdparm.timeout = new_timeout;
	
	dev->wdparm.pending = 1;
    
    wd_start(&dev->wdparm.wdog, dev->wdparm.timeout, timer_callback, (wdparm_t)(&(dev->wdparm)));
}


#define hex_asc_lo(x)	(((x) & 0x0f))
#define hex_asc_hi(x)	(((x) & 0xf0) >> 4)

int is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}	

int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, char *linebuf, size_t linebuflen, bool ascii)
{
	const u8 *ptr = buf;
	int ngroups;
	u8 ch;
	int j, lx = 0;
	int ascii_column;
	int groupsize = 1;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (len > rowsize)		/* limit to one line at a time */
		len = rowsize;
	if (!is_power_of_2(groupsize) || groupsize > 8)
		groupsize = 1;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;

	ngroups = len / groupsize;
	ascii_column = rowsize * 2 + rowsize / groupsize + 1;

	if (!linebuflen)
		goto overflow1;

	if (!len)
		goto nil;

	for (j = 0; j < len; j++) {
			if (linebuflen < lx + 2)
				goto overflow2;
			ch = ptr[j];
			linebuf[lx++] = hex_asc_hi(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = hex_asc_lo(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = ' ';
		
			if (j)
				lx--;
	}
	
	if (!ascii)
		goto nil;

	while (lx < ascii_column) {
		if (linebuflen < lx + 2)
			goto overflow2;
		linebuf[lx++] = ' ';
	}
	
	for (j = 0; j < len; j++) {
		if (linebuflen < lx + 2)
			goto overflow2;
		ch = ptr[j];
		linebuf[lx++] = (isascii(ch) && isprint(ch)) ? ch : '.';
	}
	
nil:
	linebuf[lx] = '\0';
	return lx;
	
overflow2:
	linebuf[lx++] = '\0';
	
overflow1:
	return ascii ? ascii_column + len : (groupsize * 2 + 1) * ngroups - 1;
	
}

void print_hex_dump(const char *prefix_str, int prefix_type,
		    int rowsize, const void *buf, size_t len, bool ascii)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	char linebuf[32 * 3 + 2 + 32 + 1];

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(ptr + i, linelen, rowsize,
				   linebuf, sizeof(linebuf), ascii);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			printk(KERN_DEBUG, "%s%p: %s\n",
			       prefix_str, ptr + i, linebuf);
			break;
		case DUMP_PREFIX_OFFSET:
			printk(KERN_DEBUG, "%s%.8x: %s\n", prefix_str, i, linebuf);
			break;
		default:
			printk(KERN_DEBUG, "%s%s\n", prefix_str, linebuf);
			break;
		}
	}
}

void print_hex_dump_bytes(const char *prefix_str, int prefix_type,
			  const void *buf, size_t len)
{
	print_hex_dump(prefix_str, prefix_type, 16, 
		       buf, len, true);
}

void work_init(struct work_struct *work, void (*func)(void *), void *data) 
{
    work->func = func;
    work->data = data;
    work->pending = 0;
    
    memset( &(work->work_struct_work), 0, sizeof(struct work_s) );
    
    memset( &(work->work_timer), 0, sizeof(struct wdog_s) );
}

static void work_struct_work_callback(void * arg)
{
	struct work_struct * work = (struct work_struct *)arg;
	
	work->func(work);
    work->pending = 0;
}


void work_timeout(wdparm_t arg) {
    struct work_struct * work = (struct work_struct *)arg;
     
    DEBUGASSERT(work_available(&(work->work_struct_work)));

	DEBUGVERIFY(work_queue(HPWORK, &(work->work_struct_work),
			work_struct_work_callback,
			(void *)work, 0));
}

void queue_delayed_work(struct work_struct *work, unsigned long delay) 
{
	if(delay)
    {
		memset( &(work->work_timer), 0, sizeof(struct wdog_s) );
		
		wd_start(&work->work_timer, delay, work_timeout, (wdparm_t)(work));
		
		work->pending = 1;
	}
	else
	{
		work->func(work);
	}	
}

void cancel_delayed_work_sync(struct work_struct *work)
{
	if(work->pending)
	{
		wd_cancel(&work->work_timer);
		
		work->pending = 0;
	}	
}

#define DUMP_BUFFER_ADDR 0xffffffbebed40000ULL

uint32_t count_dump = 0;

char * ptr_dump_begin = (char *)(DUMP_BUFFER_ADDR);

char * ptr_dump_cur;

void append_sleep_lat(uint16_t idx, uint64_t ns)
{
	if(!count_dump)
	{
		ptr_dump_cur = ptr_dump_begin;
	}
	
	*((uint16_t *)ptr_dump_cur) = idx;
	
	ptr_dump_cur += 2;
	
	*((uint64_t *)ptr_dump_cur) = ns;
	
	ptr_dump_cur += 8;
	
	++count_dump;
}	

extern int linux_printf(const char *fmt, ...);

void print_sleep_lat(void)
{
	int i;
	uint16_t idx;
	uint64_t ns;
	char * ptr;
	
	ptr = ptr_dump_begin;
	
	for(i = 0; i < count_dump; ++i)
	{
		idx = *((uint16_t *)ptr);
	
		ptr += 2;
	
		ns = *((uint64_t *)ptr);
		
		ptr += 8;
		
		if(idx == 1)
		{
			linux_printf("*%lld\n", ns);
		}	
		else
		{
			linux_printf("#%lld\n", ns);
		}	
	}	
}

#define DEBUG_BUFFER_ADDR 0xffffffbebfc00000ULL	

uint32_t count_debug = 0;

char * ptr_debug_begin = (char *)(DEBUG_BUFFER_ADDR);

char * ptr_debug_cur;

void append_debug_str(char * str)
{
	int len;
	
	if(!count_debug)
	{
		*((uint32_t *)ptr_debug_begin) = 0;
		
		ptr_debug_cur = ptr_debug_begin + 4;
	}
	
    strcpy(ptr_debug_cur, str);
    
    len = strlen(str);
    
    ptr_debug_cur[len] = 0;
    
    ptr_debug_cur += len + 1;
    
    ++count_debug;
    
    *((uint32_t *)ptr_debug_begin) = count_debug;
}

void append_soem_lat(uint32_t period_ns, uint32_t exec_ns, uint32_t latency_ns)
{
	if(!count_debug)
	{
		*((uint32_t *)ptr_debug_begin) = 0;
		
		ptr_debug_cur = ptr_debug_begin + 4;
	}
	
	*((uint32_t *)ptr_debug_cur) = latency_ns;
	
	ptr_debug_cur += 4;
	
	*((uint32_t *)ptr_debug_cur) = exec_ns;
	
	ptr_debug_cur += 4;
	
	*((uint32_t *)ptr_debug_cur) = period_ns;
	
	ptr_debug_cur += 4;
	
    ++count_debug;
    
    *((uint32_t *)ptr_debug_begin) = count_debug;
}

void print_soem_lat(void)
{
	int i;
	char * ptr;
	uint32_t period_ns, exec_ns, latency_ns;
	uint32_t min_period_ns, min_exec_ns, min_latency_ns;
	uint32_t max_period_ns, max_exec_ns, max_latency_ns;
	uint32_t total_lat;
	
	min_period_ns = 0;
	min_exec_ns = 0;
	min_latency_ns = 0;
	max_period_ns = 0; 
	max_exec_ns = 0;
	max_latency_ns = 0;
	
	ptr = ptr_debug_begin;
	
	total_lat = *((uint32_t *)ptr);
	
	ptr += 4;
	
	for(i = 0; i < total_lat; ++i)
	{
		 latency_ns = *((uint32_t *)ptr);
	
		 ptr += 4;
	
		 exec_ns = *((uint32_t *)ptr);
	
		 ptr += 4;
	
		 period_ns = *((uint32_t *)ptr);
	
		 ptr += 4;
	
		 linux_printf("%d: latency_ns %d, exec_ns %d, period_ns %d\n", 
			i, latency_ns, exec_ns, period_ns);
			
		if(!min_latency_ns)
		{
			min_latency_ns = latency_ns;
		}
		else if(latency_ns < min_latency_ns)
		{
			min_latency_ns = latency_ns;
		}	
		
		if(!max_latency_ns)
		{
			max_latency_ns = latency_ns;
		}
		else if(latency_ns > max_latency_ns)
		{
			max_latency_ns = latency_ns;
		}
		
		if(!min_exec_ns)
		{
			min_exec_ns = exec_ns;
		}
		else if(exec_ns < min_exec_ns)
		{
			min_exec_ns = exec_ns;
		}	
		
		if(!max_exec_ns)
		{
			max_exec_ns = exec_ns;
		}
		else if(exec_ns > max_exec_ns)
		{
			max_exec_ns = exec_ns;
		}
		
		if(!min_period_ns)
		{
			min_period_ns = period_ns;
		}
		else if(period_ns < min_period_ns)
		{
			min_period_ns = period_ns;
		}	
		
		if(!max_period_ns)
		{
			max_period_ns = period_ns;
		}
		else if(period_ns > max_period_ns)
		{
			max_period_ns = period_ns;
		}
	}	
	
	linux_printf("min_latency_ns %d, min_exec_ns %d, min_period_ns %d\n", 
			min_latency_ns, min_exec_ns, min_period_ns);
			
	linux_printf("max_latency_ns %d, max_exec_ns %d, max_period_ns %d\n", 
			max_latency_ns, max_exec_ns, max_period_ns);		
}
