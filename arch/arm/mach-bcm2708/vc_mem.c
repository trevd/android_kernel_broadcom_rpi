/*****************************************************************************
* Copyright 2010 - 2011 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>




#include "mach/vc_mem.h"
#include <mach/vcio.h>

#define DRIVER_NAME  "vc-mem"

// Device (/dev) related variables
static dev_t vc_mem_devnum = 0;
static struct class *vc_mem_class = NULL;
static struct cdev vc_mem_cdev;
static int vc_mem_inited = 0;

#ifdef CONFIG_DEBUG_FS
static struct dentry *vc_mem_debugfs_entry;
#endif

/*
 * Videocore memory addresses and size
 *
 * Drivers that wish to know the videocore memory addresses and sizes should
 * use these variables instead of the MM_IO_BASE and MM_ADDR_IO defines in
 * headers. This allows the other drivers to not be tied down to a a certain
 * address/size at compile time.
 *
 * In the future, the goal is to have the videocore memory virtual address and
 * size be calculated at boot time rather than at compile time. The decision of
 * where the videocore memory resides and its size would be in the hands of the
 * bootloader (and/or kernel). When that happens, the values of these variables
 * would be calculated and assigned in the init function.
 */
// in the 2835 VC in mapped above ARM, but ARM has full access to VC space
unsigned long mm_vc_mem_phys_addr = 0x00000000;
unsigned int mm_vc_mem_size = 0;
unsigned int mm_vc_mem_base = 0;

unsigned int mm_vc_mem_load;
EXPORT_SYMBOL(mm_vc_mem_phys_addr);
EXPORT_SYMBOL(mm_vc_mem_size);
EXPORT_SYMBOL(mm_vc_mem_base);

static uint phys_addr = 0;
static uint mem_size = 0;
static uint mem_base = 0;


/****************************************************************************
*
*   vc_mem_open
*
***************************************************************************/

static int
vc_mem_open(struct inode *inode, struct file *file)
{
	(void) inode;
	(void) file;

	pr_debug("%s: called file = 0x%p\n", __func__, file);

	return 0;
}

/****************************************************************************
*
*   vc_mem_release
*
***************************************************************************/

static int
vc_mem_release(struct inode *inode, struct file *file)
{
	(void) inode;
	(void) file;

	pr_debug("%s: called file = 0x%p\n", __func__, file);

	return 0;
}

/****************************************************************************
*
*   vc_mem_get_size
*
***************************************************************************/
static void vc_mem_get_size(void)
{

    u32 p[8];
    p[0] = 32; //  size = sizeof u32 * length of p
    p[1] = VCMSG_PROCESS_REQUEST; // process request
    p[2] = VCMSG_GET_VC_MEMORY; // (the tag id)
    p[3] = 8; // (size of the response buffer)
    p[4] = 0; // (size of the request data)
    p[5] = 0; //  This is where the base address is returned to
    p[6] = 0; //  This is where the size is returned to
    p[7] = VCMSG_PROPERTY_END; // end tag
    bcm_mailbox_property(&p, p[0]);
    pr_info("ioctl_vc_get_vc_memory p[0]=0x%x p[1]=0x%x p[2]=0x%x p[3]=0x%x p[4]=0x%x p[5]=0x%x p[6]=0x%x\n",p[0],p[1],p[2],p[3],p[4],p[5],p[6]);
    if ( p[1] == VCMSG_REQUEST_SUCCESSFUL ){
	mm_vc_mem_base = p[5];
	mm_vc_mem_size = p[6];
	mm_vc_mem_load = mm_vc_mem_size;
    }

}


/****************************************************************************
*
*   vc_mem_get_current_size
*
***************************************************************************/
int vc_mem_get_current_size(void)
{
	vc_mem_get_size();
	printk(KERN_INFO "vc-mem: current size check = 0x%08x (%u MiB)\n",
	       mm_vc_mem_size, mm_vc_mem_size / (1024 * 1024));
	return mm_vc_mem_size;
}


EXPORT_SYMBOL_GPL(vc_mem_get_current_size);
/****************************************************************************
*
*   vc_mem_get_current_base
*
***************************************************************************/

int vc_mem_get_current_base(void)
{
	vc_mem_get_size();
	printk(KERN_INFO "vc-mem: current base check = 0x%08x (%u MiB)\n",
	       mm_vc_mem_base, mm_vc_mem_base / (1024 * 1024));
	return mm_vc_mem_base;
}

EXPORT_SYMBOL_GPL(vc_mem_get_current_base);

/****************************************************************************
*
*   vc_mem_get_current_load
*
***************************************************************************/

int vc_mem_get_current_load(void)
{
	vc_mem_get_size();
	printk(KERN_INFO "vc-mem: current load check = 0x%08x (%u MiB)\n",
	       mm_vc_mem_load, mm_vc_mem_load / (1024 * 1024));
	return mm_vc_mem_load;
}

EXPORT_SYMBOL_GPL(vc_mem_get_current_load);

/****************************************************************************
*
*   vc_mem_ioctl
*
***************************************************************************/

