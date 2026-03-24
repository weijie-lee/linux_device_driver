/*
 * platform_demo.c — Ch06: Platform 平台设备驱动
 *
 * 【知识点】
 * Platform 设备是 Linux 对 SoC 片上外设的统一抽象。这类外设（UART、I2C控制器、
 * GPIO 控制器等）没有自动枚举机制，需要通过设备树（DTS）或代码静态描述资源。
 *
 * 【核心概念】
 * - platform_device：描述硬件资源（寄存器地址、中断号、时钟等）
 * - platform_driver：实现 probe/remove 回调，通过 name 或设备树 compatible 匹配
 * - devm_* 系列：设备管理的资源，在 probe 失败或 remove 时自动释放
 *
 * 【本示例的模拟方案】
 * 由于没有真实硬件，本示例在 module_init 中手动注册一个虚拟 platform_device，
 * 然后注册 platform_driver，内核会自动调用 probe()。
 * 这与真实场景（设备树触发）的驱动代码完全相同，只是设备注册方式不同。
 *
 * 【验证方法】
 * sudo insmod platform_demo.ko
 * ls /sys/bus/platform/devices/ | grep virt_plat   # 查看注册的平台设备
 * ls /sys/bus/platform/drivers/ | grep virt_plat   # 查看注册的平台驱动
 * cat /dev/virt_plat                                # 读取设备
 * echo "test" > /dev/virt_plat                      # 写入设备
 * sudo rmmod platform_demo
 */

#include <linux/module.h>
#include <linux/platform_device.h>  /* platform_driver, platform_device */
#include <linux/miscdevice.h>       /* misc_register（用于暴露用户接口） */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>               /* ioremap, iounmap（真实驱动中使用） */

#define DRIVER_NAME     "virt_plat"
#define BUF_SIZE        128

/* ============================================================
 * 设备私有数据结构
 * 通过 platform_set_drvdata/platform_get_drvdata 与 platform_device 绑定
 * ============================================================ */
struct virt_plat_priv {
	struct platform_device  *pdev;      /* 反向引用所属的 platform_device */
	struct miscdevice        misc;      /* 通过 misc 设备暴露用户接口 */
	char                     buf[BUF_SIZE];
	size_t                   data_len;
};

/* ============================================================
 * 文件操作（用户接口）
 * ============================================================ */
static ssize_t virt_plat_read(struct file *filp, char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	/* 通过 misc 设备的 parent 获取 platform_device，
	 * 再通过 drvdata 获取私有数据 */
	struct miscdevice *misc = filp->private_data;
	struct virt_plat_priv *priv = container_of(misc, struct virt_plat_priv, misc);
	size_t avail;

	avail = (priv->data_len > *ppos) ? (priv->data_len - *ppos) : 0;
	if (!avail)
		return 0;

	count = min(count, avail);
	if (copy_to_user(ubuf, priv->buf + *ppos, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t virt_plat_write(struct file *filp, const char __user *ubuf,
			       size_t count, loff_t *ppos)
{
	struct miscdevice *misc = filp->private_data;
	struct virt_plat_priv *priv = container_of(misc, struct virt_plat_priv, misc);

	count = min(count, (size_t)(BUF_SIZE - 1));
	if (copy_from_user(priv->buf, ubuf, count))
		return -EFAULT;

	priv->buf[count] = '\0';
	priv->data_len = count;
	*ppos = 0;

	dev_info(&priv->pdev->dev, "wrote %zu bytes\n", count);
	return count;
}

static const struct file_operations virt_plat_fops = {
	.owner   = THIS_MODULE,
	.read    = virt_plat_read,
	.write   = virt_plat_write,
	.llseek  = default_llseek,
};

/* ============================================================
 * Platform Driver 核心：probe 和 remove
 * ============================================================ */

/*
 * virt_plat_probe — 设备与驱动匹配成功后由内核调用
 *
 * 真实驱动中，probe 通常执行：
 * 1. platform_get_resource() 获取寄存器地址
 * 2. devm_ioremap_resource() 映射寄存器
 * 3. platform_get_irq() 获取中断号
 * 4. devm_request_irq() 注册中断处理函数
 * 5. 初始化硬件
 * 6. 注册到上层子系统（如 misc/input/net）
 *
 * 本示例跳过硬件操作，直接注册 misc 设备作为用户接口。
 */
static int virt_plat_probe(struct platform_device *pdev)
{
	struct virt_plat_priv *priv;
	int ret;

	dev_info(&pdev->dev, "probe called\n");

	/*
	 * devm_kzalloc：设备管理的内存分配
	 * 当 probe 失败或 remove 被调用时，内核自动调用 kfree()
	 * 无需在错误路径中手动释放
	 */
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pdev = pdev;

	/* 初始化 misc 设备，作为用户接口 */
	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name  = DRIVER_NAME;
	priv->misc.fops  = &virt_plat_fops;

	ret = misc_register(&priv->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register failed: %d\n", ret);
		return ret;
	}

	/*
	 * platform_set_drvdata：将私有数据绑定到 platform_device
	 * 在 remove() 中通过 platform_get_drvdata() 取回
	 */
	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "probed successfully, /dev/%s created\n", DRIVER_NAME);
	return 0;
}

/*
 * virt_plat_remove — 设备移除时由内核调用
 *
 * 注意：devm_* 分配的资源会在此函数返回后自动释放，
 * 无需手动调用 kfree()。
 */
static int virt_plat_remove(struct platform_device *pdev)
{
	struct virt_plat_priv *priv = platform_get_drvdata(pdev);

	misc_deregister(&priv->misc);
	dev_info(&pdev->dev, "removed\n");
	return 0;
}

/* ============================================================
 * Platform Driver 注册结构体
 * ============================================================ */
static struct platform_driver virt_plat_driver = {
	.probe  = virt_plat_probe,
	.remove = virt_plat_remove,
	.driver = {
		.name  = DRIVER_NAME,   /* 与 platform_device.name 匹配 */
		.owner = THIS_MODULE,
		/*
		 * 真实驱动中还需要：
		 * .of_match_table = virt_plat_of_ids,  // 设备树匹配
		 * .pm = &virt_plat_pm_ops,             // 电源管理
		 */
	},
};

/* ============================================================
 * 虚拟 Platform Device（模拟设备树注册）
 *
 * 真实场景中，platform_device 由设备树自动创建，
 * 驱动代码中不需要这部分。
 * ============================================================ */
static struct platform_device *virt_pdev;

static int __init virt_plat_init(void)
{
	int ret;

	/*
	 * 步骤1：注册虚拟 platform_device（模拟设备树）
	 * 真实驱动中此步骤由内核根据设备树自动完成
	 */
	virt_pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!virt_pdev)
		return -ENOMEM;

	ret = platform_device_add(virt_pdev);
	if (ret) {
		platform_device_put(virt_pdev);
		return ret;
	}

	/* 步骤2：注册 platform_driver，内核会自动匹配并调用 probe() */
	ret = platform_driver_register(&virt_plat_driver);
	if (ret) {
		platform_device_unregister(virt_pdev);
		return ret;
	}

	pr_info("%s: module loaded\n", DRIVER_NAME);
	return 0;
}

static void __exit virt_plat_exit(void)
{
	platform_driver_unregister(&virt_plat_driver);
	platform_device_unregister(virt_pdev);
	pr_info("%s: module unloaded\n", DRIVER_NAME);
}

module_init(virt_plat_init);
module_exit(virt_plat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch06: Platform device driver demo");
