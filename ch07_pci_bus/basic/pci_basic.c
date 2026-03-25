/*
 * PCI 总线驱动 - 基础示例
 * 
 * 功能：
 * 1. PCI 设备枚举和探测
 * 2. PCI 配置空间访问
 * 3. 基本的 PCI 驱动框架
 * 
 * 编译：make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Driver");
MODULE_DESCRIPTION("PCI Bus Basic Driver");
MODULE_VERSION("1.0");

/* PCI 设备 ID 表 */
static const struct pci_device_id pci_basic_ids[] = {
	/* 虚拟 PCI 设备 ID（用于 QEMU 模拟） */
	{ PCI_DEVICE(0x1234, 0x5678) },  /* Vendor ID: 0x1234, Device ID: 0x5678 */
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_basic_ids);

/* 驱动私有数据结构 */
struct pci_basic_dev {
	struct pci_dev *pdev;
	void __iomem *mmio_base;
	unsigned long mmio_len;
	int irq;
};

/*
 * PCI 设备探测函数
 * 当 PCI 总线发现匹配的设备时调用
 */
static int pci_basic_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct pci_basic_dev *dev;
	int ret;

	pr_info("[PCI BASIC] Probing device: %04x:%04x\n", 
		pdev->vendor, pdev->device);

	/* 分配驱动私有数据 */
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->pdev = pdev;
	pci_set_drvdata(pdev, dev);

	/* 启用 PCI 设备 */
	ret = pci_enable_device(pdev);
	if (ret) {
		pr_err("[PCI BASIC] Failed to enable PCI device\n");
		return ret;
	}

	/* 启用 PCI 设备的总线主控功能（DMA 必需） */
	pci_set_master(pdev);

	/* 请求 PCI 资源 */
	ret = pci_request_regions(pdev, "pci_basic");
	if (ret) {
		pr_err("[PCI BASIC] Failed to request PCI regions\n");
		goto err_disable;
	}

	/* 获取 BAR0 资源信息 */
	if (pci_resource_len(pdev, 0) > 0) {
		dev->mmio_len = pci_resource_len(pdev, 0);
		dev->mmio_base = pci_iomap(pdev, 0, dev->mmio_len);
		if (!dev->mmio_base) {
			pr_err("[PCI BASIC] Failed to map BAR0\n");
			ret = -ENOMEM;
			goto err_release;
		}
		pr_info("[PCI BASIC] BAR0 mapped: %p (len: %lu)\n", 
			dev->mmio_base, dev->mmio_len);
	}

	/* 获取 IRQ */
	dev->irq = pdev->irq;
	pr_info("[PCI BASIC] IRQ: %d\n", dev->irq);

	/* 打印 PCI 配置空间信息 */
	pr_info("[PCI BASIC] Vendor ID: 0x%04x\n", pdev->vendor);
	pr_info("[PCI BASIC] Device ID: 0x%04x\n", pdev->device);
	pr_info("[PCI BASIC] Class: 0x%06x\n", pdev->class);
	pr_info("[PCI BASIC] Bus: %d, Slot: %d, Function: %d\n",
		pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	/* 设置 DMA 掩码 */
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("[PCI BASIC] Failed to set DMA mask\n");
		goto err_unmap;
	}

	pr_info("[PCI BASIC] Device probed successfully\n");
	return 0;

err_unmap:
	if (dev->mmio_base)
		pci_iounmap(pdev, dev->mmio_base);
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

/*
 * PCI 设备移除函数
 * 当 PCI 设备被移除时调用
 */
static void pci_basic_remove(struct pci_dev *pdev)
{
	struct pci_basic_dev *dev = pci_get_drvdata(pdev);

	pr_info("[PCI BASIC] Removing device\n");

	if (dev->mmio_base)
		pci_iounmap(pdev, dev->mmio_base);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	pr_info("[PCI BASIC] Device removed\n");
}

/* PCI 驱动结构 */
static struct pci_driver pci_basic_driver = {
	.name = "pci_basic",
	.id_table = pci_basic_ids,
	.probe = pci_basic_probe,
	.remove = pci_basic_remove,
};

/*
 * 模块初始化函数
 */
static int __init pci_basic_init(void)
{
	int ret;

	pr_info("[PCI BASIC] Initializing PCI Basic Driver\n");

	ret = pci_register_driver(&pci_basic_driver);
	if (ret) {
		pr_err("[PCI BASIC] Failed to register PCI driver\n");
		return ret;
	}

	pr_info("[PCI BASIC] PCI driver registered successfully\n");
	return 0;
}

/*
 * 模块清理函数
 */
static void __exit pci_basic_exit(void)
{
	pr_info("[PCI BASIC] Exiting PCI Basic Driver\n");
	pci_unregister_driver(&pci_basic_driver);
	pr_info("[PCI BASIC] PCI driver unregistered\n");
}

module_init(pci_basic_init);
module_exit(pci_basic_exit);
