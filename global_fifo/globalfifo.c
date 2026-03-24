/*
 * globalfifo.c — 具有阻塞 I/O、poll 和异步通知的 FIFO 字符设备
 *
 * 【驱动架构概述】
 * 本驱动在 globalmem 基础上增加了以下核心机制：
 *
 * 1. 阻塞 I/O 与等待队列：
 *    - FIFO 为空时 read 阻塞，等待 r_wait 队列
 *    - FIFO 为满时 write 阻塞，等待 w_wait 队列
 *    - 读写完成后互相唤醒对方的等待队列
 *
 * 2. poll/select/epoll 支持：
 *    - 实现 .poll 回调，向内核注册读写等待队列
 *    - 内核就能在这些队列上睡眠，有数据时自动唤醒用户进程
 *
 * 3. 异步通知（fasync/SIGIO）：
 *    - 应用程序可设置 O_ASYNC 标志，注册异步通知
 *    - 有数据写入时驱动向已注册的进程发送 SIGIO 信号
 *
 * 4. platform_driver 注册：
 *    - 使用 platform_driver 框架，模拟嵌入式设备的总线驱动模式
 *    - probe/remove 回调替代传统的 module_init/module_exit
 *
 * 【等待队列竞态条件修复说明】
 * 原始代码在释放 mutex 后才调用 __set_current_state(TASK_INTERRUPTIBLE)，
 * 存在竞态条件：如果写者在这个窗口内调用 wake_up()，
 * 读者的状态还是 TASK_RUNNING，唤醒信号丢失，导致永久阻塞。
 * 修复：将 __set_current_state() 移到 mutex_unlock() 之前。
 *
 * 【整改记录】
 *   - 修复等待队列竞态条件（永久阻塞 Bug）
 *   - 修复 remove 中 device_destroy/class_destroy 顺序
 *   - 修复 probe 中 alloc_chrdev_region 失败后缺少 return
 *   - 删除冗余调试 printk
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

/*
 * struct global_mem_dev — FIFO 字符设备的私有数据结构体
 *
 * 在 globalmem 的基础上增加了三个关键字段：
 *
 *   current_len   — FIFO 中当前已存储的字节数。
 *                   current_len == 0          → FIFO 为空，读者应阻塞
 *                   current_len == GLOBALMEM_SIZE → FIFO 为满，写者应阻塞
 *
 *   r_wait        — 读等待队列。当 FIFO 为空时，读者在此等待。
 *                   写者写入数据后调用 wake_up_interruptible(&r_wait) 唤醒。
 *
 *   w_wait        — 写等待队列。当 FIFO 为满时，写者在此等待。
 *                   读者读出数据后调用 wake_up_interruptible(&w_wait) 唤醒。
 *
 *   async_queue   — 异步通知链表。应用程序设置 O_ASYNC 后注册到此链表。
 *                   有数据写入时通过 kill_fasync() 向所有注册进程发送 SIGIO。
 */
struct global_mem_dev {
	struct cdev cdev;			/* 内核字符设备结构体 */
	unsigned char mem[GLOBALMEM_SIZE];	/* FIFO 数据缓冲区 */
	int current_len;			/* FIFO 中当前已存储的字节数 */
	struct mutex mutex;			/* 保护所有字段的互斥锁 */
	wait_queue_head_t r_wait;		/* 读等待队列：FIFO 为空时读者在此等待 */
	wait_queue_head_t w_wait;		/* 写等待队列：FIFO 为满时写者在此等待 */
	struct fasync_struct *async_queue;	/* 异步通知链表头指针 */
};

static struct global_mem_dev *global_mem_devp;
static dev_t devno;

/*
 * global_mem_fasync — 异步通知注册/注销
 *
 * 当应用程序对设备文件设置或清除 O_ASYNC 标志时，内核调用此函数。
 * fasync_helper() 负责维护 async_queue 链表：
 *   mode > 0：将此文件加入链表（注册）
 *   mode == 0：从链表中移除此文件（注销）
 */
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
	/*
	 * 关闭时必须从异步通知链表中移除此文件。
	 * 如果不清除，内核在发送 SIGIO 时会访问已关闭的文件结构体，导致内核崩溃。
	 * fasync_helper(-1, filp, 0) 等价于从链表中注销此文件。
	 */
	global_mem_fasync(-1, filp, 0);
	return 0;
}

/*
 * global_mem_read — 阻塞式读取 FIFO
 *
 * 【阻塞读实现原理】
 * 1. 加锁后检查 FIFO 是否有数据（current_len > 0）
 * 2. 如果没有数据：
 *    a. 非阻塞模式（O_NONBLOCK）直接返回 -EAGAIN
 *    b. 阻塞模式：将进程加入等待队列，释放锁，调用 schedule() 休眠
 *    c. 唤醒后重新加锁并检查条件（while 循环防止虚假唤醒）
 * 3. 有数据时：copy_to_user 将数据拷贝到用户空间
 * 4. 将剩余数据左移（简单 FIFO 实现），唤醒写等待队列
 *
 * 【竞态条件修复说明】
 * 必须在 mutex_unlock() 之前调用 __set_current_state(TASK_INTERRUPTIBLE)。
 * 先设置状态再 unlock：写者的 wake_up() 会将状态重置为 TASK_RUNNING，
 * 这样 schedule() 会立即返回，不会永久阻塞。
 */
