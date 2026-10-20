// SPDX-License-Identifier: GPL-2.0
/*
 * virt_mux.c — Virtual mux
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mux/driver.h>

#define DRV_NAME "virt_mux"

static int virt_set(struct mux_control *mux, int state)
{
	return 0;
}
static const struct mux_control_ops virt_ops = {
	.set = virt_set,
};
static int virt_probe(struct platform_device *pdev)
{
	struct mux_chip *chip;
	int ret;

	chip = devm_mux_chip_alloc(&pdev->dev, 1, 0);
	if (IS_ERR(chip))
		return PTR_ERR(chip);
	chip->ops = &virt_ops;
	chip->mux[0].states = 4;
	chip->mux[0].idle_state = MUX_IDLE_AS_IS;
	ret = devm_mux_chip_register(&pdev->dev, chip);
	return ret;
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
MODULE_DESCRIPTION("Virtual mux");
