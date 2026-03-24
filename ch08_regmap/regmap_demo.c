/*
 * regmap_demo.c — Ch08: Regmap 寄存器访问抽象层
 *
 * 【知识点】
 * Regmap 是 Linux 内核提供的统一寄存器访问框架，屏蔽了底层总线差异
 * （I2C/SPI/MMIO），驱动代码只需调用 regmap_read/regmap_write，
 * 无需关心底层是 i2c_smbus_read_byte_data 还是 spi_sync。
 *
 * 【核心优势】
 * - 统一 API：regmap_read/write/update_bits
 * - 寄存器缓存：REGCACHE_FLAT/RBTREE/MAPLE，减少总线访问
 * - 调试接口：自动在 debugfs 创建寄存器 dump 接口
 * - 位操作：regmap_update_bits 原子地修改寄存器中的特定位
 *
 * 【本示例的模拟方案】
 * 使用 regmap_init() 配合自定义 reg_read/reg_write 回调，
 * 底层用 vmalloc 内存模拟寄存器文件（256个32位寄存器）。
 * 通过 /dev/regmap_demo 暴露 ioctl 接口进行读写测试。
 *
 * 【验证方法】
 * sudo insmod regmap_demo.ko
 * ls /sys/kernel/debug/regmap/                    # 查看 debugfs 接口
 * cat /sys/kernel/debug/regmap/regmap_demo/registers  # dump 所有寄存器
 * sudo rmmod regmap_demo
 */

#include <linux/module.h>
#include <linux/regmap.h>       /* regmap_init, regmap_read, regmap_write */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define DRIVER_NAME     "regmap_demo"
#define REG_COUNT       256     /* 模拟 256 个寄存器 */
#define REG_MAX_ADDR    (REG_COUNT - 1)

/* ioctl 命令定义 */
#define REGMAP_MAGIC        'R'
#define REGMAP_IOC_READ     _IOWR(REGMAP_MAGIC, 0, struct regmap_ioc_arg)
#define REGMAP_IOC_WRITE    _IOW(REGMAP_MAGIC,  1, struct regmap_ioc_arg)
#define REGMAP_IOC_UPDBITS  _IOW(REGMAP_MAGIC,  2, struct regmap_ioc_updbits)

struct regmap_ioc_arg {
	unsigned int reg;   /* 寄存器地址 */
	unsigned int val;   /* 读取的值（REGMAP_IOC_READ）或写入的值（WRITE） */
};

struct regmap_ioc_updbits {
	unsigned int reg;   /* 寄存器地址 */
	unsigned int mask;  /* 要修改的位掩码 */
	unsigned int val;   /* 新值（只有 mask 对应的位有效） */
};

/* ============================================================
 * 设备私有数据
 * ============================================================ */
struct regmap_demo_priv {
	struct regmap       *regmap;
	struct miscdevice    misc;
	u32                  regs[REG_COUNT];   /* 模拟寄存器存储 */
	struct device        dev;               /* 虚拟 device，供 regmap 使用 */
};

static struct regmap_demo_priv *g_priv;

/* ============================================================
 * 自定义 reg_read/reg_write 回调（底层寄存器访问实现）
 * 真实驱动中这里会调用 i2c_smbus_read/spi_sync 等
 * ============================================================ */
static int regmap_demo_reg_read(void *context, unsigned int reg,
				unsigned int *val)
{
	struct regmap_demo_priv *priv = context;

	if (reg > REG_MAX_ADDR)
		return -EINVAL;

	*val = priv->regs[reg];
	return 0;
}

static int regmap_demo_reg_write(void *context, unsigned int reg,
				 unsigned int val)
{
	struct regmap_demo_priv *priv = context;

	if (reg > REG_MAX_ADDR)
		return -EINVAL;

	priv->regs[reg] = val;
	return 0;
}

/* ============================================================
 * Regmap 配置
 * ============================================================ */
static const struct regmap_config regmap_demo_config = {
	.reg_bits       = 8,            /* 寄存器地址宽度：8位（0-255） */
	.val_bits       = 32,           /* 寄存器数据宽度：32位 */
	.max_register   = REG_MAX_ADDR, /* 最大寄存器地址 */
	.reg_read       = regmap_demo_reg_read,
	.reg_write      = regmap_demo_reg_write,
	/*
	 * 寄存器缓存配置：
	 * REGCACHE_NONE  — 不缓存，每次都访问硬件
	 * REGCACHE_FLAT  — 平坦数组缓存（适合寄存器密集的设备）
	 * REGCACHE_RBTREE — 红黑树缓存（适合寄存器稀疏的设备）
	 */
	.cache_type     = REGCACHE_FLAT,
	.name           = DRIVER_NAME,
};

