// SPDX-License-Identifier: GPL-2.0
/*
 * virt_bt.c — Virtual Bluetooth HCI
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#define DRV_NAME "virt_bt"

static int virt_open(struct hci_dev *hdev) { return 0; }
static int virt_close(struct hci_dev *hdev) { return 0; }
static int virt_send(struct hci_dev *hdev, struct sk_buff *skb)
{
	kfree_skb(skb);
	return 0;
}
static int virt_probe(struct platform_device *pdev)
{
	struct hci_dev *hdev;
	int ret;

	hdev = hci_alloc_dev();
	if (!hdev)
		return -ENOMEM;
	hdev->bus = HCI_VIRTUAL;
	hci_set_drvdata(hdev, pdev);
	SET_HCIDEV_DEV(hdev, &pdev->dev);
	hdev->open = virt_open;
	hdev->close = virt_close;
	hdev->send = virt_send;
	ret = hci_register_dev(hdev);
	if (ret < 0) {
		hci_free_dev(hdev);
		return ret;
	}
	platform_set_drvdata(pdev, hdev);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct hci_dev *hdev = platform_get_drvdata(pdev);

	hci_unregister_dev(hdev);
	hci_free_dev(hdev);
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
MODULE_DESCRIPTION("Virtual Bluetooth HCI");
