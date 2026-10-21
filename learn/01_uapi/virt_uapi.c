// SPDX-License-Identifier: GPL-2.0
/*
 * virt_uapi.c — 同一个字符设备上的 read/write、ioctl、poll、mmap、sysfs、debugfs。
 *
 * 缓冲区是一页普通内存。write/read/ioctl 走拷贝，mmap 把这一页映射进用户进程，
 * 两边看到的是同一块数据。有数据时 poll 返回 POLLIN，VIRT_UAPI_CLEAR 清掉长度并唤醒。
 * sysfs 的 bytes 给不想 open 设备节点的人看长度，debugfs 的 flags 只给调试用。
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/wait.h>

#include "virt_uapi_cmd.h"

struct virt_uapi {
	struct mutex		lock;
	wait_queue_head_t	wq;
	struct page		*page;
	u32			len;
	u32			flags;
	struct miscdevice	misc;
	struct dentry		*dbg;
};

static struct virt_uapi *g_uapi;

static int virt_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_uapi;
	return 0;
}

static ssize_t virt_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *ppos)
{
	struct virt_uapi *p = filp->private_data;
	size_t n;
	int ret = 0;

	if (mutex_lock_interruptible(&p->lock))
		return -ERESTARTSYS;
	if (*ppos < 0 || *ppos > p->len) {
		ret = -EINVAL;
		goto out;
	}
	n = min_t(size_t, count, p->len - *ppos);
	if (n && copy_to_user(buf, page_address(p->page) + *ppos, n)) {
		ret = -EFAULT;
		goto out;
	}
	*ppos += n;
	ret = n;
out:
	mutex_unlock(&p->lock);
	return ret;
}

static ssize_t virt_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct virt_uapi *p = filp->private_data;
	size_t n;
	int ret = 0;

	if (mutex_lock_interruptible(&p->lock))
		return -ERESTARTSYS;
	if (*ppos < 0 || *ppos > PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}
	n = min_t(size_t, count, PAGE_SIZE - *ppos);
	if (!n)
		goto out;
	if (copy_from_user(page_address(p->page) + *ppos, buf, n)) {
		ret = -EFAULT;
		goto out;
	}
	*ppos += n;
	if (*ppos > p->len)
		p->len = *ppos;
	ret = n;
out:
	mutex_unlock(&p->lock);
	if (ret > 0)
		wake_up_interruptible(&p->wq);
	return ret;
}

static long virt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct virt_uapi *p = filp->private_data;
	u32 len;

	switch (cmd) {
	case VIRT_UAPI_GET_LEN:
		if (mutex_lock_interruptible(&p->lock))
			return -ERESTARTSYS;
		len = p->len;
		mutex_unlock(&p->lock);
		if (copy_to_user((void __user *)arg, &len, sizeof(len)))
			return -EFAULT;
		return 0;
	case VIRT_UAPI_CLEAR:
		if (mutex_lock_interruptible(&p->lock))
			return -ERESTARTSYS;
		memset(page_address(p->page), 0, PAGE_SIZE);
		p->len = 0;
		mutex_unlock(&p->lock);
		wake_up_interruptible(&p->wq);
		return 0;
	default:
		return -ENOTTY;
	}
}

static __poll_t virt_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct virt_uapi *p = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &p->wq, wait);
	if (mutex_lock_interruptible(&p->lock))
		return EPOLLERR;
	if (p->len)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (p->len < PAGE_SIZE)
		mask |= EPOLLOUT | EPOLLWRNORM;
	mutex_unlock(&p->lock);
	return mask;
}

static int virt_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct virt_uapi *p = filp->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (vma->vm_pgoff != 0 || size > PAGE_SIZE)
		return -EINVAL;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	return remap_pfn_range(vma, vma->vm_start, page_to_pfn(p->page),
			       size, vma->vm_page_prot);
}

static loff_t virt_llseek(struct file *filp, loff_t off, int whence)
{
	struct virt_uapi *p = filp->private_data;
	loff_t n;

	if (mutex_lock_interruptible(&p->lock))
		return -ERESTARTSYS;
	switch (whence) {
	case SEEK_SET:
		n = off;
		break;
	case SEEK_CUR:
		n = filp->f_pos + off;
		break;
	default:
		mutex_unlock(&p->lock);
		return -EINVAL;
	}
	if (n < 0 || n > PAGE_SIZE) {
		mutex_unlock(&p->lock);
		return -EINVAL;
	}
	filp->f_pos = n;
	mutex_unlock(&p->lock);
	return n;
}

static const struct file_operations virt_uapi_fops = {
	.owner		= THIS_MODULE,
	.open		= virt_open,
	.read		= virt_read,
	.write		= virt_write,
	.unlocked_ioctl	= virt_ioctl,
	.llseek		= virt_llseek,
	.poll		= virt_poll,
	.mmap		= virt_mmap,
};

static ssize_t bytes_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct virt_uapi *p = g_uapi;
	u32 len;

	if (mutex_lock_interruptible(&p->lock))
		return -ERESTARTSYS;
	len = p->len;
	mutex_unlock(&p->lock);
	return sysfs_emit(buf, "%u\n", len);
}
static DEVICE_ATTR_RO(bytes);

static int __init virt_uapi_init(void)
{
	struct virt_uapi *p;
	int ret;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	mutex_init(&p->lock);
	init_waitqueue_head(&p->wq);
	p->flags = 1;
	p->page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!p->page) {
		kfree(p);
		return -ENOMEM;
	}

	/* misc 核心把 miscdevice 放在 this_device 的 drvdata 里。
	 * 覆盖它会让 misc_deregister 在 devnode 回调里读到错误指针。
	 * sysfs 的 bytes 只有一个实例，直接用 g_uapi。
	 */
	p->misc.minor = MISC_DYNAMIC_MINOR;
	p->misc.name = "virt_uapi";
	p->misc.fops = &virt_uapi_fops;
	ret = misc_register(&p->misc);
	if (ret)
		goto err_page;

	ret = device_create_file(p->misc.this_device, &dev_attr_bytes);
	if (ret)
		goto err_misc;

	p->dbg = debugfs_create_dir("virt_uapi", NULL);
	if (IS_ERR(p->dbg)) {
		ret = PTR_ERR(p->dbg);
		p->dbg = NULL;
		goto err_misc;
	}
	debugfs_create_u32("flags", 0444, p->dbg, &p->flags);

	g_uapi = p;
	pr_info("virt_uapi: /dev/virt_uapi ready\n");
	return 0;

err_misc:
	misc_deregister(&p->misc);
err_page:
	__free_page(p->page);
	kfree(p);
	return ret;
}

static void __exit virt_uapi_exit(void)
{
	struct virt_uapi *p = g_uapi;

	debugfs_remove_recursive(p->dbg);
	misc_deregister(&p->misc);
	__free_page(p->page);
	kfree(p);
	g_uapi = NULL;
}

module_init(virt_uapi_init);
module_exit(virt_uapi_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Character device with ioctl, poll, mmap, sysfs and debugfs");
