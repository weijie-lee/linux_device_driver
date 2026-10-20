// SPDX-License-Identifier: GPL-2.0
/*
 * virt_reset.c — Virtual reset controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>

#define DRV_NAME "virt_reset"

struct virt_rst {
	struct reset_controller_dev rc;
	bool asserted[4];
};
static struct virt_rst *to_rst(struct reset_controller_dev *rc)
{
	return container_of(rc, struct virt_rst, rc);
}
static int virt_assert(struct reset_controller_dev *rc, unsigned long id)
{
	if (id >= 4)
		return -EINVAL;
	to_rst(rc)->asserted[id] = true;
	return 0;
}
static int virt_deassert(struct reset_controller_dev *rc, unsigned long id)
{
	if (id >= 4)
		return -EINVAL;
	to_rst(rc)->asserted[id] = false;
	return 0;
}
static int virt_reset(struct reset_controller_dev *rc, unsigned long id)
{
	int ret = virt_assert(rc, id);

	return ret ? ret : virt_deassert(rc, id);
}
static int virt_status(struct reset_controller_dev *rc, unsigned long id)
{
	if (id >= 4)
		return -EINVAL;
	return to_rst(rc)->asserted[id];
}
static const struct reset_control_ops virt_ops = {
	.reset = virt_reset,
	.assert = virt_assert,
	.deassert = virt_deassert,
	.status = virt_status,
};

static int virt_probe(struct platform_device *pdev)
{
	struct virt_rst *v;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->rc.ops = &virt_ops;
	v->rc.owner = THIS_MODULE;
	v->rc.nr_resets = 4;
	v->rc.dev = &pdev->dev;
	return devm_reset_controller_register(&pdev->dev, &v->rc);
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
MODULE_DESCRIPTION("Virtual reset controller");
