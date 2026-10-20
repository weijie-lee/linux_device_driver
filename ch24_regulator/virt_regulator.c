// SPDX-License-Identifier: GPL-2.0
/*
 * virt_regulator.c — Virtual fixed regulator
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#define DRV_NAME "virt_regulator"

struct virt_reg {
	bool enabled;
};

static struct virt_reg *vr(struct regulator_dev *rdev)
{
	return rdev_get_drvdata(rdev);
}
static int virt_en(struct regulator_dev *r) { vr(r)->enabled = true; return 0; }
static int virt_dis(struct regulator_dev *r) { vr(r)->enabled = false; return 0; }
static int virt_isen(struct regulator_dev *r) { return vr(r)->enabled; }
static int virt_uV(struct regulator_dev *r) { return 1800000; }
static int virt_list(struct regulator_dev *r, unsigned int sel)
{
	return sel ? -EINVAL : 1800000;
}
static const struct regulator_ops virt_rops = {
	.enable = virt_en,
	.disable = virt_dis,
	.is_enabled = virt_isen,
	.get_voltage = virt_uV,
	.list_voltage = virt_list,
};
static const struct regulator_desc virt_rdesc = {
	.name = "virt_vdd_1v8",
	.type = REGULATOR_VOLTAGE,
	.ops = &virt_rops,
	.n_voltages = 1,
	.fixed_uV = 1800000,
	.owner = THIS_MODULE,
};
static struct regulator_init_data virt_init = {
	.constraints = {
		.name = "virt_vdd_1v8",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static int virt_probe(struct platform_device *pdev)
{
	struct virt_reg *v;
	struct regulator_config cfg = { };
	struct regulator_dev *rdev;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->enabled = true;
	cfg.dev = &pdev->dev;
	cfg.driver_data = v;
	cfg.init_data = &virt_init;
	rdev = devm_regulator_register(&pdev->dev, &virt_rdesc, &cfg);
	if (IS_ERR(rdev))
		return PTR_ERR(rdev);
	return 0;
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
MODULE_DESCRIPTION("Virtual fixed regulator");
