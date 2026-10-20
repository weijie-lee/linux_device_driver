// SPDX-License-Identifier: GPL-2.0
/*
 * virt_rfkill.c — Virtual rfkill switch
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>

#define DRV_NAME "virt_rfkill"

static int virt_block(void *data, bool blocked)
{
	return 0;
}
static const struct rfkill_ops virt_ops = {
	.set_block = virt_block,
};
static int virt_probe(struct platform_device *pdev)
{
	struct rfkill *rf;
	int ret;

	rf = rfkill_alloc("virt_rfkill", &pdev->dev, RFKILL_TYPE_WLAN,
			  &virt_ops, NULL);
	if (!rf)
		return -ENOMEM;
	ret = rfkill_register(rf);
	if (ret) {
		rfkill_destroy(rf);
		return ret;
	}
	platform_set_drvdata(pdev, rf);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct rfkill *rf = platform_get_drvdata(pdev);

	rfkill_unregister(rf);
	rfkill_destroy(rf);
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
MODULE_DESCRIPTION("Virtual rfkill switch");
