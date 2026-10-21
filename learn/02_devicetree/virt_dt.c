// SPDX-License-Identifier: GPL-2.0
/*
 * virt_dt.c — 按设备树的 reg / interrupts / 自定义属性取资源。
 *
 * Ubuntu 5.15.0-173-generic 的 CONFIG_OF 没有打开，客户机不会 unflatten dtb。
 * 模块把自己的 virt-reg.dts 落成同样的 platform 资源：
 *   reg          -> IORESOURCE_MEM，指向一页 RAM 的物理地址
 *   interrupts   -> IORESOURCE_IRQ，编号 19（只读取，不 request_irq）
 *   ldd,threshold -> platform_data 里的 40；of_property_read_u32 在有 OF 时优先
 *
 * probe 用 memremap 访问这页，并用 readl/writel 当寄存器。
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/mm.h>

#define VIRT_DT_ID	0x4c444454

struct virt_dt_pdata {
	u32 threshold;
};

struct virt_dt {
	void *regs;
	u32 irq;
	u32 threshold;
	u32 id;
};

static struct virt_dt_pdata virt_dt_pdata = {
	.threshold = 40,
};

static const struct of_device_id virt_dt_of_match[] = {
	{ .compatible = "ldd,virt-reg" },
	{ }
};
MODULE_DEVICE_TABLE(of, virt_dt_of_match);

static ssize_t threshold_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct virt_dt *d = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", d->threshold);
}
static DEVICE_ATTR_RO(threshold);

static ssize_t id_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct virt_dt *d = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%08x\n", d->id);
}
static DEVICE_ATTR_RO(id);

static ssize_t irq_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct virt_dt *d = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", d->irq);
}
static DEVICE_ATTR_RO(irq);

static int virt_dt_probe(struct platform_device *pdev)
{
	struct virt_dt *d;
	struct resource *mem;
	struct resource *irqres;
	const struct virt_dt_pdata *pd = dev_get_platdata(&pdev->dev);
	void __iomem *regs;
	u32 thr;
	int ret;

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem)
		return -ENODEV;
	d->regs = devm_memremap(&pdev->dev, mem->start, resource_size(mem),
				MEMREMAP_WB);
	if (IS_ERR(d->regs))
		return PTR_ERR(d->regs);
	regs = d->regs;

	irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irqres)
		return -ENODEV;
	d->irq = irqres->start;

	thr = 0;
	if (of_property_read_u32(pdev->dev.of_node, "ldd,threshold", &thr)) {
		if (!pd)
			return -EINVAL;
		thr = pd->threshold;
	}
	d->threshold = thr;

	writel(VIRT_DT_ID, regs + 0);
	writel(thr, regs + 4);
	d->id = readl(regs + 0);
	if (d->id != VIRT_DT_ID || readl(regs + 4) != thr)
		return -EIO;

	platform_set_drvdata(pdev, d);
	ret = device_create_file(&pdev->dev, &dev_attr_threshold);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_id);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_irq);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "reg phys %#llx irq %u threshold %u\n",
		 (unsigned long long)mem->start, d->irq, d->threshold);
	return 0;
}

static int virt_dt_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver virt_dt_driver = {
	.probe = virt_dt_probe,
	.remove = virt_dt_remove,
	.driver = {
		.name = "virt_dt",
		.of_match_table = virt_dt_of_match,
		.owner = THIS_MODULE,
	},
};

static struct platform_device *virt_dt_pdev;
static struct page *virt_dt_page;

static int __init virt_dt_init(void)
{
	struct resource res[2];
	struct platform_device_info info = {
		.name = "virt_dt",
		.id = PLATFORM_DEVID_NONE,
		.res = res,
		.num_res = ARRAY_SIZE(res),
		.data = &virt_dt_pdata,
		.size_data = sizeof(virt_dt_pdata),
	};
	phys_addr_t phys;
	int ret;

	virt_dt_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!virt_dt_page)
		return -ENOMEM;
	phys = page_to_phys(virt_dt_page);

	memset(res, 0, sizeof(res));
	res[0].start = phys;
	res[0].end = phys + PAGE_SIZE - 1;
	res[0].flags = IORESOURCE_MEM;
	res[1].start = 19;
	res[1].end = 19;
	res[1].flags = IORESOURCE_IRQ;

	ret = platform_driver_register(&virt_dt_driver);
	if (ret)
		goto err_page;

	virt_dt_pdev = platform_device_register_full(&info);
	if (IS_ERR(virt_dt_pdev)) {
		ret = PTR_ERR(virt_dt_pdev);
		virt_dt_pdev = NULL;
		goto err_drv;
	}
	if (!virt_dt_pdev->dev.driver) {
		ret = -ENODEV;
		goto err_dev;
	}
	return 0;

err_dev:
	platform_device_unregister(virt_dt_pdev);
err_drv:
	platform_driver_unregister(&virt_dt_driver);
err_page:
	__free_page(virt_dt_page);
	virt_dt_page = NULL;
	return ret;
}

static void __exit virt_dt_exit(void)
{
	platform_device_unregister(virt_dt_pdev);
	platform_driver_unregister(&virt_dt_driver);
	__free_page(virt_dt_page);
}

module_init(virt_dt_init);
module_exit(virt_dt_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Platform resources laid out like a device-tree node");
