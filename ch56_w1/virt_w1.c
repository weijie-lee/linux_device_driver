// SPDX-License-Identifier: GPL-2.0
/*
 * virt_w1.c — Virtual 1-Wire master
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/w1.h>

#define DRV_NAME "virt_w1"

static u8 virt_touch(void *data, u8 bit)
{
	return 1;
}
static u8 virt_reset(void *data)
{
	return 1;
}
static struct w1_bus_master virt_master = {
	.touch_bit = virt_touch,
	.reset_bus = virt_reset,
};
static int virt_probe(struct platform_device *pdev)
{
	virt_master.data = pdev;
	return w1_add_master_device(&virt_master);
}
static int virt_remove(struct platform_device *pdev)
{
	w1_remove_master_device(&virt_master);
	return 0;
}

static struct platform_driver virt_pdrv = {
	.probe = virt_probe,
	.remove = virt_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device *g_pdev;

static int __init virt_mod_init(void)
{
	int ret;

	g_pdev = platform_device_alloc(DRV_NAME, -1);
	if (!g_pdev)
		return -ENOMEM;
	ret = platform_device_add(g_pdev);
	if (ret) {
		platform_device_put(g_pdev);
		return ret;
	}
	ret = platform_driver_register(&virt_pdrv);
	if (ret) {
		platform_device_unregister(g_pdev);
		return ret;
	}
	if (!g_pdev->dev.driver) {
		platform_driver_unregister(&virt_pdrv);
		platform_device_unregister(g_pdev);
		pr_err("%s: probe did not bind\n", DRV_NAME);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_mod_exit(void)
{
	platform_driver_unregister(&virt_pdrv);
	platform_device_unregister(g_pdev);
}

module_init(virt_mod_init);
module_exit(virt_mod_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual 1-Wire master");
