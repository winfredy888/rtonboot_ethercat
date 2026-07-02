#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/compat.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>

#include "rtbulk.h"
#include "rtpub.h"
#include "../../kernel/rtonboot/rtonboot.h"

extern void RTOnBootTraceDetail(char * fmt, ...);
	
extern void RTOnBootTraceInfo(char * fmt, ...);	

extern void RTOnBootTraceWarn(char * fmt, ...);	

extern void RTOnBootTraceError(char * fmt, ...);	

struct cdev * rtbulk_chardev;
struct class * my_rtbulk_class;
static dev_t dev;

#define DP_MAJOR MAJOR(dev)
#define DP_MINOR MINOR(dev)

extern char * get_ptr_rtonboot(void);

int rtbulk_open(struct inode *inode, struct file *filp)
{
		return 0; 
}

int rtbulk_release(struct inode *inode, struct file *filp)
{	
	return 0;
}

static long rtbulk_do_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		void __user *argp = (void __user *)arg;
		struct rtbulk_bufpara bufpara;
		
        switch (cmd) 
		{
			case RTBULK_IOCTL_GET_RTBULK_BUFPARA:
			
				bufpara.total_size = RTONBOOT_BULK_BUFFER_SIZE;
				
				if (copy_to_user(argp, (void *)(&bufpara), sizeof(struct rtbulk_bufpara)))
				{
					RTOnBootTraceError("RTBULK_IOCTL_GET_RTBULK_BUFPARA  copy_to_user fail ");
				
					return -EFAULT;
				}	
	
				break;	

			default:
		
				printk("Winfred Young RTBULK: invalid IOCTL(0x%x)\n",cmd);
				
				RTOnBootTraceError("RTBULK: invalid IOCTL(0x%x)", cmd);
			
				return -EINVAL;
		};
	
		return 0;
}


static long rtbulk_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	
	/*lock_kernel();*/
	ret = rtbulk_do_ioctl(file, cmd, arg);
	/*unlock_kernel();*/
	
	return ret;
}

static int rtbulk_mmap(struct file *filp, struct vm_area_struct *vma)
{    
     unsigned long page;
     unsigned long start = (unsigned long)(vma->vm_start);
     unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
     /*char * ptr_rtonboot;
     uint64_t * ptemp;*/
 
     page = ((unsigned long)(CONFIG_RTONBOOT_LOAD_ADDR)) + ((unsigned long)(OFFSET_TO_RTONBOOT_SHARE_BUFFER))
		+ ((unsigned long)(OFFSET_TO_RTONBOOT_BULK_BUFFER));  
     
     /*if(remap_pfn_range(vma,start,page >> PAGE_SHIFT, size, PAGE_SHARED))*/
     if(remap_pfn_range(vma,start,page >> PAGE_SHIFT, size, vma->vm_page_prot))
     {
		 printk("Winfred Young rtbulk_mmap remap_pfn_range fail\n");
		 
		 RTOnBootTraceError("rtbulk_mmap remap_pfn_range fail");
		 
         return -1;
     } 
     
     /*ptr_rtonboot = get_ptr_rtonboot(); 
     
     ptr_rtonboot +=  OFFSET_TO_RTONBOOT_SHARE_BUFFER; 
     
     ptemp = (uint64_t *)(ptr_rtonboot);
     
     *((uint64_t *)ptemp) = 0x88888888; */
 
     return 0;
}

static const struct file_operations rtbulk_fops =
{
	.owner = THIS_MODULE,
  	.open = rtbulk_open,
  	.release = rtbulk_release,
	.unlocked_ioctl	 = rtbulk_ioctl,
	.mmap = rtbulk_mmap,
};

static char *rtbulk_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
		
	return kasprintf(GFP_KERNEL, "%s", RTBULK_DEVICE_NAME);
}

static int rtbulk_init(void)
{
		int result;

    	rtbulk_chardev = cdev_alloc();
    	if(rtbulk_chardev == NULL)
    	{
        	return -1;
    	}
    
    	if(alloc_chrdev_region(&dev, 0, 1, RTBULK_DEVICE_NAME))
    	{
    		printk("Winfred Young Register char dev error %s\n", RTBULK_DEVICE_NAME);
    	
    		return -1;
    	} 
    
    	cdev_init(rtbulk_chardev, &rtbulk_fops);
    
    	if(cdev_add(rtbulk_chardev, dev, 1))
    	{
        	printk("Winfred Young Add char dev error %s !\n", RTBULK_DEVICE_NAME);

       	 	result = -1;

			goto fail1;
    	}
    
    	my_rtbulk_class = class_create(THIS_MODULE, "rtbulk_class");
    	if(IS_ERR(my_rtbulk_class)) 
    	{
        	printk("Winfred Young Err: failed in creating rtbulk_class.\n");
        
        	result = -1;

			goto fail2;
    	}
    	
    	my_rtbulk_class->devnode = rtbulk_devnode;
    
    	/* register your own device in sysfs, and this will cause udevd to create corresponding device node */
    	device_create(my_rtbulk_class, NULL, dev, NULL, RTBULK_DEVICE_NAME);
  	
		return 0;


	fail2:

		cdev_del(rtbulk_chardev);

	fail1: 

		unregister_chrdev_region(MKDEV(DP_MAJOR,DP_MINOR),1);

		return result;
}

static void rtbulk_exit(void)
{	
		unregister_chrdev_region(MKDEV(DP_MAJOR,DP_MINOR),1);

		cdev_del(rtbulk_chardev);

		device_destroy(my_rtbulk_class, dev);

		class_destroy(my_rtbulk_class);
}


MODULE_AUTHOR("Winfred Young");
MODULE_LICENSE("GPL");

module_init(rtbulk_init);
module_exit(rtbulk_exit);

