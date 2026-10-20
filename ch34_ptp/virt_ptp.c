// SPDX-License-Identifier: GPL-2.0
/*
 * virt_ptp.c — Virtual PTP clock
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ptp_clock_kernel.h>

#define DRV_NAME "virt_ptp"

static int virt_adjfine(struct ptp_clock_info *info, long scaled_ppm)
{
	return 0;
}
static int virt_adjtime(struct ptp_clock_info *info, s64 delta)
{
	return 0;
}
static int virt_gettime(struct ptp_clock_info *info, struct timespec64 *ts)
{
	ktime_get_real_ts64(ts);
	return 0;
}
static int virt_settime(struct ptp_clock_info *info, const struct timespec64 *ts)
{
	return 0;
}
static int virt_enable(struct ptp_clock_info *info,
		       struct ptp_clock_request *req, int on)
{
	return -EOPNOTSUPP;
}
static struct ptp_clock_info virt_info = {
	.owner = THIS_MODULE,
	.name = "virt_ptp",
	.max_adj = 1000000,
	.adjfine = virt_adjfine,
	.adjtime = virt_adjtime,
	.gettime64 = virt_gettime,
	.settime64 = virt_settime,
	.enable = virt_enable,
};
static int virt_probe(struct platform_device *pdev)
{
	struct ptp_clock *clk;

	clk = ptp_clock_register(&virt_info, &pdev->dev);
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	platform_set_drvdata(pdev, clk);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	ptp_clock_unregister(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual PTP clock");
