// SPDX-License-Identifier: GPL-2.0
/*
 * virt_i3c.c — Virtual I3C master
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i3c/master.h>

#define DRV_NAME "virt_i3c"

struct virt_i3c {
	struct i3c_master_controller master;
};
static int virt_bus_init(struct i3c_master_controller *m) { return 0; }
static void virt_bus_cleanup(struct i3c_master_controller *m) { }
static int virt_daa(struct i3c_master_controller *m) { return 0; }
static int virt_ccc(struct i3c_master_controller *m, struct i3c_ccc_cmd *cmd)
{
	return 0;
}
static int virt_i2c(struct i2c_dev_desc *dev, const struct i2c_msg *xfers, int n)
{
	return n;
}
static const struct i3c_master_controller_ops virt_ops = {
	.bus_init = virt_bus_init,
	.bus_cleanup = virt_bus_cleanup,
	.do_daa = virt_daa,
	.send_ccc_cmd = virt_ccc,
	.i2c_xfers = virt_i2c,
};
static int virt_probe(struct platform_device *pdev)
{
	struct virt_i3c *v;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	platform_set_drvdata(pdev, v);
	return i3c_master_register(&v->master, &pdev->dev, &virt_ops, false);
}
static int virt_remove(struct platform_device *pdev)
{
	struct virt_i3c *v = platform_get_drvdata(pdev);

	i3c_master_unregister(&v->master);
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
MODULE_DESCRIPTION("Virtual I3C master");
