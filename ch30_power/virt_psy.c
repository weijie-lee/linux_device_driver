// SPDX-License-Identifier: GPL-2.0
/*
 * virt_psy.c — Virtual battery
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#define DRV_NAME "virt_psy"

static enum power_supply_property virt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_PRESENT,
};
static int virt_get(struct power_supply *psy, enum power_supply_property psp,
		    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 77;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 3800000;
		return 0;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		return 0;
	default:
		return -EINVAL;
	}
}
static const struct power_supply_desc virt_desc = {
	.name = "virt-battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = virt_props,
	.num_properties = ARRAY_SIZE(virt_props),
	.get_property = virt_get,
};
static int virt_probe(struct platform_device *pdev)
{
	struct power_supply_config cfg = { };
	struct power_supply *psy;

	psy = devm_power_supply_register(&pdev->dev, &virt_desc, &cfg);
	return PTR_ERR_OR_ZERO(psy);
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
MODULE_DESCRIPTION("Virtual battery");