static long
vc_mem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	(void) cmd;
	(void) arg;

	pr_debug("%s: called file = 0x%p\n", __func__, file);

	switch (cmd) {
	case VC_MEM_IOC_MEM_PHYS_ADDR:
		{
			pr_debug("%s: VC_MEM_IOC_MEM_PHYS_ADDR=0x%p\n",
				__func__, (void *) mm_vc_mem_phys_addr);

			if (copy_to_user((void *) arg, &mm_vc_mem_phys_addr,
					 sizeof (mm_vc_mem_phys_addr)) != 0) {
				rc = -EFAULT;
			}
			break;
		}
	case VC_MEM_IOC_MEM_SIZE:
		{
			// Get the videocore memory size first
			vc_mem_get_size();

			pr_debug("%s: VC_MEM_IOC_MEM_SIZE=%u\n", __func__,
				mm_vc_mem_size);

			if (copy_to_user((void *) arg, &mm_vc_mem_size,
					 sizeof (mm_vc_mem_size)) != 0) {
				rc = -EFAULT;
			}
			break;
		}
	case VC_MEM_IOC_MEM_BASE:
		{
			/* Get the videocore memory size first */
			vc_mem_get_size();

			pr_debug("%s: VC_MEM_IOC_MEM_BASE=%u", __func__,
				mm_vc_mem_base);

			if (copy_to_user((void *)arg, &mm_vc_mem_base,
					 sizeof(mm_vc_mem_base)) != 0) {
				rc = -EFAULT;
			}
			break;
		}
	case VC_MEM_IOC_MEM_LOAD:
		{
			/* Get the videocore memory size first */
			vc_mem_get_size();

			pr_debug("%s: VC_MEM_IOC_MEM_LOAD=%u", __func__,
				mm_vc_mem_load);

			if (copy_to_user((void *)arg, &mm_vc_mem_load,
					 sizeof(mm_vc_mem_load)) != 0) {
				rc = -EFAULT;
			}
			break;
		}
	default:
		{
			return -ENOTTY;
		}
	}
	pr_debug("%s: file = 0x%p returning %d\n", __func__, file, rc);

	return rc;
}

/****************************************************************************
*
*   vc_mem_mmap
*
***************************************************************************/

static int
vc_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int rc = 0;
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	pr_debug("%s: vm_start = 0x%08lx vm_end = 0x%08lx vm_pgoff = 0x%08lx\n",
		__func__, (long) vma->vm_start, (long) vma->vm_end,
		(long) vma->vm_pgoff);

	if (offset + length > mm_vc_mem_size) {
		pr_err("%s: length %ld is too big\n", __func__, length);
		return -EINVAL;
	}
	// Do not cache the memory map
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	rc = remap_pfn_range(vma, vma->vm_start,
			     (mm_vc_mem_phys_addr >> PAGE_SHIFT) +
			     vma->vm_pgoff, length, vma->vm_page_prot);
	if (rc != 0) {
		pr_err("%s: remap_pfn_range failed (rc=%d)\n", __func__, rc);
	}

	return rc;
}

/****************************************************************************
*
*   File Operations for the driver.
*
***************************************************************************/

static const struct file_operations vc_mem_fops = {
	.owner = THIS_MODULE,
	.open = vc_mem_open,
	.release = vc_mem_release,
	.unlocked_ioctl = vc_mem_ioctl,
	.mmap = vc_mem_mmap,
};

#ifdef CONFIG_DEBUG_FS
static void vc_mem_debugfs_deinit(void)
{
	debugfs_remove_recursive(vc_mem_debugfs_entry);
	vc_mem_debugfs_entry = NULL;
}


static int vc_mem_debugfs_init(
	struct device *dev)
{
	vc_mem_debugfs_entry = debugfs_create_dir(DRIVER_NAME, NULL);
	if (!vc_mem_debugfs_entry) {
		dev_warn(dev, "could not create debugfs entry\n");
		return -EFAULT;
	}

	if (!debugfs_create_x32("vc_mem_phys_addr",
				0444,
				vc_mem_debugfs_entry,
				(u32 *)&mm_vc_mem_phys_addr)) {
		dev_warn(dev, "%s:could not create vc_mem_phys entry\n",
			__func__);
		goto fail;
	}

	if (!debugfs_create_x32("vc_mem_size",
				0444,
				vc_mem_debugfs_entry,
				(u32 *)&mm_vc_mem_size)) {
		dev_warn(dev, "%s:could not create vc_mem_size entry\n",
			__func__);
		goto fail;
	}

	if (!debugfs_create_x32("vc_mem_base",
				0444,
				vc_mem_debugfs_entry,
				(u32 *)&mm_vc_mem_base)) {
		dev_warn(dev, "%s:could not create vc_mem_base entry\n",
			 __func__);
		goto fail;
	}

	return 0;

fail:
	vc_mem_debugfs_deinit();
	return -EFAULT;
}

#endif /* CONFIG_DEBUG_FS */

/****************************************************************************
*
*   vc_mem_proc_read
*
***************************************************************************/

