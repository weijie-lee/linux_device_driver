// SPDX-License-Identifier: GPL-2.0
/*
 * virt_dma.c — 一致性 DMA 和流式 DMA（含 scatterlist）。
 *
 * dma_alloc_coherent：CPU 和设备同时看这一块，不用 sync。
 * dma_map_single：普通 kmalloc 缓冲，交给设备前 sync_for_device，
 * 设备写完后 sync_for_cpu，再 dma_unmap_single。
 * dma_map_sg：一页拆成 scatterlist，设备看到的是总线地址。
 *
 * 映射在 probe 里做完就解除，sysfs 只留下结果字符串。
 * ch12 的 dma_demo 只打印 kmalloc/vmalloc 地址，DMA API 以这里为准。
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

struct virt_dma {
	char report[64];
};

static u64 virt_dma_mask;

static ssize_t report_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct virt_dma *d = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s", d->report);
}
static DEVICE_ATTR_RO(report);

static int virt_dma_probe(struct platform_device *pdev)
{
	struct virt_dma *d;
	void *coh = NULL;
	void *stream = NULL;
	struct page *page = NULL;
	dma_addr_t caddr = 0, saddr = 0;
	struct scatterlist sg;
	bool mapped_single = false;
	bool mapped_sg = false;
	int n, ret;

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	platform_set_drvdata(pdev, d);

	pdev->dev.dma_mask = &virt_dma_mask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		return ret;

	coh = dma_alloc_coherent(&pdev->dev, PAGE_SIZE, &caddr, GFP_KERNEL);
	if (!coh)
		return -ENOMEM;
	memset(coh, 0x5a, 16);
	if (((u8 *)coh)[0] != 0x5a) {
		ret = -EIO;
		goto out;
	}

	stream = kmalloc(128, GFP_KERNEL);
	if (!stream) {
		ret = -ENOMEM;
		goto out;
	}
	memset(stream, 0xa5, 64);
	saddr = dma_map_single(&pdev->dev, stream, 64, DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, saddr)) {
		ret = -EIO;
		goto out;
	}
	mapped_single = true;
	dma_sync_single_for_device(&pdev->dev, saddr, 64, DMA_TO_DEVICE);
	dma_sync_single_for_cpu(&pdev->dev, saddr, 64, DMA_TO_DEVICE);
	if (((u8 *)stream)[0] != 0xa5) {
		ret = -EIO;
		goto out;
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}
	memset(page_address(page), 0x3c, 32);
	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, PAGE_SIZE, 0);
	n = dma_map_sg(&pdev->dev, &sg, 1, DMA_TO_DEVICE);
	if (n < 1) {
		ret = -EIO;
		goto out;
	}
	mapped_sg = true;
	if (((u8 *)page_address(page))[0] != 0x3c) {
		ret = -EIO;
		goto out;
	}

	scnprintf(d->report, sizeof(d->report), "coherent:ok stream:ok sg:ok\n");
	ret = device_create_file(&pdev->dev, &dev_attr_report);

out:
	if (mapped_sg)
		dma_unmap_sg(&pdev->dev, &sg, 1, DMA_TO_DEVICE);
	if (page)
		__free_page(page);
	if (mapped_single)
		dma_unmap_single(&pdev->dev, saddr, 64, DMA_TO_DEVICE);
	kfree(stream);
	if (coh)
		dma_free_coherent(&pdev->dev, PAGE_SIZE, coh, caddr);
	return ret;
}

static int virt_dma_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver virt_dma_driver = {
	.probe = virt_dma_probe,
	.remove = virt_dma_remove,
	.driver = {
		.name = "virt_dma",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *virt_dma_pdev;

static int __init virt_dma_init(void)
{
	int ret;

	ret = platform_driver_register(&virt_dma_driver);
	if (ret)
		return ret;
	virt_dma_pdev = platform_device_register_simple("virt_dma",
							PLATFORM_DEVID_NONE,
							NULL, 0);
	if (IS_ERR(virt_dma_pdev)) {
		platform_driver_unregister(&virt_dma_driver);
		return PTR_ERR(virt_dma_pdev);
	}
	if (!virt_dma_pdev->dev.driver) {
		platform_device_unregister(virt_dma_pdev);
		platform_driver_unregister(&virt_dma_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_dma_exit(void)
{
	platform_device_unregister(virt_dma_pdev);
	platform_driver_unregister(&virt_dma_driver);
}

module_init(virt_dma_init);
module_exit(virt_dma_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Coherent and streaming DMA on a virtual platform device");