/* ============================================================
 * 文件操作：通过 ioctl 暴露 regmap 读写接口
 * ============================================================ */
static long regmap_demo_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	struct regmap_demo_priv *priv = g_priv;
	struct regmap_ioc_arg rw_arg;
	struct regmap_ioc_updbits upd_arg;
	int ret;

	switch (cmd) {
	case REGMAP_IOC_READ:
		if (copy_from_user(&rw_arg, (void __user *)arg, sizeof(rw_arg)))
			return -EFAULT;
		/*
		 * regmap_read：读取寄存器值
		 * 如果开启了缓存，优先从缓存读取，避免总线访问
		 */
		ret = regmap_read(priv->regmap, rw_arg.reg, &rw_arg.val);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &rw_arg, sizeof(rw_arg)))
			return -EFAULT;
		return 0;

	case REGMAP_IOC_WRITE:
		if (copy_from_user(&rw_arg, (void __user *)arg, sizeof(rw_arg)))
			return -EFAULT;
		/*
		 * regmap_write：写入寄存器值
		 * 同时更新缓存（如果开启了缓存）
		 */
		return regmap_write(priv->regmap, rw_arg.reg, rw_arg.val);

	case REGMAP_IOC_UPDBITS:
		if (copy_from_user(&upd_arg, (void __user *)arg, sizeof(upd_arg)))
			return -EFAULT;
		/*
		 * regmap_update_bits：原子地修改寄存器中的特定位
		 * 等价于：read → (val & ~mask) | (new_val & mask) → write
		 * 但通过 regmap 的锁保护，是线程安全的
		 */
		return regmap_update_bits(priv->regmap, upd_arg.reg,
					  upd_arg.mask, upd_arg.val);

	default:
		return -ENOTTY;
	}
}

static const struct file_operations regmap_demo_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = regmap_demo_ioctl,
};

/* ============================================================
 * 模块初始化与退出
 * ============================================================ */
static int __init regmap_demo_init(void)
{
	struct regmap_demo_priv *priv;
	int ret, i;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* 初始化模拟寄存器：寄存器 N 的初始值为 N * 0x10 */
	for (i = 0; i < REG_COUNT; i++)
		priv->regs[i] = i * 0x10;

	/*
	 * 初始化虚拟 device，供 regmap debugfs 使用
	 * 真实驱动中使用 &i2c_client->dev 或 &spi_device->dev
	 */
	device_initialize(&priv->dev);
	dev_set_name(&priv->dev, DRIVER_NAME);

	/*
	 * regmap_init：初始化 regmap
	 * 参数：device、bus_context（传给 reg_read/write 的 context）、config
	 *
	 * 真实 I2C 驱动使用：regmap_init_i2c(client, &config)
	 * 真实 SPI 驱动使用：regmap_init_spi(spi, &config)
	 */
	priv->regmap = regmap_init(&priv->dev, NULL, priv, &regmap_demo_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		pr_err("%s: regmap_init failed: %d\n", DRIVER_NAME, ret);
		goto err_dev;
	}

	/* 注册 misc 设备 */
	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name  = DRIVER_NAME;
	priv->misc.fops  = &regmap_demo_fops;

	ret = misc_register(&priv->misc);
	if (ret) {
		pr_err("%s: misc_register failed: %d\n", DRIVER_NAME, ret);
		goto err_regmap;
	}

	g_priv = priv;
	pr_info("%s: initialized, %d registers (32-bit each)\n",
		DRIVER_NAME, REG_COUNT);
	pr_info("%s: debugfs: /sys/kernel/debug/regmap/%s/registers\n",
		DRIVER_NAME, DRIVER_NAME);
	return 0;

err_regmap:
	regmap_exit(priv->regmap);
err_dev:
	kfree(priv);
	return ret;
}

static void __exit regmap_demo_exit(void)
{
	misc_deregister(&g_priv->misc);
	regmap_exit(g_priv->regmap);
	kfree(g_priv);
	pr_info("%s: unloaded\n", DRIVER_NAME);
}

module_init(regmap_demo_init);
module_exit(regmap_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch08: Regmap register abstraction layer demo");
