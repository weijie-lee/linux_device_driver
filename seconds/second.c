/*
 * second.c - Kernel timer demonstration: counts elapsed seconds via /dev/second
 *
 * A user-space process opens /dev/second, then reads an integer that represents
 * the number of seconds elapsed since the device was opened. The kernel timer
 * fires every HZ jiffies (1 second) and increments an atomic counter.
 *
 * Fix history:
 *   - Fix second_init(): cdev_add failure now jumps to fail_cdev to clean up
 *     instead of silently continuing to create the class and device node.
 *   - Fix second_init(): class_create failure now properly cleans up the cdev
 *     and device number instead of returning -1 with resources leaked.
 *   - Fix second_exit(): device_destroy() must be called BEFORE class_destroy();
 *     the original order was correct here but the init failure path was not.
 *   - Replace printk(KERN_INFO/ERR ...) with pr_err/pr_info throughout.
 *   - Add MODULE_AUTHOR and MODULE_DESCRIPTION.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/timer.h>

static dev_t devno;
static struct class *seconds_class;

struct second_dev {
	struct cdev cdev;
	atomic_t counter;
	struct timer_list s_timer;
};

static struct second_dev *second_devp;

static void second_timer_handler(struct timer_list *unused)
{
	/* Re-arm the timer for the next second */
	mod_timer(&second_devp->s_timer, jiffies + HZ);
	atomic_inc(&second_devp->counter);
	pr_info("second: jiffies=%ld, elapsed=%d s\n",
		jiffies, atomic_read(&second_devp->counter));
}

static int second_open(struct inode *inode, struct file *filp)
{
	/* Initialise and start the 1-second periodic timer */
	timer_setup(&second_devp->s_timer, second_timer_handler, 0);
	second_devp->s_timer.expires = jiffies + HZ;
	add_timer(&second_devp->s_timer);
	atomic_set(&second_devp->counter, 0);
	return 0;
}

static int second_release(struct inode *inode, struct file *filp)
{
	del_timer_sync(&second_devp->s_timer);
	return 0;
}

static ssize_t second_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int counter = atomic_read(&second_devp->counter);

	if (put_user(counter, (int __user *)buf))
		return -EFAULT;

	return sizeof(int);
}

static const struct file_operations second_fops = {
	.owner   = THIS_MODULE,
	.open    = second_open,
	.release = second_release,
	.read    = second_read,
};

static int __init second_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&devno, 0, 1, "second");
	if (ret < 0) {
		pr_err("second: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;
	}

	second_devp = kzalloc(sizeof(*second_devp), GFP_KERNEL);
	if (!second_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	cdev_init(&second_devp->cdev, &second_fops);
	second_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&second_devp->cdev, devno, 1);
	if (ret) {
		pr_err("second: cdev_add failed, ret=%d\n", ret);
		goto fail_cdev;	/* fix: was silently continuing */
	}

	seconds_class = class_create(THIS_MODULE, "seconds_class");
	if (IS_ERR(seconds_class)) {
		ret = PTR_ERR(seconds_class);
		pr_err("second: class_create failed, ret=%d\n", ret);
		goto fail_class;	/* fix: was leaking cdev and devno */
	}

	device_create(seconds_class, NULL, devno, NULL, "second");
	pr_info("second: module loaded, device /dev/second created\n");
	return 0;

fail_class:
	cdev_del(&second_devp->cdev);
fail_cdev:
	kfree(second_devp);
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void __exit second_exit(void)
{
	/*
	 * Correct teardown order:
	 *   1. Stop the timer (del_timer_sync ensures handler has finished)
	 *   2. Remove the device node
	 *   3. Destroy the class
	 *   4. Remove the cdev
	 *   5. Free memory and release the device number
	 */
	device_destroy(seconds_class, devno);
	class_destroy(seconds_class);
	cdev_del(&second_devp->cdev);
	kfree(second_devp);
	unregister_chrdev_region(devno, 1);
	pr_info("second: module unloaded\n");
}

module_init(second_init);
module_exit(second_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Kernel timer demo: counts elapsed seconds via /dev/second");
