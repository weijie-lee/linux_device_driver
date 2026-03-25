/*
 * PCI 总线驱动 - 高级示例
 * 
 * 功能：
 * 1. PCI BAR 资源管理（多个 BAR）
 * 2. PCI 中断处理（MSI/MSI-X）
 * 3. DMA 缓冲区管理
 * 4. 设备字符驱动接口
 * 
 * 编译：make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Driver");
MODULE_DESCRIPTION("PCI Bus Advanced Driver");
MODULE_VERSION("2.0");

#define PCI_ADVANCED_NAME "pci_advanced"
#define PCI_ADVANCED_MAJOR 0
#define PCI_ADVANCED_MINOR 0
#define PCI_ADVANCED_NR_DEVS 1

/* PCI 设备 ID 表 */
static const struct pci_device_id pci_advanced_ids[] = {
	{ PCI_DEVICE(0x1234, 0x5679) },  /* 高级驱动设备 ID */
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_advanced_ids);

/* 驱动私有数据结构 */
struct pci_advanced_dev {
	struct pci_dev *pdev;
	struct cdev cdev;
	dev_t devno;
	
	/* BAR 资源 */
	void __iomem *bar[6];
	unsigned long bar_len[6];
	
	/* DMA 缓冲区 */
	void *dma_buf;
	dma_addr_t dma_addr;
	size_t dma_size;
	
	/* 中断 */
	int irq;
	int msi_enabled;
	
	/* 统计信息 */
	unsigned long irq_count;
};

static struct class *pci_advanced_class;
static dev_t pci_advanced_devno;

/*
 * 中断处理函数
 */
static irqreturn_t pci_advanced_irq_handler(int irq, void *dev_id)
{
	struct pci_advanced_dev *dev = dev_id;
	
	dev->irq_count++;
	pr_info("[PCI ADV] IRQ %d received (count: %lu)\n", irq, dev->irq_count);
	
	return IRQ_HANDLED;
}

/*
 * 字符设备 open 函数
 */
static int pci_advanced_open(struct inode *inode, struct file *filp)
{
	struct pci_advanced_dev *dev;
	
	dev = container_of(inode->i_cdev, struct pci_advanced_dev, cdev);
	filp->private_data = dev;
	
	pr_info("[PCI ADV] Device opened\n");
	return 0;
}

/*
 * 字符设备 read 函数
 */
static ssize_t pci_advanced_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct pci_advanced_dev *dev = filp->private_data;
	int ret;
	
	if (!dev->dma_buf)
		return -ENOMEM;
	
	if (count > dev->dma_size)
		count = dev->dma_size;
	
	ret = copy_to_user(buf, dev->dma_buf, count);
	if (ret)
		return -EFAULT;
	
	pr_info("[PCI ADV] Read %zu bytes from DMA buffer\n", count);
	return count;
}

/*
 * 字符设备 write 函数
 */
static ssize_t pci_advanced_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct pci_advanced_dev *dev = filp->private_data;
	int ret;
	
	if (!dev->dma_buf)
		return -ENOMEM;
	
	if (count > dev->dma_size)
		count = dev->dma_size;
	
	ret = copy_from_user(dev->dma_buf, buf, count);
	if (ret)
		return -EFAULT;
	
	pr_info("[PCI ADV] Write %zu bytes to DMA buffer\n", count);
	return count;
}

/*
 * 字符设备 ioctl 函数
 */
static long pci_advanced_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct pci_advanced_dev *dev = filp->private_data;
	
	switch (cmd) {
	case 0x1001:  /* GET_IRQ_COUNT */
		if (copy_to_user((void __user *)arg, &dev->irq_count,
				  sizeof(dev->irq_count)))
			return -EFAULT;
		pr_info("[PCI ADV] Get IRQ count: %lu\n", dev->irq_count);
		return 0;
		
	case 0x1002:  /* GET_DMA_ADDR */
		if (copy_to_user((void __user *)arg, &dev->dma_addr,
				  sizeof(dev->dma_addr)))
			return -EFAULT;
		pr_info("[PCI ADV] Get DMA address: 0x%llx\n", 
			(unsigned long long)dev->dma_addr);
		return 0;
		
	default:
		return -ENOTTY;
	}
}

/*
 * 字符设备 close 函数
 */
static int pci_advanced_release(struct inode *inode, struct file *filp)
{
	pr_info("[PCI ADV] Device closed\n");
	return 0;
}

static const struct file_operations pci_advanced_fops = {
	.owner = THIS_MODULE,
	.open = pci_advanced_open,
	.read = pci_advanced_read,
	.write = pci_advanced_write,
	.unlocked_ioctl = pci_advanced_ioctl,
	.release = pci_advanced_release,
};

/*
 * PCI 设备探测函数
 */
