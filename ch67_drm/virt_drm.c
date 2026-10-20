// SPDX-License-Identifier: GPL-2.0
/*
 * virt_drm.c — Virtual DRM device
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_file.h>

#define DRV_NAME "virt_drm"

static char virt_name[] = "virt_drm";
static char virt_desc[] = "Virtual DRM";
static char virt_date[] = "20261020";

static const struct file_operations virt_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.poll = drm_poll,
	.read = drm_read,
	.llseek = noop_llseek,
};
static struct drm_driver virt_ddrv = {
	.driver_features = DRIVER_RENDER,
	.fops = &virt_fops,
	.name = virt_name,
	.desc = virt_desc,
	.date = virt_date,
	.major = 1,
	.minor = 0,
	.patchlevel = 0,
};
static int virt_probe(struct platform_device *pdev)
{
	struct drm_device *drm;
	int ret;

	drm = drm_dev_alloc(&virt_ddrv, &pdev->dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);
	ret = drm_dev_register(drm, 0);
	if (ret) {
		drm_dev_put(drm);
		return ret;
	}
	platform_set_drvdata(pdev, drm);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
	drm_dev_put(drm);
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
MODULE_DESCRIPTION("Virtual DRM device");
