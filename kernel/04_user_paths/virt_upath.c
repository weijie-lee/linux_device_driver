// SPDX-License-Identifier: GPL-2.0
/*
 * virt_upath.c — 同一个值从三条用户态入口进出驱动。
 *
 *   ioctl     /dev/virt_upath            vfs_ioctl → unlocked_ioctl
 *   32 位 ioctl                           compat_ptr_ioctl → unlocked_ioctl
 *   sysfs     /sys/class/misc/virt_upath/value   dev_attr_show → show
 *   procfs    /proc/virt_upath           seq_read_iter → show
 *
 * UPATH_SET 要求 CAP_SYS_ADMIN。打开设备时 VFS 只做文件权限检查，
 * 能力检查在驱动自己手里。
 *
 * 回调都标了 noinline，ftrace 里才有单独的名字。
 */
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "virt_upath.h"

static DEFINE_MUTEX(upath_lock);
static u32 upath_value;
static u32 upath_calls;
static u64 upath_stamp;

static noinline long virt_upath_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	struct upath_info info;
	u32 v;

	switch (cmd) {
	case UPATH_GET:
		/* 结构体整个清零再填，字段之间的填充字节也不会带出内核栈上的旧值 */
		memset(&info, 0, sizeof(info));
		mutex_lock(&upath_lock);
		upath_calls++;
		info.value = upath_value;
		info.calls = upath_calls;
		info.stamp_ns = upath_stamp;
		mutex_unlock(&upath_lock);
		if (copy_to_user(uarg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	case UPATH_SET:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(v, (u32 __user *)uarg))
			return -EFAULT;
		mutex_lock(&upath_lock);
		upath_calls++;
		upath_value = v;
		upath_stamp = ktime_get_ns();
		mutex_unlock(&upath_lock);
		pr_info("virt_upath: value set to %u by uid %u\n", v,
			from_kuid(&init_user_ns, current_uid()));
		return 0;
	default:
		return -ENOTTY;
	}
}

static noinline ssize_t upath_value_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	u32 v;

	mutex_lock(&upath_lock);
	v = upath_value;
	mutex_unlock(&upath_lock);
	return sysfs_emit(buf, "%u\n", v);
}

static noinline ssize_t upath_value_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	u32 v;
	int ret;

	ret = kstrtou32(buf, 0, &v);
	if (ret)
		return ret;
	mutex_lock(&upath_lock);
	upath_value = v;
	upath_stamp = ktime_get_ns();
	mutex_unlock(&upath_lock);
	return count;
}
static DEVICE_ATTR(value, 0644, upath_value_show, upath_value_store);

static struct attribute *upath_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};
ATTRIBUTE_GROUPS(upath);

static noinline int virt_upath_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&upath_lock);
	seq_printf(m, "value=%u calls=%u\n", upath_value, upath_calls);
	mutex_unlock(&upath_lock);
	return 0;
}

static const struct file_operations upath_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = virt_upath_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static struct miscdevice upath_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "virt_upath",
	.fops = &upath_fops,
	.groups = upath_groups,
};

static struct proc_dir_entry *upath_proc;

static int __init virt_upath_init(void)
{
	int ret;

	upath_proc = proc_create_single("virt_upath", 0444, NULL,
					virt_upath_proc_show);
	if (!upath_proc)
		return -ENOMEM;
	ret = misc_register(&upath_dev);
	if (ret)
		proc_remove(upath_proc);
	return ret;
}

static void __exit virt_upath_exit(void)
{
	misc_deregister(&upath_dev);
	proc_remove(upath_proc);
}

module_init(virt_upath_init);
module_exit(virt_upath_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("ioctl, compat ioctl, sysfs and procfs paths into one driver");
MODULE_LICENSE("GPL");
