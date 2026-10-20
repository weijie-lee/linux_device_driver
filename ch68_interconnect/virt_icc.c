// SPDX-License-Identifier: GPL-2.0
/*
 * virt_icc.c — Virtual interconnect provider
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interconnect-provider.h>

#define DRV_NAME "virt_icc"

static struct icc_node *virt_node;

static int virt_set(struct icc_node *src, struct icc_node *dst)
{
	return 0;
}
static struct icc_node *virt_xlate(struct of_phandle_args *spec, void *data)
{
	return virt_node;
}
static int virt_probe(struct platform_device *pdev)
{
	struct icc_provider *prov;
	struct icc_node *node;
	int ret;

	prov = devm_kzalloc(&pdev->dev, sizeof(*prov), GFP_KERNEL);
	if (!prov)
		return -ENOMEM;
	prov->set = virt_set;
	prov->aggregate = icc_std_aggregate;
	prov->xlate = virt_xlate;
	prov->dev = &pdev->dev;
	ret = icc_provider_add(prov);
	if (ret)
		return ret;
	node = icc_node_create(0x56530001);
	if (IS_ERR_OR_NULL(node)) {
		icc_provider_del(prov);
		return IS_ERR(node) ? PTR_ERR(node) : -ENOMEM;
	}
	node->name = "virt_icc";
	virt_node = node;
	icc_node_add(node, prov);
	platform_set_drvdata(pdev, prov);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct icc_provider *prov = platform_get_drvdata(pdev);

	icc_nodes_remove(prov);
	icc_provider_del(prov);
	icc_node_destroy(0x56530001);
	virt_node = NULL;
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
MODULE_DESCRIPTION("Virtual interconnect provider");
