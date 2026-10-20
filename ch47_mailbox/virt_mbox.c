// SPDX-License-Identifier: GPL-2.0
/*
 * virt_mbox.c — Virtual mailbox controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>

#define DRV_NAME "virt_mbox"

struct virt_mbox {
	struct mbox_controller mbox;
	struct mbox_chan chan;
};
static int virt_send(struct mbox_chan *chan, void *data)
{
	return 0;
}
static const struct mbox_chan_ops virt_ops = {
	.send_data = virt_send,
};
static int virt_probe(struct platform_device *pdev)
{
	struct virt_mbox *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->mbox.dev = &pdev->dev;
	v->mbox.ops = &virt_ops;
	v->mbox.chans = &v->chan;
	v->mbox.num_chans = 1;
	ret = mbox_controller_register(&v->mbox);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, v);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	mbox_controller_unregister(&((struct virt_mbox *)platform_get_drvdata(pdev))->mbox);
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
MODULE_DESCRIPTION("Virtual mailbox controller");
