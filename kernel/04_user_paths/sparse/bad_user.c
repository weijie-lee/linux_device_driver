// SPDX-License-Identifier: GPL-2.0
/*
 * bad_user.c — 给 make sparse 用的反例，不进 make all，也不在客户机里加载。
 *
 * 两处错误 gcc 都不报：
 *   1. 用户态指针少了 __user，传给 copy_from_user
 *   2. 带 __user 的指针在内核里直接解引用
 * sparse 用地址空间检查把它们找出来。正确写法在 ../virt_upath.c。
 */
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

static long bad_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 *plain = (u32 *)arg;
	u32 __user *up = (u32 __user *)arg;
	u32 v;

	if (cmd == 1) {
		if (copy_from_user(&v, plain, sizeof(v)))
			return -EFAULT;
		return v;
	}
	return *up;
}

static const struct file_operations bad_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = bad_ioctl,
};

static struct miscdevice bad_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bad_user",
	.fops = &bad_fops,
};
module_misc_device(bad_dev);

MODULE_LICENSE("GPL");
