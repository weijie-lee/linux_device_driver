// SPDX-License-Identifier: GPL-2.0
/*
 * virt_hwspin.c — Virtual hwspinlock
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hwspinlock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct hwspinlock_device;
struct hwspinlock {
	struct hwspinlock_device *bank;
	spinlock_t lock;
	void *priv;
};
struct hwspinlock_ops {
	int (*trylock)(struct hwspinlock *lock);
	void (*unlock)(struct hwspinlock *lock);
	int (*bust)(struct hwspinlock *lock, unsigned int id);
	void (*relax)(struct hwspinlock *lock);
};
struct hwspinlock_device {
	struct device *dev;
	const struct hwspinlock_ops *ops;
	int base_id;
	int num_locks;
	struct hwspinlock lock[];
};

#define DRV_NAME "virt_hwspin"

static int virt_trylock(struct hwspinlock *lock)
{
	return 1;
}
static void virt_unlock(struct hwspinlock *lock) { }
static void virt_relax(struct hwspinlock *lock)
{
	cpu_relax();
}
static const struct hwspinlock_ops virt_ops = {
	.trylock = virt_trylock,
	.unlock = virt_unlock,
	.relax = virt_relax,
};
static int virt_probe(struct platform_device *pdev)
{
	struct hwspinlock_device *bank;
	int ret;

	bank = devm_kzalloc(&pdev->dev, sizeof(*bank) + sizeof(struct hwspinlock),
			    GFP_KERNEL);
	if (!bank)
		return -ENOMEM;
	ret = hwspin_lock_register(bank, &pdev->dev, &virt_ops, 32, 1);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, bank);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	return hwspin_lock_unregister(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual hwspinlock");