static int pci_advanced_probe(struct pci_dev *pdev,
			      const struct pci_device_id *id)
{
	struct pci_advanced_dev *dev;
	int ret, i;

	pr_info("[PCI ADV] Probing device: %04x:%04x\n",
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
		pr_err("[PCI ADV] Failed to enable PCI device\n");
		return ret;
	}

	/* 请求 PCI 资源 */
	ret = pci_request_regions(pdev, "pci_advanced");
	if (ret) {
		pr_err("[PCI ADV] Failed to request PCI regions\n");
		goto err_disable;
	}

	/* 映射所有 BAR */
	for (i = 0; i < 6; i++) {
		if (pci_resource_len(pdev, i) > 0) {
			dev->bar_len[i] = pci_resource_len(pdev, i);
			dev->bar[i] = pci_iomap(pdev, i, dev->bar_len[i]);
			if (!dev->bar[i]) {
				pr_warn("[PCI ADV] Failed to map BAR%d\n", i);
			} else {
				pr_info("[PCI ADV] BAR%d mapped: %p (len: %lu)\n",
					i, dev->bar[i], dev->bar_len[i]);
			}
		}
	}

	/* 分配 DMA 缓冲区 */
	dev->dma_size = 4096;
	dev->dma_buf = dma_alloc_coherent(&pdev->dev, dev->dma_size,
					  &dev->dma_addr, GFP_KERNEL);
	if (!dev->dma_buf) {
		pr_err("[PCI ADV] Failed to allocate DMA buffer\n");
		ret = -ENOMEM;
		goto err_unmap;
	}
	pr_info("[PCI ADV] DMA buffer allocated: virt=%p phys=0x%llx\n",
		dev->dma_buf, (unsigned long long)dev->dma_addr);

	/* 设置 DMA 掩码 */
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("[PCI ADV] Failed to set DMA mask\n");
		goto err_dma_free;
	}

	/* 注册字符设备 */
	ret = alloc_chrdev_region(&dev->devno, 0, 1, PCI_ADVANCED_NAME);
	if (ret) {
		pr_err("[PCI ADV] Failed to allocate chrdev region\n");
		goto err_dma_free;
	}

	cdev_init(&dev->cdev, &pci_advanced_fops);
	dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev->cdev, dev->devno, 1);
	if (ret) {
		pr_err("[PCI ADV] Failed to add cdev\n");
		goto err_unregister_chrdev;
	}

	/* 创建设备类 */
	if (!pci_advanced_class) {
		pci_advanced_class = class_create(THIS_MODULE, PCI_ADVANCED_NAME);
		if (IS_ERR(pci_advanced_class)) {
			ret = PTR_ERR(pci_advanced_class);
			goto err_cdev_del;
		}
	}

	/* 创建设备节点 */
	device_create(pci_advanced_class, NULL, dev->devno, NULL,
		      "%s", PCI_ADVANCED_NAME);

	/* 请求 IRQ */
	dev->irq = pdev->irq;
	if (dev->irq > 0) {
		ret = request_irq(dev->irq, pci_advanced_irq_handler,
				  IRQF_SHARED, "pci_advanced", dev);
		if (ret) {
			pr_warn("[PCI ADV] Failed to request IRQ %d\n", dev->irq);
		} else {
			pr_info("[PCI ADV] IRQ %d registered\n", dev->irq);
		}
	}

	pr_info("[PCI ADV] Device probed successfully\n");
	return 0;

err_cdev_del:
	cdev_del(&dev->cdev);
err_unregister_chrdev:
	unregister_chrdev_region(dev->devno, 1);
err_dma_free:
	dma_free_coherent(&pdev->dev, dev->dma_size, dev->dma_buf,
			  dev->dma_addr);
err_unmap:
	for (i = 0; i < 6; i++) {
		if (dev->bar[i])
			pci_iounmap(pdev, dev->bar[i]);
	}
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

/*
 * PCI 设备移除函数
 */
static void pci_advanced_remove(struct pci_dev *pdev)
{
	struct pci_advanced_dev *dev = pci_get_drvdata(pdev);
	int i;

	pr_info("[PCI ADV] Removing device\n");

	if (dev->irq > 0)
		free_irq(dev->irq, dev);

	device_destroy(pci_advanced_class, dev->devno);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devno, 1);

	dma_free_coherent(&pdev->dev, dev->dma_size, dev->dma_buf,
			  dev->dma_addr);

	for (i = 0; i < 6; i++) {
		if (dev->bar[i])
			pci_iounmap(pdev, dev->bar[i]);
	}

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	pr_info("[PCI ADV] Device removed\n");
}

/* PCI 驱动结构 */
static struct pci_driver pci_advanced_driver = {
	.name = "pci_advanced",
	.id_table = pci_advanced_ids,
	.probe = pci_advanced_probe,
	.remove = pci_advanced_remove,
};

/*
 * 模块初始化函数
 */
static int __init pci_advanced_init(void)
{
	int ret;

	pr_info("[PCI ADV] Initializing PCI Advanced Driver\n");

	ret = pci_register_driver(&pci_advanced_driver);
	if (ret) {
		pr_err("[PCI ADV] Failed to register PCI driver\n");
		return ret;
	}

	pr_info("[PCI ADV] PCI driver registered successfully\n");
	return 0;
}

/*
 * 模块清理函数
 */
static void __exit pci_advanced_exit(void)
{
	pr_info("[PCI ADV] Exiting PCI Advanced Driver\n");
	pci_unregister_driver(&pci_advanced_driver);
	if (pci_advanced_class) {
		class_destroy(pci_advanced_class);
		pci_advanced_class = NULL;
	}
	pr_info("[PCI ADV] PCI driver unregistered\n");
}

module_init(pci_advanced_init);
module_exit(pci_advanced_exit);