static ssize_t global_mem_read(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);	/* 在栈上创建等待队列项 */

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);	/* 将当前进程加入读等待队列 */

	/* 循环检查条件（防止虚假唤醒） */
	while (dev->current_len == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;	/* 非阻塞模式：直接返回错误 */
			goto out;
		}

		/*
		 * 关键：必须在 mutex_unlock() 之前设置进程状态为 TASK_INTERRUPTIBLE。
		 * 这样即使写者在 unlock 后、schedule() 前调用 wake_up()，
		 * 读者的状态也会被重置为 TASK_RUNNING，schedule() 不会休眠。
		 */
		__set_current_state(TASK_INTERRUPTIBLE);	/* 先设置状态 */
		mutex_unlock(&dev->mutex);			/* 再释放锁 */

		pr_debug("globalfifo: read blocking, waiting for data\n");
		schedule();	/* 主动让出 CPU，进入休眠 */

		if (signal_pending(current)) {
			/* 收到信号（如 Ctrl+C），返回 -ERESTARTSYS 让内核重启调用 */
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);	/* 唤醒后重新加锁，再次检查条件 */
	}

	/* 防止读取超过实际数据量 */
	if (count > dev->current_len)
		count = dev->current_len;

	/* 将 FIFO 头部数据拷贝到用户空间 */
	if (copy_to_user(buf, dev->mem, count)) {
		ret = -EFAULT;
		goto out;
	}

	/*
	 * 简单 FIFO 实现：将剩余数据左移到缓冲区头部。
	 * 这是 O(n) 操作，对于高性能场景应改用环形缓冲区实现。
	 */
	memcpy(dev->mem, dev->mem + count, dev->current_len - count);
	dev->current_len -= count;
	pr_info("globalfifo: read %zu byte(s), remaining=%d\n",
		count, dev->current_len);

	wake_up_interruptible(&dev->w_wait);	/* 唤醒写等待队列（现在有空间了） */
	ret = count;

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);	/* 从等待队列中移除 */
	set_current_state(TASK_RUNNING);		/* 恢复进程状态为 RUNNING */
	return ret;
}

/*
 * global_mem_write — 阻塞式写入 FIFO
 *
 * 与 read 类似，但阻塞条件是 FIFO 为满（current_len == GLOBALMEM_SIZE）。
 * 写入成功后需要：
 *   1. 唤醒读等待队列（现在有数据可读了）
 *   2. 向异步通知链表中的进程发送 SIGIO
 */
static ssize_t global_mem_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);	/* 将当前进程加入写等待队列 */

	/* 循环检查 FIFO 是否有空间 */
	while (dev->current_len == GLOBALMEM_SIZE) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}

		/* 同样需要先设置状态再 unlock，防止错过唤醒 */
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

	/* 防止写入超过剩余空间 */
	if (count > GLOBALMEM_SIZE - dev->current_len)
		count = GLOBALMEM_SIZE - dev->current_len;

	/* 将用户空间数据拷贝到 FIFO 尾部 */
	if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	dev->current_len += count;
	pr_info("globalfifo: wrote %zu byte(s), current_len=%d\n",
		count, dev->current_len);

	wake_up_interruptible(&dev->r_wait);	/* 唤醒读等待队列 */

	/*
	 * 异步通知：向所有已注册 O_ASYNC 的进程发送 SIGIO 信号。
	 * POLL_IN 表示有数据可读。
	 */
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

/*
 * global_mem_poll — 实现 poll/select/epoll 支持
 *
 * 【poll 工作原理】
 * 内核调用 .poll 回调时传入一个 poll_table。
 * 驱动通过 poll_wait() 将设备的等待队列注册到 poll_table 中。
 * 内核就能在这些队列上睡眠，有事件时自动唤醒用户进程。
 *
 * 返回的 mask 表示当前可用的 I/O 事件：
 *   POLLIN | POLLRDNORM  — 有数据可读
 *   POLLOUT | POLLWRNORM — 有空间可写
 *
 * 注意：poll_wait() 不会阻塞，它只是注册等待队列。
 * 真正的阻塞发生在 poll/select/epoll 的内核实现层。
 */
static unsigned int global_mem_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct global_mem_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);
	poll_wait(filp, &dev->r_wait, wait);	/* 注册读等待队列 */
	poll_wait(filp, &dev->w_wait, wait);	/* 注册写等待队列 */

	if (dev->current_len != 0)
		mask |= POLLIN | POLLRDNORM;	/* FIFO 非空：可读 */

	if (dev->current_len != GLOBALMEM_SIZE)
		mask |= POLLOUT | POLLWRNORM;	/* FIFO 非满：可写 */

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

/*
 * global_mem_probe — platform_driver 的 probe 回调
 *
 * 当内核在 platform_bus 上找到名为 "globalfifo" 的设备时调用此函数。
 * 它替代了传统的 module_init，实现设备的初始化逻辑。
 *
 * 在真实嵌入式驱动中，设备信息通常来自设备树或 ACPI 表。
 * 本驱动用于演示，直接在 init 中手动注册 platform_device。
 */
static int __init global_mem_probe(struct platform_device *pdev)
{
	int ret;
	int i;

	ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "global_mem");
	if (ret < 0) {
		pr_err("globalfifo: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;
	}
	pr_info("globalfifo: major=%d minor_base=%d\n", MAJOR(devno), MINOR(devno));

	global_mem_devp = kzalloc(sizeof(*global_mem_devp) * DEVICE_NUM, GFP_KERNEL);
	if (!global_mem_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	for (i = 0; i < DEVICE_NUM; i++) {
		mutex_init(&(global_mem_devp + i)->mutex);
		init_waitqueue_head(&(global_mem_devp + i)->r_wait);	/* 初始化读等待队列 */
		init_waitqueue_head(&(global_mem_devp + i)->w_wait);	/* 初始化写等待队列 */
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
