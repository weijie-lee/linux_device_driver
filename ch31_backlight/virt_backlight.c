// SPDX-License-Identifier: GPL-2.0
/*
 * virt_backlight.c — Virtual backlight
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>

#define DRV_NAME "virt_backlight"

static int virt_update(struct backlight_device *bd)
{
	dev_dbg(&bd->dev, "brightness %d\n", bd->props.brightness);
	return 0;
}
static int virt_get(struct backlight_device *bd)
{
	return bd->props.brightness;
}
static const struct backlight_ops virt_ops = {
	.update_status = virt_update,
	.get_brightness = virt_get,
};
static int virt_probe(struct platform_device *pdev)
{
	struct backlight_properties props = { };
	struct backlight_device *bd;

	props.type = BACKLIGHT_RAW;
	props.max_brightness = 100;
	props.brightness = 50;
	bd = devm_backlight_device_register(&pdev->dev, "virt_bl", &pdev->dev,
					    NULL, &virt_ops, &props);
	return PTR_ERR_OR_ZERO(bd);
}
static int virt_remove(struct platform_device *pdev) { return 0; }

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
MODULE_DESCRIPTION("Virtual backlight");
