// SPDX-License-Identifier: GPL-2.0
/*
 * virt_counter.c — Virtual counter
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/counter.h>

#define DRV_NAME "virt_counter"

static const enum counter_function virt_funcs[] = {
	COUNTER_FUNCTION_INCREASE,
};
static const enum counter_synapse_action virt_actions[] = {
	COUNTER_SYNAPSE_ACTION_NONE,
};
static struct counter_signal virt_sig = {
	.id = 0,
	.name = "virt_sig",
};
static struct counter_synapse virt_syn = {
	.actions_list = virt_actions,
	.num_actions = ARRAY_SIZE(virt_actions),
	.signal = &virt_sig,
};
static struct counter_count virt_cnt = {
	.id = 0,
	.name = "virt_count",
	.functions_list = virt_funcs,
	.num_functions = ARRAY_SIZE(virt_funcs),
	.synapses = &virt_syn,
	.num_synapses = 1,
};
static int virt_count_read(struct counter_device *c, struct counter_count *count,
			   u64 *value)
{
	*value = 0;
	return 0;
}
static const struct counter_ops virt_ops = {
	.count_read = virt_count_read,
};
static int virt_probe(struct platform_device *pdev)
{
	struct counter_device *counter;
	int ret;

	counter = devm_kzalloc(&pdev->dev, sizeof(*counter), GFP_KERNEL);
	if (!counter)
		return -ENOMEM;
	counter->name = DRV_NAME;
	counter->parent = &pdev->dev;
	counter->ops = &virt_ops;
	counter->signals = &virt_sig;
	counter->num_signals = 1;
	counter->counts = &virt_cnt;
	counter->num_counts = 1;
	ret = devm_counter_register(&pdev->dev, counter);
	return ret;
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
MODULE_DESCRIPTION("Virtual counter");
