// SPDX-License-Identifier: GPL-2.0
/*
 * virt_mdio.c — Virtual MDIO bus
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/phy.h>

#define DRV_NAME "virt_mdio"

static int virt_read(struct mii_bus *bus, int addr, int reg)
{
	return 0xffff;
}
static int virt_write(struct mii_bus *bus, int addr, int reg, u16 val)
{
	return 0;
}
static int virt_probe(struct platform_device *pdev)
{
	struct mii_bus *bus;
	int ret;

	bus = mdiobus_alloc();
	if (!bus)
		return -ENOMEM;
	bus->name = "virt-mdio";
	bus->read = virt_read;
	bus->write = virt_write;
	bus->parent = &pdev->dev;
	bus->phy_mask = ~0;
	snprintf(bus->id, MII_BUS_ID_SIZE, "virt-mdio");
	ret = __mdiobus_register(bus, THIS_MODULE);
	if (ret) {
		mdiobus_free(bus);
		return ret;
	}
	platform_set_drvdata(pdev, bus);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct mii_bus *bus = platform_get_drvdata(pdev);

	mdiobus_unregister(bus);
	mdiobus_free(bus);
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
MODULE_DESCRIPTION("Virtual MDIO bus");
