// SPDX-License-Identifier: GPL-2.0
/*
 * virt_fpga.c — Virtual FPGA manager
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fpga/fpga-mgr.h>

#define DRV_NAME "virt_fpga"

static enum fpga_mgr_states virt_state(struct fpga_manager *mgr)
{
	return FPGA_MGR_STATE_OPERATING;
}
static int virt_winit(struct fpga_manager *mgr, struct fpga_image_info *info,
		      const char *buf, size_t count)
{
	return 0;
}
static int virt_write(struct fpga_manager *mgr, const char *buf, size_t count)
{
	return 0;
}
static int virt_complete(struct fpga_manager *mgr, struct fpga_image_info *info)
{
	return 0;
}
static const struct fpga_manager_ops virt_ops = {
	.state = virt_state,
	.write_init = virt_winit,
	.write = virt_write,
	.write_complete = virt_complete,
};
static int virt_probe(struct platform_device *pdev)
{
	struct fpga_manager *mgr;
	int ret;

	mgr = devm_fpga_mgr_create(&pdev->dev, "virt-fpga", &virt_ops, NULL);
	if (!mgr)
		return -ENOMEM;
	ret = fpga_mgr_register(mgr);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, mgr);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	fpga_mgr_unregister(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual FPGA manager");
