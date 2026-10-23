// SPDX-License-Identifier: GPL-2.0
/*
 * virt_mmio.c — 读 QEMU edu 设备的 BAR0。
 *
 * PCI ID 1234:11e8。BAR0 是 1MB MMIO。
 *   0x00 只读，识别字 0x010000ed
 *   0x04 写入一个值，读回来是它的按位取反
 *
 * 只接受 4 字节访问。System.map 里有 ioread32，但这台内核的
 * set_ftrace_filter 不接受它。跟踪停在驱动自己的 noinline 包装上。
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR	0x1234
#define EDU_DEVICE	0x11e8
#define EDU_IDENT	0x010000ed

static noinline u32 virt_mmio_read_id(void __iomem *mmio)
{
	return ioread32(mmio + 0x00);
}

static noinline ssize_t liveness_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	void __iomem *mmio = pci_get_drvdata(to_pci_dev(dev));

	return sysfs_emit(buf, "%08x\n", ioread32(mmio + 0x04));
}

static noinline ssize_t liveness_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	void __iomem *mmio = pci_get_drvdata(to_pci_dev(dev));
	u32 v;
	int ret;

	ret = kstrtou32(buf, 0, &v);
	if (ret)
		return ret;
	iowrite32(v, mmio + 0x04);
	return count;
}
static DEVICE_ATTR_RW(liveness);

static ssize_t ident_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	void __iomem *mmio = pci_get_drvdata(to_pci_dev(dev));

	return sysfs_emit(buf, "%08x\n", virt_mmio_read_id(mmio));
}
static DEVICE_ATTR_RO(ident);

static noinline int virt_mmio_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	void __iomem *mmio;
	u32 ident;
	int ret;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	ret = pci_request_regions(pdev, "virt_mmio");
	if (ret)
		goto err_disable;
	mmio = pci_iomap(pdev, 0, 0);
	if (!mmio) {
		ret = -ENOMEM;
		goto err_regions;
	}
	ident = virt_mmio_read_id(mmio);
	if (ident != EDU_IDENT) {
		dev_err(&pdev->dev, "ident %08x, want %08x\n", ident, EDU_IDENT);
		ret = -ENODEV;
		goto err_iomap;
	}
	pci_set_drvdata(pdev, mmio);
	ret = device_create_file(&pdev->dev, &dev_attr_ident);
	if (ret)
		goto err_iomap;
	ret = device_create_file(&pdev->dev, &dev_attr_liveness);
	if (ret)
		goto err_ident;
	/* 先写一次，读路径在 sysfs 里再跟踪。 */
	iowrite32(0, mmio + 0x04);
	dev_info(&pdev->dev, "ident=%08x bar=%pa\n", ident,
		 &pdev->resource[0].start);
	return 0;

err_ident:
	device_remove_file(&pdev->dev, &dev_attr_ident);
err_iomap:
	pci_iounmap(pdev, mmio);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void virt_mmio_remove(struct pci_dev *pdev)
{
	void __iomem *mmio = pci_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_liveness);
	device_remove_file(&pdev->dev, &dev_attr_ident);
	pci_iounmap(pdev, mmio);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id virt_mmio_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR, EDU_DEVICE) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, virt_mmio_ids);

static struct pci_driver virt_mmio_driver = {
	.name = "virt_mmio",
	.id_table = virt_mmio_ids,
	.probe = virt_mmio_probe,
	.remove = virt_mmio_remove,
};
module_pci_driver(virt_mmio_driver);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("MMIO read and write of the QEMU edu device");
MODULE_LICENSE("GPL");