static int vc_mem_show_info(struct seq_file *m, void *v)
{

	vc_mem_get_size();

	seq_printf(m, "Videocore memory:\n");
	seq_printf(m, "   Physical address: 0x%p\n",
		     (void *)mm_vc_mem_phys_addr);
	seq_printf(m, "   Base Offset:      0x%08x (%u MiB)\n",
		     mm_vc_mem_base, mm_vc_mem_base >> 20);
	seq_printf(m, "   Load Offset:      0x%08x (%u MiB)\n",
		     mm_vc_mem_load, mm_vc_mem_load >> 20);
	seq_printf(m, "   Length (bytes):   %u (%u MiB)\n", mm_vc_mem_size,
		     mm_vc_mem_size >> 20);

	seq_printf(m, "\n");

	return 0;
}

static int vc_mem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, vc_mem_show_info, NULL);
}
/****************************************************************************
*
*   vc_mem_proc_write
*
***************************************************************************/

static int vc_mem_proc_write(struct file *filp,const char *buffer,size_t count,loff_t *data)
{
	int rc = -EFAULT;
	char input_str[10];

	memset(input_str, 0, sizeof(input_str));

	if (count > sizeof(input_str)) {
		pr_err("%s: input string length too long", __func__);
		goto out;
	}

	if (copy_from_user(input_str, buffer, count - 1)) {
		pr_err("%s: failed to get input string", __func__);
		goto out;
	}

	if (strncmp(input_str, "connect", strlen("connect")) == 0)
		/* Get the videocore memory size from the videocore */
		vc_mem_get_size();

out:
	return rc;
}

static const struct file_operations vc_mem_proc_fops = {
	.open = vc_mem_proc_open,
	.read = seq_read,
	.write = vc_mem_proc_write,
	.llseek = seq_lseek,
	.release = single_release
};

/****************************************************************************
*
*   vc_mem_init
*
***************************************************************************/

static int __init
vc_mem_init(void)
{
	int rc = -EFAULT;
	struct device *dev;

	pr_debug("%s: called\n", __func__);

	mm_vc_mem_phys_addr = phys_addr;
	mm_vc_mem_size = mem_size;
	mm_vc_mem_base = mem_base;

	vc_mem_get_size();

	pr_info("vc-mem: phys_addr:0x%08lx mem_base=0x%08x mem_size:0x%08x(%u MiB)\n",
		mm_vc_mem_phys_addr, mm_vc_mem_base, mm_vc_mem_size, mm_vc_mem_size / (1024 * 1024));

	if ((rc = alloc_chrdev_region(&vc_mem_devnum, 0, 1, DRIVER_NAME)) < 0) {
		pr_err("%s: alloc_chrdev_region failed (rc=%d)\n",
		       __func__, rc);
		goto out_err;
	}

	cdev_init(&vc_mem_cdev, &vc_mem_fops);
	if ((rc = cdev_add(&vc_mem_cdev, vc_mem_devnum, 1)) != 0) {
		pr_err("%s: cdev_add failed (rc=%d)\n", __func__, rc);
		goto out_unregister;
	}

	vc_mem_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(vc_mem_class)) {
		rc = PTR_ERR(vc_mem_class);
		pr_err("%s: class_create failed (rc=%d)\n", __func__, rc);
		goto out_cdev_del;
	}

	dev = device_create(vc_mem_class, NULL, vc_mem_devnum, NULL,
			    DRIVER_NAME);
	if (IS_ERR(dev)) {
		rc = PTR_ERR(dev);
		pr_err("%s: device_create failed (rc=%d)\n", __func__, rc);
		goto out_class_destroy;
	}

#ifdef CONFIG_DEBUG_FS
	/* don't fail if the debug entries cannot be created */
	vc_mem_debugfs_init(dev);
#endif

	if (proc_create(DRIVER_NAME, 0444, NULL,&vc_mem_proc_fops) == NULL) {
		rc = -EFAULT;
		pr_err("%s: create_proc_entry failed", __func__);
		goto out_device_destroy;
	}
	vc_mem_inited = 1;
	return 0;

out_device_destroy:
	device_destroy(vc_mem_class, vc_mem_devnum);

      out_class_destroy:
	class_destroy(vc_mem_class);
	vc_mem_class = NULL;

      out_cdev_del:
	cdev_del(&vc_mem_cdev);

      out_unregister:
	unregister_chrdev_region(vc_mem_devnum, 1);

      out_err:
	return -1;
}

/****************************************************************************
*
*   vc_mem_exit
*
***************************************************************************/

static void __exit
vc_mem_exit(void)
{
	pr_debug("%s: called\n", __func__);

	if (vc_mem_inited) {
#if CONFIG_DEBUG_FS
		vc_mem_debugfs_deinit();
#endif
		remove_proc_entry(DRIVER_NAME, NULL);
		device_destroy(vc_mem_class, vc_mem_devnum);
		class_destroy(vc_mem_class);
		cdev_del(&vc_mem_cdev);
		unregister_chrdev_region(vc_mem_devnum, 1);
	}
}

module_init(vc_mem_init);
module_exit(vc_mem_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");

module_param(phys_addr, uint, 0644);
module_param(mem_size, uint, 0644);
module_param(mem_base, uint, 0644);
