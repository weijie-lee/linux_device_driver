// SPDX-License-Identifier: GPL-2.0
/*
 * virt_kif.c — 看驱动和 5.15 内核之间的两条接口。
 *
 * 设备先不注册。实验把 ftrace 打开之后，写 /sys/bus/virtkif/bind，
 * 才能在同一次调用里看到：
 *   kernfs_fop_write_iter → sysfs_kf_write → bus_attr_store
 *   → virt_kif_bind_store → device_add → bus_probe_device
 *   → really_probe → 总线 probe → 驱动 probe
 *
 * 字符设备在 init 里就建好。打开 /dev/virt_kif 时，内核先走
 * chrdev_open，再调 file_operations.open；read 从 vfs_read 进来。
 *
 * 这些回调都标了 noinline，否则编译器会把它们折进调用者，
 * ftrace 的过滤表里就没有单独的名字。
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define TRACE_MAX 512

static char trace_buf[TRACE_MAX];
static size_t trace_len;

static void virtkif_event(const char *fmt, ...)
{
	va_list ap;
	char msg[96];
	int n;

	va_start(ap, fmt);
	n = vscnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (n <= 0)
		return;

	if (!trace_len) {
		trace_len = scnprintf(trace_buf, TRACE_MAX, "%s", msg);
	} else if (trace_len + 3 + (size_t)n + 1 < TRACE_MAX) {
		trace_len += scnprintf(trace_buf + trace_len,
				       TRACE_MAX - trace_len, " > %s", msg);
	}
	pr_info("virtkif: %s\n", msg);
}

struct virt_kif_dev {
	const char *id;
	int bound;
	struct device dev;
};

struct virt_kif_driver {
	const char *id;
	struct device_driver driver;
};

static inline struct virt_kif_dev *to_virt_kif(struct device *dev)
{
	return container_of(dev, struct virt_kif_dev, dev);
}

static struct virt_kif_dev kif_dev;
static struct device *kif_root;
static dev_t kif_devt;
static struct cdev kif_cdev;
static struct class *kif_class;
static struct device *kif_char;
static bool bus_ready;
static bool trace_file;
static bool bind_file;
static bool drv_ready;
static bool dev_ready;
static bool cdev_ready;
static bool class_ready;

static noinline int virt_kif_match(struct device *dev, struct device_driver *drv)
{
	struct virt_kif_dev *fw = to_virt_kif(dev);
	struct virt_kif_driver *ldrv = container_of(drv, struct virt_kif_driver,
						    driver);
	int ok = fw->id && ldrv->id && !strcmp(fw->id, ldrv->id);

	virtkif_event("match %s %d", dev_name(dev), ok);
	return ok;
}

static noinline int virt_kif_uevent(struct device *dev,
				    struct kobj_uevent_env *env)
{
	virtkif_event("uevent %s", dev_name(dev));
	return add_uevent_var(env, "VIRT_KIF_ID=%s", to_virt_kif(dev)->id);
}

static noinline int virt_kif_driver_probe(struct device *dev)
{
	to_virt_kif(dev)->bound = 1;
	virtkif_event("driver_probe %s", dev_name(dev));
	return 0;
}

static noinline int virt_kif_driver_remove(struct device *dev)
{
	to_virt_kif(dev)->bound = 0;
	virtkif_event("driver_remove %s", dev_name(dev));
	return 0;
}

static noinline int virt_kif_bus_probe(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	virtkif_event("bus_probe %s", dev_name(dev));
	if (!drv || !drv->probe)
		return -ENODEV;
	return drv->probe(dev);
}

static noinline void virt_kif_bus_remove(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	virtkif_event("bus_remove %s", dev_name(dev));
	if (drv && drv->remove)
		drv->remove(dev);
}

static void virt_kif_release(struct device *dev)
{
	virtkif_event("release %s", dev_name(dev));
}

static ssize_t bound_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sysfs_emit(buf, "%d\n", to_virt_kif(dev)->bound);
}
static DEVICE_ATTR_RO(bound);

static struct attribute *virt_kif_attrs[] = {
	&dev_attr_bound.attr,
	NULL,
};
ATTRIBUTE_GROUPS(virt_kif);

static struct bus_type virt_kif_bus = {
	.name = "virtkif",
	.match = virt_kif_match,
	.uevent = virt_kif_uevent,
	.probe = virt_kif_bus_probe,
	.remove = virt_kif_bus_remove,
};

static struct virt_kif_driver virt_kif_drv = {
	.id = "sensor",
	.driver = {
		.name = "virtkif_drv",
		.bus = &virt_kif_bus,
		.owner = THIS_MODULE,
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
		.probe = virt_kif_driver_probe,
		.remove = virt_kif_driver_remove,
	},
};

static int virt_kif_add_dev(void)
{
	int ret;

	memset(&kif_dev, 0, sizeof(kif_dev));
	kif_dev.id = "sensor";
	kif_dev.dev.parent = kif_root;
	kif_dev.dev.bus = &virt_kif_bus;
	kif_dev.dev.release = virt_kif_release;
	kif_dev.dev.groups = virt_kif_groups;
	kif_dev.dev.init_name = "virtkif-sensor";

	ret = device_register(&kif_dev.dev);
	if (ret)
		put_device(&kif_dev.dev);
	return ret;
}

static noinline ssize_t virt_kif_bind_store(struct bus_type *bus,
					    const char *buf, size_t count)
{
	int ret;

	if (dev_ready)
		return -EBUSY;
	ret = virt_kif_add_dev();
	if (ret)
		return ret;
	dev_ready = true;
	if (!kif_dev.bound)
		return -ENODEV;
	return count;
}

static ssize_t virt_kif_trace_show(struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%s\n", trace_buf);
}

static struct bus_attribute virt_kif_bind_attr = {
	.attr = { .name = "bind", .mode = 0200 },
	.store = virt_kif_bind_store,
};

static struct bus_attribute virt_kif_trace_attr = {
	.attr = { .name = "trace", .mode = 0444 },
	.show = virt_kif_trace_show,
};

static noinline int virt_kif_open(struct inode *inode, struct file *file)
{
	virtkif_event("cdev_open");
	return 0;
}

static noinline ssize_t virt_kif_read(struct file *file, char __user *buf,
				      size_t len, loff_t *ppos)
{
	const char msg[] = "kif\n";
	size_t n = sizeof(msg) - 1;

	if (*ppos >= n)
		return 0;
	if (len > n - *ppos)
		len = n - *ppos;
	if (copy_to_user(buf, msg + *ppos, len))
		return -EFAULT;
	*ppos += len;
	virtkif_event("cdev_read");
	return len;
}

static const struct file_operations virt_kif_fops = {
	.owner = THIS_MODULE,
	.open = virt_kif_open,
	.read = virt_kif_read,
};

static void virt_kif_cleanup(void)
{
	if (kif_char) {
		device_destroy(kif_class, kif_devt);
		kif_char = NULL;
	}
	if (cdev_ready) {
		cdev_del(&kif_cdev);
		cdev_ready = false;
	}
	if (class_ready) {
		class_destroy(kif_class);
		kif_class = NULL;
		class_ready = false;
	}
	if (kif_devt) {
		unregister_chrdev_region(kif_devt, 1);
		kif_devt = 0;
	}
	if (drv_ready) {
		driver_unregister(&virt_kif_drv.driver);
		drv_ready = false;
	}
	if (dev_ready) {
		device_unregister(&kif_dev.dev);
		dev_ready = false;
	}
	if (kif_root) {
		root_device_unregister(kif_root);
		kif_root = NULL;
	}
	if (bind_file) {
		bus_remove_file(&virt_kif_bus, &virt_kif_bind_attr);
		bind_file = false;
	}
	if (trace_file) {
		bus_remove_file(&virt_kif_bus, &virt_kif_trace_attr);
		trace_file = false;
	}
	if (bus_ready) {
		virtkif_event("bus_unregister");
		bus_unregister(&virt_kif_bus);
		bus_ready = false;
	}
}

static int __init virt_kif_init(void)
{
	int ret;

	ret = bus_register(&virt_kif_bus);
	if (ret)
		return ret;
	bus_ready = true;
	virtkif_event("bus_register");

	ret = bus_create_file(&virt_kif_bus, &virt_kif_trace_attr);
	if (ret)
		goto out;
	trace_file = true;
	ret = bus_create_file(&virt_kif_bus, &virt_kif_bind_attr);
	if (ret)
		goto out;
	bind_file = true;

	kif_root = root_device_register("virtkif");
	if (IS_ERR(kif_root)) {
		ret = PTR_ERR(kif_root);
		kif_root = NULL;
		goto out;
	}

	ret = driver_register(&virt_kif_drv.driver);
	if (ret)
		goto out;
	drv_ready = true;

	ret = alloc_chrdev_region(&kif_devt, 0, 1, "virt_kif");
	if (ret)
		goto out;
	cdev_init(&kif_cdev, &virt_kif_fops);
	ret = cdev_add(&kif_cdev, kif_devt, 1);
	if (ret)
		goto out;
	cdev_ready = true;

	kif_class = class_create(THIS_MODULE, "virt_kif");
	if (IS_ERR(kif_class)) {
		ret = PTR_ERR(kif_class);
		kif_class = NULL;
		goto out;
	}
	class_ready = true;

	kif_char = device_create(kif_class, NULL, kif_devt, NULL, "virt_kif");
	if (IS_ERR(kif_char)) {
		ret = PTR_ERR(kif_char);
		kif_char = NULL;
		goto out;
	}
	return 0;

out:
	virt_kif_cleanup();
	return ret;
}

static void __exit virt_kif_exit(void)
{
	virt_kif_cleanup();
}

module_init(virt_kif_init);
module_exit(virt_kif_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("Trace the 5.15 sysfs and VFS entries into a driver");
MODULE_LICENSE("GPL");
