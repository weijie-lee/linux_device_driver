// SPDX-License-Identifier: GPL-2.0
/*
 * virt_framework.c — 拆开 5.15 drivers/base 的总线 / 设备 / 驱动绑定。
 *
 * 这不是另一种 platform 驱动。模块自己注册 bus_type，先挂两台设备，
 * 再注册驱动，让下面这条顺序出现在 /sys/bus/lddfw/trace：
 *
 *   device_add → kobject_uevent(KOBJ_ADD) → bus->uevent
 *   driver_register → driver_attach → bus->match
 *   really_probe → call_driver_probe → bus->probe → driver->probe
 *   driver_bound → kobject_uevent(KOBJ_BIND) → bus->uevent
 *
 * call_driver_probe() 在 bus->probe 非空时不会再调用 driver->probe，
 * 所以总线的 probe 必须自己转调驱动。platform_probe() 就是这样做的。
 * remove 在 __device_release_driver() 里同样：bus->remove 非空就由它转调。
 *
 * probe_type 固定为同步，trace 的顺序不会被异步 probe 打乱。
 * 设备是静态变量，release 只记日志，不 kfree。
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#define TRACE_MAX 512

static char trace_buf[TRACE_MAX];
static size_t trace_len;

static void lddfw_event(const char *fmt, ...)
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
	pr_info("lddfw: %s\n", msg);
}

struct lddfw_dev {
	const char *id;
	int bound;
	struct device dev;
};

struct lddfw_driver {
	const char *id;
	struct device_driver driver;
};

static inline struct lddfw_dev *to_lddfw_dev(struct device *dev)
{
	return container_of(dev, struct lddfw_dev, dev);
}

static struct lddfw_dev fw_sensor;
static struct lddfw_dev fw_other;
static struct device *fw_root;

static int lddfw_match(struct device *dev, struct device_driver *drv)
{
	struct lddfw_dev *fw = to_lddfw_dev(dev);
	struct lddfw_driver *ldrv = container_of(drv, struct lddfw_driver, driver);
	int ok = fw->id && ldrv->id && !strcmp(fw->id, ldrv->id);

	lddfw_event("match %s %d", dev_name(dev), ok);
	return ok;
}

static int lddfw_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	int ret;

	lddfw_event("uevent %s", dev_name(dev));
	ret = add_uevent_var(env, "LDDFW_ID=%s", to_lddfw_dev(dev)->id);
	return ret;
}

static int lddfw_driver_probe(struct device *dev)
{
	to_lddfw_dev(dev)->bound = 1;
	lddfw_event("driver_probe %s", dev_name(dev));
	return 0;
}

static int lddfw_driver_remove(struct device *dev)
{
	to_lddfw_dev(dev)->bound = 0;
	lddfw_event("driver_remove %s", dev_name(dev));
	return 0;
}

static int lddfw_bus_probe(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	lddfw_event("bus_probe %s", dev_name(dev));
	if (!drv || !drv->probe)
		return -ENODEV;
	return drv->probe(dev);
}

static void lddfw_bus_remove(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	lddfw_event("bus_remove %s", dev_name(dev));
	if (drv && drv->remove)
		drv->remove(dev);
}

static void lddfw_release(struct device *dev)
{
	lddfw_event("release %s", dev_name(dev));
}

static ssize_t bound_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sysfs_emit(buf, "%d\n", to_lddfw_dev(dev)->bound);
}
static DEVICE_ATTR_RO(bound);

static ssize_t id_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	return sysfs_emit(buf, "%s\n", to_lddfw_dev(dev)->id);
}
static DEVICE_ATTR_RO(id);

static struct attribute *lddfw_attrs[] = {
	&dev_attr_bound.attr,
	&dev_attr_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lddfw);

static ssize_t trace_show(struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%s\n", trace_buf);
}
static BUS_ATTR_RO(trace);

static struct bus_type lddfw_bus = {
	.name = "lddfw",
	.match = lddfw_match,
	.uevent = lddfw_uevent,
	.probe = lddfw_bus_probe,
	.remove = lddfw_bus_remove,
};

static struct lddfw_driver lddfw_drv = {
	.id = "sensor",
	.driver = {
		.name = "lddfw_drv",
		.bus = &lddfw_bus,
		.owner = THIS_MODULE,
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
		.probe = lddfw_driver_probe,
		.remove = lddfw_driver_remove,
	},
};

static bool bus_ready;
static bool trace_ready;
static bool sensor_ready;
static bool other_ready;
static bool drv_ready;

static void lddfw_cleanup(void)
{
	if (drv_ready) {
		driver_unregister(&lddfw_drv.driver);
		drv_ready = false;
	}
	if (sensor_ready) {
		device_unregister(&fw_sensor.dev);
		sensor_ready = false;
	}
	if (other_ready) {
		device_unregister(&fw_other.dev);
		other_ready = false;
	}
	if (fw_root) {
		root_device_unregister(fw_root);
		fw_root = NULL;
	}
	if (trace_ready) {
		bus_remove_file(&lddfw_bus, &bus_attr_trace);
		trace_ready = false;
	}
	if (bus_ready) {
		lddfw_event("bus_unregister");
		bus_unregister(&lddfw_bus);
		bus_ready = false;
	}
}

static int lddfw_add(struct lddfw_dev *fw, const char *name, const char *id)
{
	int ret;

	memset(fw, 0, sizeof(*fw));
	fw->id = id;
	fw->dev.parent = fw_root;
	fw->dev.bus = &lddfw_bus;
	fw->dev.release = lddfw_release;
	fw->dev.groups = lddfw_groups;
	fw->dev.init_name = name;

	ret = device_register(&fw->dev);
	if (ret)
		put_device(&fw->dev);
	return ret;
}

static int __init lddfw_init(void)
{
	int ret;

	ret = bus_register(&lddfw_bus);
	if (ret)
		return ret;
	bus_ready = true;
	lddfw_event("bus_register");

	ret = bus_create_file(&lddfw_bus, &bus_attr_trace);
	if (ret)
		goto out;
	trace_ready = true;

	fw_root = root_device_register("lddfw");
	if (IS_ERR(fw_root)) {
		ret = PTR_ERR(fw_root);
		fw_root = NULL;
		goto out;
	}

	ret = lddfw_add(&fw_sensor, "lddfw-sensor", "sensor");
	if (ret)
		goto out;
	sensor_ready = true;

	ret = lddfw_add(&fw_other, "lddfw-other", "other");
	if (ret)
		goto out;
	other_ready = true;

	ret = driver_register(&lddfw_drv.driver);
	if (ret)
		goto out;
	drv_ready = true;

	if (!fw_sensor.bound || fw_other.bound) {
		ret = -ENODEV;
		goto out;
	}
	return 0;

out:
	lddfw_cleanup();
	return ret;
}

static void __exit lddfw_exit(void)
{
	lddfw_cleanup();
}

module_init(lddfw_init);
module_exit(lddfw_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("5.15 driver-core bus/device/driver bind trace");
MODULE_LICENSE("GPL");
