// SPDX-License-Identifier: GPL-2.0
/*
 * virt_gpio.c — Virtual GPIO controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>

#define DRV_NAME "virt_gpio"

struct virt_gpio {
	struct gpio_chip gc;
	u8 val[8];
	u8 out[8];
};

static struct virt_gpio *to_vg(struct gpio_chip *gc)
{
	return gpiochip_get_data(gc);
}

static int virt_get_dir(struct gpio_chip *gc, unsigned int off)
{
	return to_vg(gc)->out[off] ? GPIO_LINE_DIRECTION_OUT
				   : GPIO_LINE_DIRECTION_IN;
}

static int virt_dir_in(struct gpio_chip *gc, unsigned int off)
{
	to_vg(gc)->out[off] = 0;
	return 0;
}

static int virt_dir_out(struct gpio_chip *gc, unsigned int off, int val)
{
	struct virt_gpio *v = to_vg(gc);

	v->out[off] = 1;
	v->val[off] = val ? 1 : 0;
	return 0;
}

static int virt_get(struct gpio_chip *gc, unsigned int off)
{
	return to_vg(gc)->val[off];
}

static void virt_set(struct gpio_chip *gc, unsigned int off, int val)
{
	to_vg(gc)->val[off] = val ? 1 : 0;
}

static int virt_probe(struct platform_device *pdev)
{
	struct virt_gpio *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->gc.label = DRV_NAME;
	v->gc.parent = &pdev->dev;
	v->gc.owner = THIS_MODULE;
	v->gc.base = -1;
	v->gc.ngpio = 8;
	v->gc.get_direction = virt_get_dir;
	v->gc.direction_input = virt_dir_in;
	v->gc.direction_output = virt_dir_out;
	v->gc.get = virt_get;
	v->gc.set = virt_set;
	ret = gpiochip_add_data(&v->gc, v);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, v);
	dev_info(&pdev->dev, "8 virtual GPIOs\n");
	return 0;
}

static int virt_remove(struct platform_device *pdev)
{
	gpiochip_remove(&((struct virt_gpio *)platform_get_drvdata(pdev))->gc);
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
MODULE_DESCRIPTION("Virtual GPIO controller");
