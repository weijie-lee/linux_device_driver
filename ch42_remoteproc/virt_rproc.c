// SPDX-License-Identifier: GPL-2.0
/*
 * virt_rproc.c — Virtual remoteproc
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>

#define DRV_NAME "virt_rproc"

static int virt_start(struct rproc *rproc) { return 0; }
static int virt_stop(struct rproc *rproc) { return 0; }
static const struct rproc_ops virt_ops = {
	.start = virt_start,
	.stop = virt_stop,
};
static int virt_probe(struct platform_device *pdev)
{
	struct rproc *rproc;
	int ret;

	rproc = rproc_alloc(&pdev->dev, "virt-rproc", &virt_ops, NULL, 0);
	if (!rproc)
		return -ENOMEM;
	rproc->auto_boot = false;
	ret = rproc_add(rproc);
	if (ret) {
		rproc_free(rproc);
		return ret;
	}
	platform_set_drvdata(pdev, rproc);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	rproc_del(rproc);
	rproc_free(rproc);
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
MODULE_DESCRIPTION("Virtual remoteproc");
