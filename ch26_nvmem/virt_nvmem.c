// SPDX-License-Identifier: GPL-2.0
/*
 * virt_nvmem.c — Virtual NVMEM
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/nvmem-provider.h>

#define DRV_NAME "virt_nvmem"

struct virt_nv {
	u8 buf[256];
};
static int virt_rd(void *priv, unsigned int off, void *val, size_t n)
{
	struct virt_nv *v = priv;

	if (off + n > sizeof(v->buf))
		return -EINVAL;
	memcpy(val, v->buf + off, n);
	return 0;
}
static int virt_wr(void *priv, unsigned int off, void *val, size_t n)
{
	struct virt_nv *v = priv;

	if (off + n > sizeof(v->buf))
		return -EINVAL;
	memcpy(v->buf + off, val, n);
	return 0;
}
static int virt_probe(struct platform_device *pdev)
{
	struct virt_nv *v;
	struct nvmem_config cfg = { };
	struct nvmem_device *nv;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	cfg.dev = &pdev->dev;
	cfg.name = "virt-nvmem";
	cfg.id = -1;
	cfg.owner = THIS_MODULE;
	cfg.priv = v;
	cfg.size = sizeof(v->buf);
	cfg.word_size = 1;
	cfg.stride = 1;
	cfg.reg_read = virt_rd;
	cfg.reg_write = virt_wr;
	nv = devm_nvmem_register(&pdev->dev, &cfg);
	return PTR_ERR_OR_ZERO(nv);
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
MODULE_DESCRIPTION("Virtual NVMEM");
