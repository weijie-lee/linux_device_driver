// SPDX-License-Identifier: GPL-2.0
/*
 * virt_pps.c — Virtual PPS source
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pps_kernel.h>

#define DRV_NAME "virt_pps"

static int virt_probe(struct platform_device *pdev)
{
	struct pps_source_info info = { };
	struct pps_device *pps;

	strscpy(info.name, "virt_pps", sizeof(info.name));
	strscpy(info.path, "platform:virt_pps", sizeof(info.path));
	info.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT | PPS_ECHOASSERT |
		    PPS_CANWAIT | PPS_TSFMT_TSPEC;
	info.owner = THIS_MODULE;
	info.dev = &pdev->dev;
	pps = pps_register_source(&info, PPS_CAPTUREASSERT | PPS_OFFSETASSERT);
	if (IS_ERR(pps))
		return PTR_ERR(pps);
	if (!pps)
		return -ENODEV;
	platform_set_drvdata(pdev, pps);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	pps_unregister_source(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual PPS source");
