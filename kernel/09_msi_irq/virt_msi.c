// SPDX-License-Identifier: GPL-2.0
/*
 * virt_msi.c — 用 QEMU edu 的 MSI 等一次阶乘。
 *
 * 同一块 1234:11e8。状态寄存器 0x20 的 bit7 让设备在算完后拉中断。
 * 往 0x08 写入 n，设备算完把 n! 放回 0x08，并通过 MSI 通知。
 * 处理函数读 0x24，把对应位写进 0x64 应答。MSI 也要应答，否则状态位清不掉。
 *
 * 和 kernel/08 不要同时加载，两个驱动都匹配这一块卡。
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR	0x1234
#define EDU_DEVICE	0x11e8
#define EDU_FACT_IRQ	0x1
#define EDU_STATUS_COMPUTING 0x1
#define EDU_STATUS_IRQFACT   0x80

struct virt_msi {
	struct pci_dev	*pdev;
	void __iomem	*mmio;
	int		irq;
	unsigned int	irqs;
	u32		result;
};

static irqreturn_t virt_msi_irq(int irq, void *data)
{
	struct virt_msi *m = data;
	u32 status = ioread32(m->mmio + 0x24);

	if (status & EDU_FACT_IRQ) {
		m->irqs++;
		iowrite32(EDU_FACT_IRQ, m->mmio + 0x64);
	}
	return IRQ_HANDLED;
}

static noinline int virt_msi_compute(struct virt_msi *m, u32 n)
{
	unsigned int i;

	iowrite32(EDU_STATUS_IRQFACT, m->mmio + 0x20);
	iowrite32(n, m->mmio + 0x08);
	for (i = 0; i < 100; i++) {
		if (!(ioread32(m->mmio + 0x20) & EDU_STATUS_COMPUTING))
			break;
		msleep(10);
	}
	m->result = ioread32(m->mmio + 0x08);
	return (ioread32(m->mmio + 0x20) & EDU_STATUS_COMPUTING) ? -ETIMEDOUT : 0;
}

static ssize_t result_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct virt_msi *m = pci_get_drvdata(to_pci_dev(dev));

	return sysfs_emit(buf, "fact=%u irqs=%u\n", m->result, m->irqs);
}
static DEVICE_ATTR_RO(result);

static ssize_t run_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct virt_msi *m = pci_get_drvdata(to_pci_dev(dev));
	u32 n;
	int ret;

	ret = kstrtou32(buf, 0, &n);
	if (ret)
		return ret;
	ret = virt_msi_compute(m, n);
	if (ret)
		return ret;
	return count;
}
static DEVICE_ATTR_WO(run);

static noinline int virt_msi_probe(struct pci_dev *pdev,
				   const struct pci_device_id *id)
{
	struct virt_msi *m;
	int ret;

	m = devm_kzalloc(&pdev->dev, sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->pdev = pdev;
	pci_set_drvdata(pdev, m);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	pci_set_master(pdev);
	ret = pci_request_regions(pdev, "virt_msi");
	if (ret)
		goto err_disable;
	m->mmio = pci_iomap(pdev, 0, 0);
	if (!m->mmio) {
		ret = -ENOMEM;
		goto err_regions;
	}
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (ret < 0)
		goto err_iomap;
	m->irq = pci_irq_vector(pdev, 0);
	ret = request_irq(m->irq, virt_msi_irq, 0, "virt_msi", m);
	if (ret)
		goto err_vectors;
	ret = device_create_file(&pdev->dev, &dev_attr_result);
	if (ret)
		goto err_irq;
	ret = device_create_file(&pdev->dev, &dev_attr_run);
	if (ret)
		goto err_result;
	dev_info(&pdev->dev, "msi irq %d\n", m->irq);
	return 0;

err_result:
	device_remove_file(&pdev->dev, &dev_attr_result);
err_irq:
	free_irq(m->irq, m);
err_vectors:
	pci_free_irq_vectors(pdev);
err_iomap:
	pci_iounmap(pdev, m->mmio);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void virt_msi_remove(struct pci_dev *pdev)
{
	struct virt_msi *m = pci_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_run);
	device_remove_file(&pdev->dev, &dev_attr_result);
	free_irq(m->irq, m);
	pci_free_irq_vectors(pdev);
	pci_iounmap(pdev, m->mmio);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id virt_msi_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR, EDU_DEVICE) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, virt_msi_ids);

static struct pci_driver virt_msi_driver = {
	.name = "virt_msi",
	.id_table = virt_msi_ids,
	.probe = virt_msi_probe,
	.remove = virt_msi_remove,
};
module_pci_driver(virt_msi_driver);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("MSI from the QEMU edu factorial engine");
MODULE_LICENSE("GPL");
