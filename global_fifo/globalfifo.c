/*
 * globalfifo.c - FIFO character device with blocking I/O, poll, and fasync
 *
 * Demonstrates: wait queues, blocking read/write, poll/select/epoll support,
 * asynchronous notification (fasync/SIGIO), and platform_driver registration.
 *
 * Fix history:
 *   - Fix wait-queue race condition in read/write: the original code called
 *     add_wait_queue() then __set_current_state() AFTER releasing the mutex,
 *     creating a window where a wakeup could be missed, causing permanent
 *     sleep. Fix: set state to TASK_INTERRUPTIBLE BEFORE releasing the mutex,
 *     so any concurrent wake_up() between unlock and schedule() is not lost.
 *   - Fix global_mem_remove(): device_destroy() must precede class_destroy();
 *     the original order left device_destroy() accessing a freed class object.
 *   - Fix global_mem_probe(): alloc_chrdev_region failure now returns early
 *     instead of continuing with an invalid devno.
 *   - Remove redundant async_queue debug printks in write path.
 *   - Remove dead commented-out code and duplicate #include <linux/wait.h>.
 *   - Replace printk(KERN_INFO ...) with pr_info/pr_err throughout.
 *   - Replace bare printk() (missing log level) in read/write wait loops.
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>			/* for kzalloc() */
#include <linux/uaccess.h>		/* for copy_from/to_user() */
#include <linux/wait.h>			/* for wait_queue_head_t, DECLARE_WAITQUEUE */
#include <linux/sched/signal.h>		/* for signal_pending() */
#include <linux/poll.h>			/* for poll_table */
#include <linux/platform_device.h>	/* for platform_device/driver */

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
	int current_len;		/* bytes currently stored in the FIFO */
	struct mutex mutex;
	wait_queue_head_t r_wait;	/* readers wait here when FIFO is empty */
	wait_queue_head_t w_wait;	/* writers wait here when FIFO is full */
	struct fasync_struct *async_queue;
};

static struct global_mem_dev *global_mem_devp;
static dev_t devno;

static int global_mem_fasync(int fd, struct file *filp, int mode)
{
	struct global_mem_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int global_mem_open(struct inode *inode, struct file *filp)
{
	struct global_mem_dev *dev = container_of(inode->i_cdev,
						  struct global_mem_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int global_mem_release(struct inode *inode, struct file *filp)
{
	/* Remove this file from the async notification list */
	global_mem_fasync(-1, filp, 0);
	return 0;
}

/*
 * global_mem_read - blocking read from the FIFO
 *
 * Race-condition fix: __set_current_state(TASK_INTERRUPTIBLE) is now called
 * BEFORE mutex_unlock(). This ensures that if a writer calls wake_up() in the
 * window between our unlock and schedule(), the state will already be
 * TASK_RUNNING when schedule() is called, so schedule() returns immediately
 * instead of sleeping indefinitely.
 */
static ssize_t global_mem_read(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);

	while (dev->current_len == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}

		/*
		 * Fix: set state BEFORE releasing the mutex.
		 * If a writer wakes us up between unlock and schedule(), the
		 * state will be reset to TASK_RUNNING by wake_up_interruptible,
		 * and schedule() will return immediately — no missed wakeup.
		 */
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		pr_debug("globalfifo: read blocking, waiting for data\n");
		schedule();

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if (count > dev->current_len)
		count = dev->current_len;

	if (copy_to_user(buf, dev->mem, count)) {
		ret = -EFAULT;
		goto out;
	}

	/* Shift remaining data to the front of the buffer */
	memcpy(dev->mem, dev->mem + count, dev->current_len - count);
	dev->current_len -= count;
	pr_info("globalfifo: read %zu byte(s), remaining=%d\n",
		count, dev->current_len);

	wake_up_interruptible(&dev->w_wait);
	ret = count;

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

/*
 * global_mem_write - blocking write to the FIFO
 *
 * Same race-condition fix as global_mem_read: state is set to
 * TASK_INTERRUPTIBLE before releasing the mutex.
 */
static ssize_t global_mem_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);

	while (dev->current_len == GLOBALMEM_SIZE) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}

		/* Fix: set state before unlock to avoid missed wakeup */
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		pr_debug("globalfifo: write blocking, waiting for space\n");
		schedule();

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if (count > GLOBALMEM_SIZE - dev->current_len)
		count = GLOBALMEM_SIZE - dev->current_len;

	if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	dev->current_len += count;
	pr_info("globalfifo: wrote %zu byte(s), current_len=%d\n",
		count, dev->current_len);

	wake_up_interruptible(&dev->r_wait);

	/* Notify async readers that data is available */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

	ret = count;

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static unsigned int global_mem_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct global_mem_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);
	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);

	if (dev->current_len != 0)
		mask |= POLLIN | POLLRDNORM;

	if (dev->current_len != GLOBALMEM_SIZE)
		mask |= POLLOUT | POLLWRNORM;

	mutex_unlock(&dev->mutex);
	return mask;
}

