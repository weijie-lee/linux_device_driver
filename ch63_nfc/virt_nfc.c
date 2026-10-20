// SPDX-License-Identifier: GPL-2.0
/*
 * virt_nfc.c — Virtual NFC controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <net/nfc/nfc.h>

#define DRV_NAME "virt_nfc"

static int virt_up(struct nfc_dev *dev) { return 0; }
static int virt_down(struct nfc_dev *dev) { return 0; }
static int virt_poll(struct nfc_dev *dev, u32 im, u32 tm) { return 0; }
static void virt_stop(struct nfc_dev *dev) { }
static const struct nfc_ops virt_ops = {
	.dev_up = virt_up,
	.dev_down = virt_down,
	.start_poll = virt_poll,
	.stop_poll = virt_stop,
};
static int virt_probe(struct platform_device *pdev)
{
	struct nfc_dev *ndev;
	int ret;

	ndev = nfc_allocate_device(&virt_ops, NFC_PROTO_NFC_DEP_MASK, 0, 0);
	if (!ndev)
		return -ENOMEM;
	ret = nfc_register_device(ndev);
	if (ret) {
		nfc_free_device(ndev);
		return ret;
	}
	platform_set_drvdata(pdev, ndev);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct nfc_dev *ndev = platform_get_drvdata(pdev);

	nfc_unregister_device(ndev);
	nfc_free_device(ndev);
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
MODULE_DESCRIPTION("Virtual NFC controller");
