// SPDX-License-Identifier: GPL-2.0
/*
 * virt_gnss.c — Virtual GNSS receiver
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gnss.h>

#define DRV_NAME "virt_gnss"

static int virt_open(struct gnss_device *gdev) { return 0; }
static void virt_close(struct gnss_device *gdev) { }
static int virt_write(struct gnss_device *gdev, const unsigned char *buf, size_t count)
{
	return count;
}
static const struct gnss_operations virt_ops = {
	.open = virt_open,
	.close = virt_close,
	.write_raw = virt_write,
};
static int virt_probe(struct platform_device *pdev)
{
	struct gnss_device *gdev;
	int ret;

	gdev = gnss_allocate_device(&pdev->dev);
	if (!gdev)
		return -ENOMEM;
	gdev->ops = &virt_ops;
	gdev->type = GNSS_TYPE_NMEA;
	ret = gnss_register_device(gdev);
	if (ret) {
		gnss_put_device(gdev);
		return ret;
	}
	platform_set_drvdata(pdev, gdev);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct gnss_device *gdev = platform_get_drvdata(pdev);

	gnss_deregister_device(gdev);
	gnss_put_device(gdev);
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
MODULE_DESCRIPTION("Virtual GNSS receiver");