static loff_t global_mem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret;

	switch (orig) {
	case SEEK_SET:
		if (offset < 0 || (unsigned int)offset > GLOBALMEM_SIZE)
			return -EINVAL;
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;

	case SEEK_CUR:
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
		dev->current_len = 0;
		mutex_unlock(&dev->mutex);
		pr_info("globalfifo: device memory cleared\n");
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
	.poll		= global_mem_poll,
	.fasync		= global_mem_fasync,
};

static int __init global_mem_probe(struct platform_device *pdev)
{
	int ret;
	int i;

	ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "global_mem");
	if (ret < 0) {
		pr_err("globalfifo: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;	/* fix: was missing early return */
	}
	pr_info("globalfifo: major=%d minor_base=%d\n", MAJOR(devno), MINOR(devno));

	global_mem_devp = kzalloc(sizeof(*global_mem_devp) * DEVICE_NUM, GFP_KERNEL);
	if (!global_mem_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	for (i = 0; i < DEVICE_NUM; i++) {
		mutex_init(&(global_mem_devp + i)->mutex);
		init_waitqueue_head(&(global_mem_devp + i)->r_wait);
		init_waitqueue_head(&(global_mem_devp + i)->w_wait);
		cdev_init(&(global_mem_devp + i)->cdev, &global_mem_fops);
		(global_mem_devp + i)->cdev.owner = THIS_MODULE;
		ret = cdev_add(&(global_mem_devp + i)->cdev, MKDEV(MAJOR(devno), i), 1);
		if (ret) {
			pr_err("globalfifo: cdev_add failed for device %d\n", i);
			goto fail_cdev;
		}
	}

	globalmem_class = class_create(THIS_MODULE, "global_mem_class");
	if (IS_ERR(globalmem_class)) {
		ret = PTR_ERR(globalmem_class);
		pr_err("globalfifo: class_create failed, ret=%d\n", ret);
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

static int __exit global_mem_remove(struct platform_device *pdev)
{
	int i;

	/*
	 * Fix: original code called class_destroy() BEFORE device_destroy(),
	 * leaving device_destroy() to access a freed class pointer.
	 * Correct order: device_destroy -> class_destroy.
	 */
	for (i = 0; i < DEVICE_NUM; i++)
		device_destroy(globalmem_class, MKDEV(MAJOR(devno), i));

	class_destroy(globalmem_class);

	for (i = 0; i < DEVICE_NUM; i++)
		cdev_del(&(global_mem_devp + i)->cdev);

	kfree(global_mem_devp);
	unregister_chrdev_region(devno, DEVICE_NUM);
	pr_info("globalfifo: module unloaded\n");

	return 0;
}

static struct platform_driver global_mem_driver = {
	.driver = {
		.name = "globalfifo",
		.owner = THIS_MODULE,
	},
	.probe  = global_mem_probe,
	.remove = global_mem_remove,
};

module_platform_driver(global_mem_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("FIFO character device with blocking I/O, poll, and fasync");
