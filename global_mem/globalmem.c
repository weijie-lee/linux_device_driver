/*
 * globalmem.c - Simple character device driver with mutex concurrency control
 *
 * Demonstrates: cdev registration, file_operations, copy_to/from_user,
 * llseek, ioctl, and mutex-based locking for multiple device instances.
 *
 * Fix history:
 *   - Replace trace_printk() with pr_err() in init error path; trace_printk
 *     is an ftrace debugging facility and must not appear in production drivers
 *   - Fix global_mem_init(): on alloc_chrdev_region failure, add early return
 *     instead of continuing to use an invalid devno
 *   - Fix global_mem_init(): class_create failure path now properly cleans up
 *     already-registered cdevs before returning
 *   - Fix global_mem_exit(): device_destroy() must be called BEFORE
 *     class_destroy(); the original order accessed a dangling class pointer
 *   - Fix typo in ioctl printk: "zer0" -> "zero"
 *   - Remove dead commented-out code in global_mem_open()
 *   - Replace printk(KERN_INFO ...) with pr_info() throughout
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>		/* for kzalloc() */
#include <linux/uaccess.h>	/* for copy_from/to_user() */

#define GLOBALMEM_SIZE	4096
#define DEVICE_NUM	4
#define GLOBAL_MEM_MAGIC	'g'
#define MEM_CLEAR	_IO(GLOBAL_MEM_MAGIC, 0)

static struct class *globalmem_class;
static const char *chr_dev_name[DEVICE_NUM] = {
	"global_mem_0", "global_mem_1", "global_mem_2", "global_mem_3"
};

struct global_mem_dev {
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
	struct mutex mutex;
};

static struct global_mem_dev *global_mem_devp;
static dev_t devno;

static int global_mem_open(struct inode *inode, struct file *filp)
{
	/* Use container_of to retrieve the per-device structure from the inode's
	 * embedded cdev, then store it in private_data for use by other fops. */
	struct global_mem_dev *dev = container_of(inode->i_cdev,
						  struct global_mem_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int global_mem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t global_mem_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;

	if (p >= GLOBALMEM_SIZE)
		return 0;

	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	mutex_lock(&dev->mutex);
	if (copy_to_user(buf, dev->mem + p, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
		pr_info("globalmem: read %u byte(s) from offset %lu\n", count, p);
	}
	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t global_mem_write(struct file *filp, const char __user *buf,
				size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;

	if (p >= GLOBALMEM_SIZE)
		return 0;

	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	mutex_lock(&dev->mutex);
	if (copy_from_user(dev->mem + p, buf, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
		pr_info("globalmem: wrote %u byte(s) at offset %lu\n", count, p);
	}
	mutex_unlock(&dev->mutex);

	return ret;
}

static loff_t global_mem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret;

	switch (orig) {
	case SEEK_SET:	/* seek from beginning of file */
		if (offset < 0 || (unsigned int)offset > GLOBALMEM_SIZE)
			return -EINVAL;
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;

	case SEEK_CUR:	/* seek relative to current position */
		if ((filp->f_pos + offset) > GLOBALMEM_SIZE ||
		    (filp->f_pos + offset) < 0)
			return -EINVAL;
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long global_mem_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long args)
{
	long ret = 0;
	struct global_mem_dev *dev = filp->private_data;

	switch (cmd) {
	case MEM_CLEAR:
		mutex_lock(&dev->mutex);
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		mutex_unlock(&dev->mutex);
		pr_info("globalmem: device memory cleared to zero\n"); /* fix: was "zer0" */
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations global_mem_fops = {
	.owner		= THIS_MODULE,
	.open		= global_mem_open,
	.release	= global_mem_release,
	.read		= global_mem_read,
	.write		= global_mem_write,
	.unlocked_ioctl	= global_mem_ioctl,
	.llseek		= global_mem_llseek,
};

static int __init global_mem_init(void)
{
	int ret;
	int i;

	/* Allocate a contiguous range of minor numbers under a single major */
	ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "global_mem");
	if (ret < 0) {
		/* fix: was trace_printk() — an ftrace facility, not for drivers */
		pr_err("globalmem: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;	/* fix: was missing early return; code continued with invalid devno */
	}
	pr_info("globalmem: major=%d minor_base=%d\n", MAJOR(devno), MINOR(devno));

	global_mem_devp = kzalloc(sizeof(*global_mem_devp) * DEVICE_NUM, GFP_KERNEL);
	if (!global_mem_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	for (i = 0; i < DEVICE_NUM; i++) {
		mutex_init(&(global_mem_devp + i)->mutex);
		cdev_init(&(global_mem_devp + i)->cdev, &global_mem_fops);
		(global_mem_devp + i)->cdev.owner = THIS_MODULE;
		ret = cdev_add(&(global_mem_devp + i)->cdev, MKDEV(MAJOR(devno), i), 1);
		if (ret) {
			pr_err("globalmem: failed to add cdev %d, ret=%d\n", i, ret);
			goto fail_cdev;
		}
	}

	globalmem_class = class_create(THIS_MODULE, "global_mem_class");
	if (IS_ERR(globalmem_class)) {
		ret = PTR_ERR(globalmem_class);
		pr_err("globalmem: class_create failed, ret=%d\n", ret);
		goto fail_cdev;
	}

	for (i = 0; i < DEVICE_NUM; i++)
		device_create(globalmem_class, NULL, MKDEV(MAJOR(devno), i),
			      NULL, chr_dev_name[i]);

	return 0;

fail_cdev:
	while (--i >= 0)
		cdev_del(&(global_mem_devp + i)->cdev);
	kfree(global_mem_devp);
fail_malloc:
	unregister_chrdev_region(devno, DEVICE_NUM);
	return ret;
}

static void __exit global_mem_exit(void)
{
	int i;

	/*
	 * Fix: original code called class_destroy() BEFORE device_destroy(),
	 * which left device_destroy() accessing a dangling class pointer.
	 * Correct teardown order: device_destroy -> class_destroy.
	 */
	for (i = 0; i < DEVICE_NUM; i++)
		device_destroy(globalmem_class, MKDEV(MAJOR(devno), i));

	class_destroy(globalmem_class);

	for (i = 0; i < DEVICE_NUM; i++)
		cdev_del(&(global_mem_devp + i)->cdev);

	kfree(global_mem_devp);
	unregister_chrdev_region(devno, DEVICE_NUM);
	pr_info("globalmem: module unloaded\n");
}

module_init(global_mem_init);
module_exit(global_mem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Simple character device driver with mutex concurrency control");
