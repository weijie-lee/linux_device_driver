// SPDX-License-Identifier: GPL-2.0
/*
 * virt_hid.c — Virtual HID device
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hid.h>

#define DRV_NAME "virt_hid"

static const u8 virt_rdesc[] = {
	0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07,
	0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
	0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
	0x75, 0x08, 0x81, 0x01, 0x95, 0x06, 0x75, 0x08,
	0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00,
	0x29, 0x65, 0x81, 0x00, 0xc0
};
static int virt_parse(struct hid_device *hdev)
{
	return hid_parse_report(hdev, (u8 *)virt_rdesc, sizeof(virt_rdesc));
}
static int virt_start(struct hid_device *hdev) { return 0; }
static void virt_stop(struct hid_device *hdev) { }
static int virt_hopen(struct hid_device *hdev) { return 0; }
static void virt_hclose(struct hid_device *hdev) { }
static int virt_raw(struct hid_device *hdev, unsigned char reportnum, u8 *buf,
		    size_t len, unsigned char rtype, int reqtype)
{
	return -EIO;
}
static struct hid_ll_driver virt_ll = {
	.parse = virt_parse,
	.start = virt_start,
	.stop = virt_stop,
	.open = virt_hopen,
	.close = virt_hclose,
	.raw_request = virt_raw,
};
static int virt_probe(struct platform_device *pdev)
{
	struct hid_device *hdev;
	int ret;

	hdev = hid_allocate_device();
	if (IS_ERR(hdev))
		return PTR_ERR(hdev);
	hdev->ll_driver = &virt_ll;
	hdev->bus = BUS_VIRTUAL;
	hdev->vendor = 0x0001;
	hdev->product = 0x0001;
	hdev->version = 0x0100;
	hdev->dev.parent = &pdev->dev;
	strscpy(hdev->name, "virt_hid", sizeof(hdev->name));
	ret = hid_add_device(hdev);
	if (ret) {
		hid_destroy_device(hdev);
		return ret;
	}
	platform_set_drvdata(pdev, hdev);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	hid_destroy_device(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual HID device");
