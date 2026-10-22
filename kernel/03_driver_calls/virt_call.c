// SPDX-License-Identifier: GPL-2.0
/*
 * virt_call.c — 驱动调用内核，把字节搬过用户态边界。
 *
 * kernel/02 看的是内核走进 file_operations。这一步反过来：
 *   vfs_write 调用驱动的 write，驱动再调用 _copy_from_user
 *   vfs_read  调用驱动的 read，驱动再调用 _copy_to_user
 *
 * 5.15 的 x86 上，copy_from_user / copy_to_user 是内联函数。
 * 函数跟踪里能单独点名的是 _copy_from_user 和 _copy_to_user。
 * 这两个回调标了 noinline，调用者才会落在驱动函数上。
 *
 * 缓冲只有 64 字节。一次 write 放进内核，下一次 open 再 read 出来。
 * copy_* 可能睡眠，所以用 mutex，不用自旋锁。
 */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define CALL_MAX 64

static DEFINE_MUTEX(call_lock);
static char call_buf[CALL_MAX];
static size_t call_len;

static noinline ssize_t virt_call_write(struct file *file, const char __user *buf,
					 size_t len, loff_t *ppos)
{
	size_t n = len;

	if (!n)
		return 0;
	if (n > CALL_MAX)
		n = CALL_MAX;

	mutex_lock(&call_lock);
	if (copy_from_user(call_buf, buf, n)) {
		mutex_unlock(&call_lock);
		return -EFAULT;
	}
	call_len = n;
	mutex_unlock(&call_lock);

	*ppos += n;
	pr_info("virt_call: stored %zu\n", n);
	return n;
}

static noinline ssize_t virt_call_read(struct file *file, char __user *buf,
				       size_t len, loff_t *ppos)
{
	size_t n;

	mutex_lock(&call_lock);
	if (*ppos >= call_len) {
		mutex_unlock(&call_lock);
		return 0;
	}
	n = call_len - *ppos;
	if (len < n)
		n = len;
	if (copy_to_user(buf, call_buf + *ppos, n)) {
		mutex_unlock(&call_lock);
		return -EFAULT;
	}
	*ppos += n;
	mutex_unlock(&call_lock);

	pr_info("virt_call: returned %zu\n", n);
	return n;
}

static const struct file_operations virt_call_fops = {
	.owner = THIS_MODULE,
	.read = virt_call_read,
	.write = virt_call_write,
};

static struct miscdevice virt_call_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "virt_call",
	.fops = &virt_call_fops,
};

static int __init virt_call_init(void)
{
	return misc_register(&virt_call_dev);
}

static void __exit virt_call_exit(void)
{
	misc_deregister(&virt_call_dev);
}

module_init(virt_call_init);
module_exit(virt_call_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("Trace a driver calling copy_from_user and copy_to_user");
MODULE_LICENSE("GPL");
