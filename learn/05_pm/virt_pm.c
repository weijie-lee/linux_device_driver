// SPDX-License-Identifier: GPL-2.0
/*
 * virt_pm.c — runtime PM 和系统睡眠用的 suspend/resume。
 *
 * runtime：probe 里 pm_runtime_set_active + pm_runtime_enable。
 * 写 idle 会调用 pm_runtime_idle()，usage 为 0 时进入 runtime_suspend。
 * 写 power/control 为 on 会 pm_runtime_forbid 并 resume。
 *
 * 系统睡眠：dev_pm_ops.suspend/resume 保存和恢复 ctrl。
 * 不调用 pm_suspend()，那会把整个客户机睡下去。
 * sysfs sleep 调用的是同一对函数指针，PM 核心在 mem 睡眠时走的也是它们。
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

struct virt_pm {
	u32 ctrl;
	u32 saved_ctrl;
	bool sleeping;
	const char *runtime;
	int idle_ret;
};

static int virt_suspend(struct device *dev)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	p->saved_ctrl = p->ctrl;
	p->ctrl = 0;
	p->sleeping = true;
	return 0;
}

static int virt_resume(struct device *dev)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	p->ctrl = p->saved_ctrl;
	p->sleeping = false;
	return 0;
}

static int virt_runtime_suspend(struct device *dev)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	p->runtime = "suspended";
	return 0;
}

static int virt_runtime_resume(struct device *dev)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	p->runtime = "active";
	return 0;
}

static const struct dev_pm_ops virt_pm_ops = {
	.suspend = virt_suspend,
	.resume = virt_resume,
	.runtime_suspend = virt_runtime_suspend,
	.runtime_resume = virt_runtime_resume,
};

static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", p->ctrl);
}
static DEVICE_ATTR_RO(ctrl);

static ssize_t sleeping_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", p->sleeping ? 1 : 0);
}
static DEVICE_ATTR_RO(sleeping);

static ssize_t runtime_marker_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct virt_pm *p = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", p->runtime);
}
static DEVICE_ATTR_RO(runtime_marker);

static ssize_t sleep_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	const struct dev_pm_ops *ops = dev->driver->pm;
	int ret;

	if (!ops)
		return -ENODEV;
	if (sysfs_streq(buf, "suspend"))
		ret = ops->suspend(dev);
	else if (sysfs_streq(buf, "resume"))
		ret = ops->resume(dev);
	else
		return -EINVAL;
	return ret ? ret : count;
}
static DEVICE_ATTR_WO(sleep);

static ssize_t idle_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct virt_pm *p = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_idle(dev);
	p->idle_ret = ret;
	if (ret < 0)
		return ret;
	return count;
}
static DEVICE_ATTR_WO(idle);

static int virt_pm_probe(struct platform_device *pdev)
{
	struct virt_pm *p;
	int ret;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->ctrl = 3;
	p->saved_ctrl = 3;
	p->runtime = "active";
	platform_set_drvdata(pdev, p);

	ret = device_create_file(&pdev->dev, &dev_attr_ctrl);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_sleeping);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_runtime_marker);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_sleep);
	if (ret)
		return ret;
	ret = device_create_file(&pdev->dev, &dev_attr_idle);
	if (ret)
		return ret;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int virt_pm_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static struct platform_driver virt_pm_driver = {
	.probe = virt_pm_probe,
	.remove = virt_pm_remove,
	.driver = {
		.name = "virt_pm",
		.pm = &virt_pm_ops,
		.owner = THIS_MODULE,
	},
};

static struct platform_device *virt_pm_pdev;

static int __init virt_pm_init(void)
{
	int ret;

	ret = platform_driver_register(&virt_pm_driver);
	if (ret)
		return ret;
	virt_pm_pdev = platform_device_register_simple("virt_pm",
						       PLATFORM_DEVID_NONE,
						       NULL, 0);
	if (IS_ERR(virt_pm_pdev)) {
		platform_driver_unregister(&virt_pm_driver);
		return PTR_ERR(virt_pm_pdev);
	}
	if (!virt_pm_pdev->dev.driver) {
		platform_device_unregister(virt_pm_pdev);
		platform_driver_unregister(&virt_pm_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_pm_exit(void)
{
	platform_device_unregister(virt_pm_pdev);
	platform_driver_unregister(&virt_pm_driver);
}

module_init(virt_pm_init);
module_exit(virt_pm_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Runtime PM and system sleep callbacks on a virtual device");
