// SPDX-License-Identifier: GPL-2.0
/*
 * virt_sleep.c — 读者在 wait_event 里睡下，写者 wake_up 把它叫醒。
 *
 *   read:  sleepers++，wait_event(sleep_wq, 有数据)，sleepers--，拷出数据
 *   write: 放入数据，wake_up(&sleep_wq)
 *
 * wait_event 把任务置为 TASK_UNINTERRUPTIBLE，睡着的读者在 /proc/PID/status 里是 D。
 * hung task 检测器只看这种状态，实验靠它拿到读者的内核栈。
 *
 * 等待条件里不能拿 mutex：此时任务状态已经不是 TASK_RUNNING。
 * 条件只做无锁检查，醒来以后再在锁里确认一次。
 */
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define SLEEP_MAX 32

static DECLARE_WAIT_QUEUE_HEAD(sleep_wq);
static DEFINE_MUTEX(sleep_lock);
static char sleep_buf[SLEEP_MAX];
static size_t sleep_len;
static atomic_t sleepers = ATOMIC_INIT(0);
static atomic_t wakeups = ATOMIC_INIT(0);

static noinline ssize_t virt_sleep_read(struct file *file, char __user *buf,
					size_t len, loff_t *ppos)
{
	char tmp[SLEEP_MAX];
	size_t n;

	atomic_inc(&sleepers);
	for (;;) {
		wait_event(sleep_wq, READ_ONCE(sleep_len) > 0);
		mutex_lock(&sleep_lock);
		if (sleep_len)
			break;
		mutex_unlock(&sleep_lock);
	}
	n = min(len, sleep_len);
	memcpy(tmp, sleep_buf, n);
	sleep_len = 0;
	mutex_unlock(&sleep_lock);
	atomic_dec(&sleepers);

	if (copy_to_user(buf, tmp, n))
		return -EFAULT;
	return n;
}

static noinline ssize_t virt_sleep_write(struct file *file,
					 const char __user *buf, size_t len,
					 loff_t *ppos)
{
	char tmp[SLEEP_MAX];
	size_t n = min_t(size_t, len, SLEEP_MAX);

	if (!n)
		return 0;
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	mutex_lock(&sleep_lock);
	memcpy(sleep_buf, tmp, n);
	sleep_len = n;
	mutex_unlock(&sleep_lock);
	atomic_inc(&wakeups);
	wake_up(&sleep_wq);
	return n;
}

static ssize_t sleepers_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&sleepers));
}
static DEVICE_ATTR_RO(sleepers);

static ssize_t wakeups_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&wakeups));
}
static DEVICE_ATTR_RO(wakeups);

static struct attribute *sleep_attrs[] = {
	&dev_attr_sleepers.attr,
	&dev_attr_wakeups.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sleep);

static const struct file_operations sleep_fops = {
	.owner = THIS_MODULE,
	.read = virt_sleep_read,
	.write = virt_sleep_write,
};

static struct miscdevice sleep_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "virt_sleep",
	.fops = &sleep_fops,
	.groups = sleep_groups,
};
module_misc_device(sleep_dev);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("A reader sleeps in wait_event until a writer calls wake_up");
MODULE_LICENSE("GPL");
