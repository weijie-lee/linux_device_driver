// SPDX-License-Identifier: GPL-2.0
/*
 * virt_pinctrl.c — Virtual pin controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#define DRV_NAME "virt_pinctrl"

static const struct pinctrl_pin_desc virt_pins[] = {
	PINCTRL_PIN(0, "virt_gpio0"),
	PINCTRL_PIN(1, "virt_gpio1"),
	PINCTRL_PIN(2, "virt_i2c_scl"),
	PINCTRL_PIN(3, "virt_i2c_sda"),
};
static const unsigned int virt_grp[] = { 0, 1, 2, 3 };

static int virt_ngroups(struct pinctrl_dev *p)
{
	return 1;
}
static const char *virt_gname(struct pinctrl_dev *p, unsigned int s)
{
	return "virt_all";
}
static int virt_gpins(struct pinctrl_dev *p, unsigned int s,
		      const unsigned int **pins, unsigned int *num)
{
	*pins = virt_grp;
	*num = ARRAY_SIZE(virt_grp);
	return 0;
}
static const struct pinctrl_ops virt_ops = {
	.get_groups_count = virt_ngroups,
	.get_group_name = virt_gname,
	.get_group_pins = virt_gpins,
};
static struct pinctrl_desc virt_desc = {
	.name = DRV_NAME,
	.pins = virt_pins,
	.npins = ARRAY_SIZE(virt_pins),
	.pctlops = &virt_ops,
	.owner = THIS_MODULE,
};

static int virt_probe(struct platform_device *pdev)
{
	struct pinctrl_dev *pctl;

	pctl = pinctrl_register(&virt_desc, &pdev->dev, NULL);
	if (IS_ERR(pctl))
		return PTR_ERR(pctl);
	platform_set_drvdata(pdev, pctl);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	pinctrl_unregister(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual pin controller");
