// SPDX-License-Identifier: GPL-2.0
/*
 * virt_mtd.c — Virtual MTD RAM
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>

#define DRV_NAME "virt_mtd"

#define VIRT_MTD_SIZE (64 * 1024)

struct virt_mtd {
	struct mtd_info mtd;
	u8 *buf;
};
static int virt_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct virt_mtd *v = mtd->priv;

	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;
	memset(v->buf + instr->addr, 0xff, instr->len);
	return 0;
}
static int virt_read(struct mtd_info *mtd, loff_t from, size_t len,
		     size_t *retlen, u_char *buf)
{
	struct virt_mtd *v = mtd->priv;

	if (from + len > mtd->size)
		return -EINVAL;
	memcpy(buf, v->buf + from, len);
	*retlen = len;
	return 0;
}
static int virt_write(struct mtd_info *mtd, loff_t to, size_t len,
		      size_t *retlen, const u_char *buf)
{
	struct virt_mtd *v = mtd->priv;

	if (to + len > mtd->size)
		return -EINVAL;
	memcpy(v->buf + to, buf, len);
	*retlen = len;
	return 0;
}
static int virt_probe(struct platform_device *pdev)
{
	struct virt_mtd *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->buf = devm_kzalloc(&pdev->dev, VIRT_MTD_SIZE, GFP_KERNEL);
	if (!v->buf)
		return -ENOMEM;
	memset(v->buf, 0xff, VIRT_MTD_SIZE);
	v->mtd.name = "virt-mtd";
	v->mtd.type = MTD_RAM;
	v->mtd.flags = MTD_CAP_RAM;
	v->mtd.size = VIRT_MTD_SIZE;
	v->mtd.writesize = 1;
	v->mtd.writebufsize = 1;
	v->mtd.erasesize = 4096;
	v->mtd.owner = THIS_MODULE;
	v->mtd.priv = v;
	v->mtd.dev.parent = &pdev->dev;
	v->mtd._erase = virt_erase;
	v->mtd._read = virt_read;
	v->mtd._write = virt_write;
	ret = mtd_device_register(&v->mtd, NULL, 0);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, v);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct virt_mtd *v = platform_get_drvdata(pdev);

	mtd_device_unregister(&v->mtd);
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
MODULE_DESCRIPTION("Virtual MTD RAM");
