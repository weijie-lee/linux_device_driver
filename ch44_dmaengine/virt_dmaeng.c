// SPDX-License-Identifier: GPL-2.0
/*
 * virt_dmaeng.c — Virtual DMA engine
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>

#define DRV_NAME "virt_dmaeng"

struct virt_dma {
	struct dma_device dma;
	struct dma_chan chan;
};
static enum dma_status virt_status(struct dma_chan *chan, dma_cookie_t cookie,
				   struct dma_tx_state *state)
{
	return DMA_COMPLETE;
}
static void virt_issue(struct dma_chan *chan) { }
static struct dma_async_tx_descriptor *
virt_prep(struct dma_chan *chan, dma_addr_t dst, dma_addr_t src, size_t len,
	  unsigned long flags)
{
	return NULL;
}
static int virt_probe(struct platform_device *pdev)
{
	struct virt_dma *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	INIT_LIST_HEAD(&v->dma.channels);
	dma_cap_set(DMA_MEMCPY, v->dma.cap_mask);
	v->dma.dev = &pdev->dev;
	v->dma.device_tx_status = virt_status;
	v->dma.device_issue_pending = virt_issue;
	v->dma.device_prep_dma_memcpy = virt_prep;
	v->dma.src_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	v->dma.dst_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	v->dma.directions = BIT(DMA_MEM_TO_MEM);
	v->dma.residue_granularity = DMA_RESIDUE_GRANULARITY_DESCRIPTOR;
	v->chan.device = &v->dma;
	list_add_tail(&v->chan.device_node, &v->dma.channels);
	v->dma.chancnt = 1;
	ret = dma_async_device_register(&v->dma);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, v);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct virt_dma *v = platform_get_drvdata(pdev);

	dma_async_device_unregister(&v->dma);
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
MODULE_DESCRIPTION("Virtual DMA engine");
