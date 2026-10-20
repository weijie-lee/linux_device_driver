// SPDX-License-Identifier: GPL-2.0
/*
 * virt_fb.c — Virtual framebuffer
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>

#define DRV_NAME "virt_fb"

#define VIRT_X 320
#define VIRT_Y 240
#define VIRT_BPP 16

static int virt_check(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if (var->xres != VIRT_X || var->yres != VIRT_Y || var->bits_per_pixel != VIRT_BPP)
		return -EINVAL;
	return 0;
}
static int virt_setpar(struct fb_info *info)
{
	return 0;
}
/* cfb_imageblit reads pseudo_palette for the console logo. */
static u32 virt_pseudo_palette[16] = {
	0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f,
	0xc618, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f,
};

static const struct fb_ops virt_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = virt_check,
	.fb_set_par = virt_setpar,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static int virt_probe(struct platform_device *pdev)
{
	struct fb_info *info;
	u32 size = VIRT_X * VIRT_Y * VIRT_BPP / 8;
	int ret;

	info = framebuffer_alloc(0, &pdev->dev);
	if (!info)
		return -ENOMEM;
	info->screen_base = vzalloc(size);
	if (!info->screen_base) {
		framebuffer_release(info);
		return -ENOMEM;
	}
	info->fbops = &virt_ops;
	info->flags = FBINFO_VIRTFB | FBINFO_DEFAULT;
	info->fix.smem_len = size;
	info->fix.line_length = VIRT_X * VIRT_BPP / 8;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	strscpy(info->fix.id, "virtfb", sizeof(info->fix.id));
	info->var.xres = VIRT_X;
	info->var.yres = VIRT_Y;
	info->var.xres_virtual = VIRT_X;
	info->var.yres_virtual = VIRT_Y;
	info->var.bits_per_pixel = VIRT_BPP;
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->pseudo_palette = virt_pseudo_palette;
	ret = register_framebuffer(info);
	if (ret) {
		vfree(info->screen_base);
		framebuffer_release(info);
		return ret;
	}
	platform_set_drvdata(pdev, info);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

	unregister_framebuffer(info);
	vfree(info->screen_base);
	framebuffer_release(info);
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
MODULE_DESCRIPTION("Virtual framebuffer");
