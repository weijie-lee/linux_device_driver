// SPDX-License-Identifier: GPL-2.0
/*
 * virt_iio.c — Virtual IIO device
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>

#define DRV_NAME "virt_iio"

static int virt_raw(struct iio_dev *indio, struct iio_chan_spec const *chan,
		    int *val, int *val2, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = 1234;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}
static const struct iio_info virt_info = {
	.read_raw = virt_raw,
};
static const struct iio_chan_spec virt_ch[] = {
	{
		.type = IIO_VOLTAGE,
		.indexed = 1,
		.channel = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_SCALE),
	},
};
static int virt_probe(struct platform_device *pdev)
{
	struct iio_dev *indio;

	indio = devm_iio_device_alloc(&pdev->dev, 0);
	if (!indio)
		return -ENOMEM;
	indio->name = DRV_NAME;
	indio->info = &virt_info;
	indio->modes = INDIO_DIRECT_MODE;
	indio->channels = virt_ch;
	indio->num_channels = ARRAY_SIZE(virt_ch);
	return devm_iio_device_register(&pdev->dev, indio);
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
MODULE_DESCRIPTION("Virtual IIO device");
