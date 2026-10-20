// SPDX-License-Identifier: GPL-2.0
/*
 * virt_devfreq.c — Virtual devfreq device
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>

#define DRV_NAME "virt_devfreq"

static unsigned long virt_freqs[] = { 100000000 };

static int virt_target(struct device *dev, unsigned long *freq, u32 flags)
{
	*freq = virt_freqs[0];
	return 0;
}
static int virt_status(struct device *dev, struct devfreq_dev_status *stat)
{
	stat->current_frequency = virt_freqs[0];
	stat->total_time = 1;
	stat->busy_time = 0;
	return 0;
}
static int virt_cur(struct device *dev, unsigned long *freq)
{
	*freq = virt_freqs[0];
	return 0;
}
static struct devfreq_dev_profile virt_profile = {
	.initial_freq = 100000000,
	.polling_ms = 0,
	.target = virt_target,
	.get_dev_status = virt_status,
	.get_cur_freq = virt_cur,
	.freq_table = virt_freqs,
	.max_state = 1,
};
static void virt_drop_opp(void *data)
{
	dev_pm_opp_remove(data, virt_freqs[0]);
}

static int virt_probe(struct platform_device *pdev)
{
	struct devfreq *df;
	int ret;

	/*
	 * 5.15 devfreq_add_device() takes its min/max from the device OPP
	 * table. A freq_table alone leaves both at 0 and the core returns
	 * -EINVAL. The devm action is registered first so it runs after
	 * devm drops the devfreq device.
	 */
	ret = dev_pm_opp_add(&pdev->dev, virt_freqs[0], 100000);
	if (ret)
		return ret;
	ret = devm_add_action_or_reset(&pdev->dev, virt_drop_opp, &pdev->dev);
	if (ret)
		return ret;
	df = devm_devfreq_add_device(&pdev->dev, &virt_profile, "performance",
				     NULL);
	return PTR_ERR_OR_ZERO(df);
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
MODULE_DESCRIPTION("Virtual devfreq device");
