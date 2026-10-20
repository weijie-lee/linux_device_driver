// SPDX-License-Identifier: GPL-2.0
/*
 * virt_hwmon.c — Virtual hwmon sensor
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>

#define DRV_NAME "virt_hwmon"

static umode_t virt_vis(const void *d, enum hwmon_sensor_types t, u32 attr, int ch)
{
	if (t == hwmon_temp && attr == hwmon_temp_input)
		return 0444;
	return 0;
}
static int virt_rd(struct device *dev, enum hwmon_sensor_types t, u32 attr,
		   int ch, long *val)
{
	if (t != hwmon_temp || attr != hwmon_temp_input)
		return -EOPNOTSUPP;
	*val = 25000;
	return 0;
}
static const struct hwmon_ops virt_hops = {
	.is_visible = virt_vis,
	.read = virt_rd,
};
static const struct hwmon_channel_info *virt_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	NULL,
};
static const struct hwmon_chip_info virt_chip = {
	.ops = &virt_hops,
	.info = virt_info,
};
static int virt_probe(struct platform_device *pdev)
{
	struct device *hw;

	hw = devm_hwmon_device_register_with_info(&pdev->dev, DRV_NAME, NULL,
						  &virt_chip, NULL);
	return PTR_ERR_OR_ZERO(hw);
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
MODULE_DESCRIPTION("Virtual hwmon sensor");
