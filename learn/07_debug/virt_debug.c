// SPDX-License-Identifier: GPL-2.0
/*
 * virt_debug.c — pr_debug / pr_info 和一个不会被内联的函数，给 dyndbg 和 ftrace 用。
 *
 * 不要在这个模块里故意制造 oops。ch29 曾经把 const 的 thermal ops 放在只读段，
 * 热核代码写入 ops->critical 时缺页。那次的修法是提供 critical 回调，并去掉 const。
 */
#include <linux/module.h>
#include <linux/platform_device.h>

noinline void virt_debug_ping(void);

noinline void virt_debug_ping(void)
{
	pr_debug("virt_debug: ping debug\n");
	pr_info("virt_debug: ping info\n");
}

static ssize_t trigger_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	virt_debug_ping();
	return count;
}
static DEVICE_ATTR_WO(trigger);

static int virt_debug_probe(struct platform_device *pdev)
{
	return device_create_file(&pdev->dev, &dev_attr_trigger);
}

static int virt_debug_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver virt_debug_driver = {
	.probe = virt_debug_probe,
	.remove = virt_debug_remove,
	.driver = {
		.name = "virt_debug",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *virt_debug_pdev;

static int __init virt_debug_init(void)
{
	int ret;

	ret = platform_driver_register(&virt_debug_driver);
	if (ret)
		return ret;
	virt_debug_pdev = platform_device_register_simple("virt_debug",
							  PLATFORM_DEVID_NONE,
							  NULL, 0);
	if (IS_ERR(virt_debug_pdev)) {
		platform_driver_unregister(&virt_debug_driver);
		return PTR_ERR(virt_debug_pdev);
	}
	if (!virt_debug_pdev->dev.driver) {
		platform_device_unregister(virt_debug_pdev);
		platform_driver_unregister(&virt_debug_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_debug_exit(void)
{
	platform_device_unregister(virt_debug_pdev);
	platform_driver_unregister(&virt_debug_driver);
}

module_init(virt_debug_init);
module_exit(virt_debug_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("dyndbg and ftrace target");
